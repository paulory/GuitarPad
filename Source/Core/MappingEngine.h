#pragma once
// GuitarPad - transforma eventos de nota em teclas.
//
// Modos por nota:
//   Toque        aperta e solta rapido (pular, atacar)
//   Sustentacao  segura a tecla enquanto a nota soa (andar, correr)
//   Trava        1a vez aperta, 2a vez solta (andar sem precisar sustentar)
//
// Com a sustentacao global DESLIGADA, todas as notas viram Toque.
// Tudo que mexe em tecla roda numa thread propria; a thread de audio so enfileira.

#include "Keys.h"
#include "NoteDetector.h"

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace gp
{

enum class KeyMode { Tap = 0, Hold = 1, Latch = 2 };

struct Mapping
{
    int note = 40;
    int key = 0;
    KeyMode mode = KeyMode::Tap;
};

struct MappingSettings
{
    std::vector<Mapping> mappings;
    int commandNote = -1; // nota que liga/desliga a sustentacao (-1 = nenhuma)
    int tapMs = 40;       // quanto tempo o Toque segura a tecla
};

class MappingEngine : public NoteDetector::Listener
{
public:
    explicit MappingEngine (KeySink& sink);
    ~MappingEngine() override;

    void start(); // inicia a thread de saida
    void stop();  // para e solta todas as teclas

    void setSettings (const MappingSettings& s);
    MappingSettings getSettings() const;

    // NoteDetector::Listener (thread de audio)
    void onNoteOn (int note, float levelDb) override;
    void onNoteOff (int note) override;

    // Interface (qualquer thread)
    void setHoldEnabled (bool on);
    void toggleHold();
    bool isHoldEnabled() const { return holdEnabled.load(); }
    void setOutputEnabled (bool on); // pausa geral: desligado nao manda nada
    bool isOutputEnabled() const { return outputEnabled.load(); }
    void panic();                    // solta tudo
    bool isKeyDown (int key) const { return key >= 0 && key < kNumKeys && keyIsDown[(size_t) key].load(); }
    uint32_t getHoldToggleCounter() const { return holdToggles.load(); }

    // Processa filas e timers. Normalmente chamado pela thread interna;
    // publico para testes deterministicos.
    void pump (double nowMs);
    static double nowMs();

private:
    struct Event { enum Type : uint8_t { On, Off } type; int16_t note; };
    struct Command { enum Type { HoldOn, HoldOff, HoldToggle, OutputOn, OutputOff, Panic } type; };

    // fila lock-free (1 produtor = audio, 1 consumidor = thread de saida)
    static constexpr size_t kQueueSize = 512;
    std::array<Event, kQueueSize> queue {};
    std::atomic<size_t> qHead { 0 }, qTail { 0 };
    void push (Event e);

    std::mutex cmdMutex;
    std::deque<Command> commands;
    void postCommand (Command::Type t);

    void handleNoteOn (int note, double now);
    void handleNoteOff (int note);
    void press (int key);
    void release (int key);
    void releaseHoldsAndLatches();
    void releaseAll();
    void applyHold (bool on);

    KeySink& sink;
    mutable std::mutex settingsMutex;
    MappingSettings settings;

    std::atomic<bool> holdEnabled { true }, outputEnabled { true }, running { false };
    std::atomic<uint32_t> holdToggles { 0 };
    std::array<std::atomic<bool>, (size_t) kNumKeys> keyIsDown {};

    // estado da thread de saida
    std::array<int, (size_t) kNumKeys> refCount {};
    struct TapRelease { int key; double at; };
    std::vector<TapRelease> taps;
    struct Repress { int key; double at; double tapMs; };
    std::vector<Repress> repress;
    std::array<int, 128> holdKeyForNote {};   // tecla segurada por nota (-1 = nenhuma)
    std::array<bool, (size_t) kNumKeys> latched {};

    std::thread worker;
};

} // namespace gp
