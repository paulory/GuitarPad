#pragma once
// GuitarPad - detector de nota monofonico para guitarra.
// C++ puro (sem JUCE) para poder ser testado isoladamente.
//
// Fluxo: filtro passa-baixa -> decimacao -> nivel (gate/ataque) -> YIN (altura)
// Saida: eventos NoteOn / NoteOff para um Listener, chamados na thread de audio.

#include <atomic>
#include <cstdint>
#include <vector>

namespace gp
{

struct NoteDetectorConfig
{
    float releaseHystDb   = 6.0f;   // solta a nota quando cai gateDb - isso
    int   minNote         = 38;     // D2 (drop D)
    int   maxNote         = 90;     // F#6
    int   changeHops      = 3;      // leituras para trocar de nota sem nova palhetada (legato/slide)
    int   releaseHops     = 3;      // leituras abaixo do gate para soltar
    float yinThreshold    = 0.15f;
};

class NoteDetector
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void onNoteOn (int midiNote, float levelDb) = 0;
        virtual void onNoteOff (int midiNote) = 0;
    };

    NoteDetector();

    void prepare (double sampleRate);
    void setConfig (const NoteDetectorConfig& c) { config = c; }
    const NoteDetectorConfig& getConfig() const { return config; }
    void setListener (Listener* l) { listener = l; }

    // Ajustes que a interface pode mudar ao vivo (thread-safe)
    void setGateDb (float db)      { gateAtomic.store (db); }
    void setOnsetRiseDb (float db) { riseAtomic.store (db); }
    float getGateDb() const        { return gateAtomic.load(); }
    float getOnsetRiseDb() const   { return riseAtomic.load(); }
    // modo rapido: confirma a nota com 1 leitura em vez de 2 (~5 ms a menos, mais erro)
    void setFastMode (bool fast)   { confirmAtomic.store (fast ? 1 : 2); }

    // Chamado na thread de audio. Sem alocacao.
    void process (const float* samples, int numSamples);
    void reset();

    // Leitura para a interface (qualquer thread)
    float getLevelDb() const        { return levelDbAtomic.load(); }
    float getFrequency() const      { return freqAtomic.load(); }   // 0 = sem altura
    int   getCurrentNote() const    { return noteAtomic.load(); }   // -1 = nenhuma
    int   getLastNoteOn() const     { return lastOnAtomic.load(); }
    uint32_t getNoteOnCounter() const { return onCounter.load(); }

    static double freqToMidi (double f);
    static double midiToFreq (double m);

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void setLowpass (double fs, double fc, double q);
        inline double process (double x)
        {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    enum class State { Idle, Attack, Sounding };

    void analyseHop();
    float runYin (float& confidenceOut); // retorna frequencia ou 0
    void emitOn (int note);
    void emitOff();

    NoteDetectorConfig config;
    Listener* listener = nullptr;

    double fs = 48000.0, fsDec = 12000.0;
    int decim = 4, decimCounter = 0;
    Biquad lp1, lp2;

    // buffer circular do sinal decimado
    std::vector<float> ring;
    int ringPos = 0;
    int hopSize = 64, hopCounter = 0;
    int windowSize = 512, tauMin = 8, tauMax = 180, integration = 330;
    std::vector<float> frame;
    std::vector<double> yinBuf, rawDiff;

    double hopEnergy = 0.0;
    float fastDb = -120.0f, slowDb = -120.0f;

    State state = State::Idle;
    int activeNote = -1;
    int candidateNote = -1, candidateCount = 0;
    int attackHops = 0, belowCount = 0;

    std::atomic<float> gateAtomic { -48.0f };
    std::atomic<float> riseAtomic { 9.0f };
    std::atomic<int> confirmAtomic { 2 };
    std::atomic<float> levelDbAtomic { -120.0f };
    std::atomic<float> freqAtomic { 0.0f };
    std::atomic<int> noteAtomic { -1 };
    std::atomic<int> lastOnAtomic { -1 };
    std::atomic<uint32_t> onCounter { 0 };
};

} // namespace gp
