#include "MappingEngine.h"

#include <algorithm>

namespace gp
{

static constexpr double kRepressGapMs = 20.0; // folga para o jogo ver a tecla solta antes de apertar de novo

double MappingEngine::nowMs()
{
    using namespace std::chrono;
    return duration<double, std::milli> (steady_clock::now().time_since_epoch()).count();
}

MappingEngine::MappingEngine (KeySink& s) : sink (s)
{
    holdKeyForNote.fill (-1);
    latched.fill (false);
    refCount.fill (0);
    for (auto& k : keyIsDown) k.store (false);
}

MappingEngine::~MappingEngine() { stop(); }

void MappingEngine::start()
{
    if (running.exchange (true))
        return;
    worker = std::thread ([this]
    {
        while (running.load())
        {
            pump (nowMs());
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
        releaseAll();
    });
}

void MappingEngine::stop()
{
    if (running.exchange (false) && worker.joinable())
        worker.join();
}

void MappingEngine::setSettings (const MappingSettings& s)
{
    std::lock_guard<std::mutex> lock (settingsMutex);
    settings = s;
    settings.tapMs = std::clamp (settings.tapMs, 10, 500);
}

MappingSettings MappingEngine::getSettings() const
{
    std::lock_guard<std::mutex> lock (settingsMutex);
    return settings;
}

// ---------- fila de eventos (thread de audio) ----------

void MappingEngine::push (Event e)
{
    const size_t head = qHead.load (std::memory_order_relaxed);
    const size_t next = (head + 1) % kQueueSize;
    if (next == qTail.load (std::memory_order_acquire))
        return; // fila cheia: descarta
    queue[head] = e;
    qHead.store (next, std::memory_order_release);
}

void MappingEngine::onNoteOn (int note, float) { push ({ Event::On, (int16_t) note }); }
void MappingEngine::onNoteOff (int note)       { push ({ Event::Off, (int16_t) note }); }

// ---------- comandos da interface ----------

void MappingEngine::postCommand (Command::Type t)
{
    std::lock_guard<std::mutex> lock (cmdMutex);
    commands.push_back ({ t });
}

void MappingEngine::setHoldEnabled (bool on)   { postCommand (on ? Command::HoldOn : Command::HoldOff); }
void MappingEngine::toggleHold()               { postCommand (Command::HoldToggle); }
void MappingEngine::setOutputEnabled (bool on) { postCommand (on ? Command::OutputOn : Command::OutputOff); }
void MappingEngine::panic()                    { postCommand (Command::Panic); }

// ---------- thread de saida ----------

void MappingEngine::pump (double now)
{
    // 1) comandos
    {
        std::deque<Command> local;
        {
            std::lock_guard<std::mutex> lock (cmdMutex);
            local.swap (commands);
        }
        for (auto& c : local)
        {
            switch (c.type)
            {
                case Command::HoldOn:     applyHold (true); break;
                case Command::HoldOff:    applyHold (false); break;
                case Command::HoldToggle: applyHold (! holdEnabled.load()); break;
                case Command::OutputOn:   outputEnabled.store (true); break;
                case Command::OutputOff:  outputEnabled.store (false); releaseAll(); break;
                case Command::Panic:      releaseAll(); break;
            }
        }
    }

    // 2) eventos de nota
    for (;;)
    {
        const size_t tail = qTail.load (std::memory_order_relaxed);
        if (tail == qHead.load (std::memory_order_acquire))
            break;
        const Event e = queue[tail];
        qTail.store ((tail + 1) % kQueueSize, std::memory_order_release);
        if (e.type == Event::On) handleNoteOn (e.note, now);
        else handleNoteOff (e.note);
    }

    // 3) timers de toque (soltar e reapertar)
    for (size_t i = 0; i < taps.size();)
    {
        if (taps[i].at <= now)
        {
            const int key = taps[i].key;
            taps.erase (taps.begin() + (long) i);
            release (key);
        }
        else
            ++i;
    }
    for (size_t i = 0; i < repress.size();)
    {
        if (repress[i].at <= now)
        {
            const int key = repress[i].key;
            const double tapMs = repress[i].tapMs;
            repress.erase (repress.begin() + (long) i);
            press (key);
            taps.push_back ({ key, now + tapMs });
        }
        else
            ++i;
    }
}

void MappingEngine::applyHold (bool on)
{
    if (holdEnabled.load() != on)
        holdToggles.fetch_add (1);
    holdEnabled.store (on);
    if (! on)
        releaseHoldsAndLatches();
}

void MappingEngine::handleNoteOn (int note, double now)
{
    if (note < 0 || note > 127)
        return;

    Mapping found;
    bool has = false;
    int commandNote, tapMs;
    {
        std::lock_guard<std::mutex> lock (settingsMutex);
        commandNote = settings.commandNote;
        tapMs = settings.tapMs;
        for (auto& m : settings.mappings)
            if (m.note == note && m.key >= 0 && m.key < kNumKeys) { found = m; has = true; break; }
    }

    if (note == commandNote)
    {
        applyHold (! holdEnabled.load());
        return;
    }
    if (! has || ! outputEnabled.load())
        return;

    const KeyMode mode = holdEnabled.load() ? found.mode : KeyMode::Tap;
    const int key = found.key;

    switch (mode)
    {
        case KeyMode::Tap:
        {
            // se essa tecla ja esta num toque, solta e reaperta depois de uma folga
            auto it = std::find_if (taps.begin(), taps.end(), [key] (const TapRelease& t) { return t.key == key; });
            if (it != taps.end())
            {
                taps.erase (it);
                release (key);
                repress.push_back ({ key, now + kRepressGapMs, (double) tapMs });
            }
            else if (std::any_of (repress.begin(), repress.end(), [key] (const Repress& r) { return r.key == key; }))
            {
                // ja tem um reaperto agendado: ignora
            }
            else
            {
                press (key);
                taps.push_back ({ key, now + tapMs });
            }
            break;
        }
        case KeyMode::Hold:
            if (holdKeyForNote[(size_t) note] < 0)
            {
                holdKeyForNote[(size_t) note] = key;
                press (key);
            }
            break;

        case KeyMode::Latch:
            if (latched[(size_t) key]) { latched[(size_t) key] = false; release (key); }
            else                       { latched[(size_t) key] = true;  press (key); }
            break;
    }
}

void MappingEngine::handleNoteOff (int note)
{
    if (note < 0 || note > 127)
        return;
    const int key = holdKeyForNote[(size_t) note];
    if (key >= 0)
    {
        holdKeyForNote[(size_t) note] = -1;
        release (key);
    }
}

void MappingEngine::press (int key)
{
    if (refCount[(size_t) key]++ == 0)
    {
        keyIsDown[(size_t) key].store (true);
        sink.keyDown (key);
    }
}

void MappingEngine::release (int key)
{
    if (refCount[(size_t) key] <= 0)
        return;
    if (--refCount[(size_t) key] == 0)
    {
        keyIsDown[(size_t) key].store (false);
        sink.keyUp (key);
    }
}

void MappingEngine::releaseHoldsAndLatches()
{
    for (auto& k : holdKeyForNote)
        if (k >= 0) { release (k); k = -1; }
    for (int k = 0; k < kNumKeys; ++k)
        if (latched[(size_t) k]) { latched[(size_t) k] = false; release (k); }
}

void MappingEngine::releaseAll()
{
    taps.clear();
    repress.clear();
    holdKeyForNote.fill (-1);
    latched.fill (false);
    for (int k = 0; k < kNumKeys; ++k)
    {
        if (refCount[(size_t) k] > 0)
        {
            refCount[(size_t) k] = 0;
            keyIsDown[(size_t) k].store (false);
            sink.keyUp (k);
        }
    }
}

} // namespace gp
