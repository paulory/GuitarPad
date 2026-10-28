#include "NoteDetector.h"

#include <algorithm>
#include <cmath>

namespace gp
{

static constexpr double kPi = 3.14159265358979323846;

double NoteDetector::freqToMidi (double f) { return 69.0 + 12.0 * std::log2 (f / 440.0); }
double NoteDetector::midiToFreq (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

void NoteDetector::Biquad::setLowpass (double sr, double fc, double q)
{
    const double w0 = 2.0 * kPi * fc / sr;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / (2.0 * q);
    const double a0 = 1.0 + alpha;
    b0 = (1.0 - cw) * 0.5 / a0;
    b1 = (1.0 - cw) / a0;
    b2 = b0;
    a1 = -2.0 * cw / a0;
    a2 = (1.0 - alpha) / a0;
    z1 = z2 = 0.0;
}

NoteDetector::NoteDetector() { prepare (48000.0); }

void NoteDetector::prepare (double sampleRate)
{
    fs = sampleRate > 0 ? sampleRate : 48000.0;
    decim = std::max (1, (int) std::lround (fs / 24000.0));
    fsDec = fs / decim;

    const double fc = std::min (2500.0, 0.4 * fsDec);
    lp1.setLowpass (fs, fc, 0.5412); // Butterworth 4a ordem (2 biquads)
    lp2.setLowpass (fs, fc, 1.3066);

    const double fMin = midiToFreq (config.minNote - 1);
    const double fMax = midiToFreq (config.maxNote + 1);
    tauMax = (int) std::ceil (fsDec / fMin) + 2;
    tauMin = std::max (2, (int) std::floor (fsDec / fMax));
    integration = std::max (2 * tauMax, 256);
    windowSize = integration + tauMax + 2;

    ring.assign ((size_t) windowSize, 0.0f);
    frame.assign ((size_t) windowSize, 0.0f);
    yinBuf.assign ((size_t) tauMax + 2, 0.0);
    rawDiff.assign ((size_t) tauMax + 2, 0.0);
    hopSize = std::max (32, (int) std::lround (fsDec * 0.0053)); // ~5 ms
    reset();
}

void NoteDetector::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    ringPos = 0;
    hopCounter = 0;
    decimCounter = 0;
    hopEnergy = 0.0;
    fastDb = slowDb = -120.0f;
    state = State::Idle;
    activeNote = -1;
    candidateNote = -1;
    candidateCount = 0;
    attackHops = belowCount = 0;
    noteAtomic.store (-1);
    freqAtomic.store (0.0f);
}

void NoteDetector::process (const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const double y = lp2.process (lp1.process ((double) samples[i]));
        if (++decimCounter < decim)
            continue;
        decimCounter = 0;

        ring[(size_t) ringPos] = (float) y;
        ringPos = (ringPos + 1) % windowSize;
        hopEnergy += y * y;

        if (++hopCounter >= hopSize)
        {
            analyseHop();
            hopCounter = 0;
            hopEnergy = 0.0;
        }
    }
}

