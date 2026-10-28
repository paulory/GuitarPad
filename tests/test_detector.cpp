// Testa o detector com cordas sinteticas (Karplus-Strong), ruido e palhetadas repetidas.
#include "../Source/Core/NoteDetector.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace gp;

struct Ev { bool on; int note; double t; };

struct Recorder : NoteDetector::Listener
{
    std::vector<Ev> evs;
    double now = 0.0;
    void onNoteOn (int n, float) override  { evs.push_back ({ true, n, now }); }
    void onNoteOff (int n) override        { evs.push_back ({ false, n, now }); }
};

static std::mt19937 rng (42);

// adiciona uma corda pinçada em buf a partir de start
static void pluck (std::vector<float>& buf, double fs, size_t start, double freq, double dur, double amp, double decay = 0.996)
{
    std::uniform_real_distribution<float> u (-1.f, 1.f);
    int N = (int) std::lround (fs / freq + 0.5); // KS desta forma tem periodo N - 0.5
    std::vector<float> line ((size_t) N);
    for (auto& v : line) v = u (rng);
    size_t len = (size_t) (dur * fs);
    int p = 0;
    for (size_t i = 0; i < len && start + i < buf.size(); ++i)
    {
        int q = (p + 1) % N;
        float out = line[(size_t) p];
        line[(size_t) p] = (float) (decay * 0.5 * (line[(size_t) p] + line[(size_t) q]));
        p = q;
        // fade out no fim (abafar)
        double env = i > len - (size_t) (0.01 * fs) ? (double) (len - i) / (0.01 * fs) : 1.0;
        buf[start + i] += (float) (amp * out * env);
    }
}

static Recorder run (std::vector<float>& buf, double fs, float noiseAmp = 0.0005f)
{
    std::normal_distribution<float> n (0.f, 1.f);
    for (auto& v : buf) v += noiseAmp * n (rng);
    NoteDetector d;
    d.prepare (fs);
    Recorder r;
    d.setListener (&r);
    const int block = 128;
    for (size_t i = 0; i < buf.size(); i += block)
    {
        r.now = (double) i / fs;
        d.process (buf.data() + i, (int) std::min<size_t> (block, buf.size() - i));
    }
    return r;
}

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("%s %s\n", ok ? "[ok]  " : "[FAIL]", what);
    if (! ok) ++failures;
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("--- fs = %.0f\n", fs);
        // 1) notas isoladas de E2 a E6, cada uma com silencio entre
        int notes[] = { 40, 45, 50, 55, 59, 64, 69, 76, 84, 88 };
        int correct = 0, total = 0;
        double worstLatency = 0, sumLat = 0;
        for (int n : notes)
        {
            std::vector<float> buf ((size_t) (fs * 1.0), 0.f);
            size_t start = (size_t) (0.2 * fs);
            pluck (buf, fs, start, NoteDetector::midiToFreq (n), 0.5, 0.3);
            auto r = run (buf, fs);
            ++total;
            int ons = 0, offs = 0; bool right = false; double lat = 0;
            for (auto& e : r.evs)
            {
                if (e.on) { ++ons; if (e.note == n && ons == 1) { right = true; lat = e.t - 0.2; } }
                else ++offs;
            }
            if (right && ons == 1 && offs == 1) ++correct;
            else std::printf ("   nota %d: ons=%d offs=%d primeira=%d\n", n, ons, offs, r.evs.empty() ? -1 : r.evs[0].note);
            worstLatency = std::max (worstLatency, lat);
            sumLat += lat;
            std::printf ("   nota %d latencia %.1f ms\n", n, lat * 1000);
        }
        char msg[128];
        std::snprintf (msg, sizeof msg, "notas isoladas corretas %d/%d (latencia media %.1f ms, pior %.1f ms)", correct, total, sumLat / total * 1000, worstLatency * 1000);
        check (correct == total, msg);

        // 2) mesma nota palhetada 4 vezes seguidas sem silencio (sustain encavalado)
        {
            std::vector<float> buf ((size_t) (fs * 2.0), 0.f);
            for (int k = 0; k < 4; ++k)
            {
                // abafa a anterior parcialmente: nova palhetada mais forte
                pluck (buf, fs, (size_t) ((0.2 + 0.35 * k) * fs), NoteDetector::midiToFreq (52), 0.34, 0.3);
            }
            auto r = run (buf, fs);
            int ons = 0; for (auto& e : r.evs) if (e.on && e.note == 52) ++ons;
            std::snprintf (msg, sizeof msg, "4 palhetadas repetidas -> %d NoteOn", ons);
            check (ons == 4, msg);
        }

        // 3) sequencia de notas diferentes (legato, sem pausa)
        {
            std::vector<float> buf ((size_t) (fs * 2.0), 0.f);
            int seq[] = { 40, 47, 52, 55 };
            for (int k = 0; k < 4; ++k)
                pluck (buf, fs, (size_t) ((0.2 + 0.3 * k) * fs), NoteDetector::midiToFreq (seq[k]), 0.3, 0.3);
            auto r = run (buf, fs);
            std::vector<int> got; for (auto& e : r.evs) if (e.on) got.push_back (e.note);
            bool ok = got.size() == 4;
            for (size_t i = 0; ok && i < 4; ++i) ok = got[i] == seq[i];
            std::snprintf (msg, sizeof msg, "sequencia E2 B2 E3 G3 -> %zu notas", got.size());
            check (ok, msg);
            if (! ok) { for (int g : got) std::printf (" %d", g); std::printf ("\n"); }
        }

        // 4) so ruido: nenhuma nota
        {
            std::vector<float> buf ((size_t) (fs * 1.0), 0.f);
            auto r = run (buf, fs, 0.002f);
            check (r.evs.empty(), "ruido de fundo nao dispara nada");
        }

        // 5) nota longa: NoteOn e NoteOff pareados, sem retrigger falso
        {
            std::vector<float> buf ((size_t) (fs * 3.0), 0.f);
            pluck (buf, fs, (size_t) (0.2 * fs), NoteDetector::midiToFreq (45), 2.5, 0.3, 0.9995);
            auto r = run (buf, fs);
            int ons = 0, offs = 0; for (auto& e : r.evs) (e.on ? ons : offs)++;
            std::snprintf (msg, sizeof msg, "nota longa (2.5 s): %d on / %d off", ons, offs);
            check (ons == 1 && offs == 1, msg);
        }
    }
    std::printf ("\n%s (%d falhas)\n", failures ? "FALHOU" : "TUDO OK", failures);
    return failures ? 1 : 0;
}