float NoteDetector::runYin (float& confidenceOut)
{
    // copia a janela em ordem cronologica
    for (int i = 0; i < windowSize; ++i)
        frame[(size_t) i] = ring[(size_t) ((ringPos + i) % windowSize)];

    const float* x = frame.data() + (windowSize - integration - tauMax);

    // funcao diferenca
    yinBuf[0] = 1.0;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        for (int j = 0; j < integration; ++j)
        {
            const double d = (double) x[j] - (double) x[j + tau];
            sum += d * d;
        }
        running += sum;
        rawDiff[(size_t) tau] = sum;
        // diferenca normalizada pela media acumulada
        yinBuf[(size_t) tau] = running > 0.0 ? sum * tau / running : 1.0;
    }

    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (yinBuf[(size_t) tau] < config.yinThreshold)
        {
            while (tau + 1 < tauMax && yinBuf[(size_t) tau + 1] < yinBuf[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }

    if (best < 0)
    {
        confidenceOut = 0.0f;
        return 0.0f;
    }

    // interpolacao parabolica sobre a diferenca bruta (menos vies em notas agudas)
    while (best + 1 < tauMax && rawDiff[(size_t) best + 1] < rawDiff[(size_t) best]) ++best;
    while (best - 1 > tauMin && rawDiff[(size_t) best - 1] < rawDiff[(size_t) best]) --best;
    double t = best;
    if (best > 1 && best < tauMax)
    {
        const double s0 = rawDiff[(size_t) best - 1], s1 = rawDiff[(size_t) best], s2 = rawDiff[(size_t) best + 1];
        const double den = s0 - 2.0 * s1 + s2;
        if (std::abs (den) > 1e-12)
            t = best + 0.5 * (s0 - s2) / den;
    }

    confidenceOut = (float) (1.0 - yinBuf[(size_t) best]);
    return (float) (fsDec / t);
}

void NoteDetector::emitOn (int note)
{
    activeNote = note;
    noteAtomic.store (note);
    lastOnAtomic.store (note);
    onCounter.fetch_add (1);
    if (listener != nullptr)
        listener->onNoteOn (note, fastDb);
}

void NoteDetector::emitOff()
{
    if (activeNote >= 0 && listener != nullptr)
        listener->onNoteOff (activeNote);
    activeNote = -1;
    noteAtomic.store (-1);
}

void NoteDetector::analyseHop()
{
    const double meanSq = hopEnergy / hopSize;
    fastDb = meanSq > 1e-12 ? (float) (10.0 * std::log10 (meanSq)) : -120.0f;
    levelDbAtomic.store (fastDb);

    const float gate = gateAtomic.load();
    const bool aboveGate = fastDb > gate;
    const bool belowRelease = fastDb < gate - config.releaseHystDb;
    const bool rise = fastDb - slowDb > riseAtomic.load();

    // media lenta do nivel (~30 ms), usada para detectar nova palhetada
    slowDb = fastDb > slowDb ? slowDb + 0.35f * (fastDb - slowDb)
                             : slowDb + 0.15f * (fastDb - slowDb);

    // altura so e calculada quando ha sinal
    float freq = 0.0f, conf = 0.0f;
    int pitchNote = -1;
    double pitchMidi = 0.0;
    if (! belowRelease)
    {
        freq = runYin (conf);
        if (freq > 0.0f)
        {
            pitchMidi = freqToMidi (freq);
            const int n = (int) std::lround (pitchMidi);
            if (n >= config.minNote && n <= config.maxNote)
                pitchNote = n;
        }
    }
    freqAtomic.store (pitchNote >= 0 ? freq : 0.0f);

    auto trackCandidate = [this] (int n)
    {
        if (n == candidateNote) ++candidateCount;
        else { candidateNote = n; candidateCount = 1; }
    };

    switch (state)
    {
        case State::Idle:
            if (aboveGate)
            {
                state = State::Attack;
                attackHops = 0;
                candidateNote = -1;
                candidateCount = 0;
                belowCount = 0;
                if (pitchNote >= 0) trackCandidate (pitchNote);
            }
            break;

        case State::Attack:
            ++attackHops;
            if (belowRelease)
            {
                state = State::Idle;
                break;
            }
            if (pitchNote >= 0)
            {
                trackCandidate (pitchNote);
                if (candidateCount >= confirmAtomic.load())
                {
                    emitOn (candidateNote);
                    state = State::Sounding;
                    candidateNote = -1;
                    candidateCount = 0;
                    break;
                }
            }
            if (attackHops > 24) // ~130 ms sem altura definida: segue sem nota
            {
                state = State::Sounding;
                candidateNote = -1;
                candidateCount = 0;
            }
            break;

        case State::Sounding:
            if (belowRelease)
            {
                if (++belowCount >= config.releaseHops)
                {
                    emitOff();
                    state = State::Idle;
                }
                break;
            }
            belowCount = 0;

            if (rise && aboveGate)
            {
                // nova palhetada: solta a nota atual e confirma a proxima
                emitOff();
                state = State::Attack;
                attackHops = 0;
                candidateNote = -1;
                candidateCount = 0;
                if (pitchNote >= 0) trackCandidate (pitchNote);
                break;
            }

            if (pitchNote >= 0)
            {
                const bool sameNote = activeNote >= 0 && std::abs (pitchMidi - activeNote) < 0.75;
                if (sameNote)
                {
                    candidateNote = -1;
                    candidateCount = 0;
                }
                else
                {
                    trackCandidate (pitchNote);
                    if (candidateCount >= config.changeHops)
                    {
                        emitOff();
                        emitOn (candidateNote);
                        candidateNote = -1;
                        candidateCount = 0;
                    }
                }
            }
            break;
    }
}

} // namespace gp
