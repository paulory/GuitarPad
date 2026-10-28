// Testa o MappingEngine de forma deterministica (sem thread, chamando pump com tempo falso).
#include "../Source/Core/MappingEngine.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace gp;

struct Log : KeySink
{
    std::vector<std::string> ev;
    void keyDown (int k) override { ev.push_back (std::string ("+") + kKeys[k].name); }
    void keyUp (int k) override   { ev.push_back (std::string ("-") + kKeys[k].name); }
    std::string str() const { std::string s; for (auto& e : ev) s += e + " "; return s; }
    void clear() { ev.clear(); }
};

static int failures = 0;
static void expect (const Log& log, const std::string& want, const char* what)
{
    const bool ok = log.str() == want;
    std::printf ("%s %s%s%s\n", ok ? "[ok]  " : "[FAIL]", what, ok ? "" : "\n        obtido:   ", ok ? "" : log.str().c_str());
    if (! ok) { std::printf ("        esperado: %s\n", want.c_str()); ++failures; }
}

int main()
{
    const int L = keyIndexFromName ("Esquerda"), R = keyIndexFromName ("Direita"), SP = keyIndexFromName ("Espaco"), X = keyIndexFromName ("X");
    MappingSettings s;
    s.mappings = { { 40, L, KeyMode::Hold }, { 45, R, KeyMode::Latch }, { 55, SP, KeyMode::Tap }, { 57, X, KeyMode::Tap } };
    s.commandNote = 84;
    s.tapMs = 40;

    Log log;
    MappingEngine e (log);
    e.setSettings (s);
    double t = 0;
    auto step = [&] (double ms) { for (double end = t + ms; t < end; t += 1) e.pump (t); };

    // Toque
    e.onNoteOn (55, 0); step (10);
    expect (log, "+Espaco ", "toque aperta na hora");
    step (40);
    expect (log, "+Espaco -Espaco ", "toque solta depois de 40 ms");
    log.clear();

    // Toque repetido rapido: solta, espera folga, reaperta
    e.onNoteOn (55, 0); step (10); e.onNoteOn (55, 0); step (10);
    expect (log, "+Espaco -Espaco ", "toque repetido solta antes de reapertar");
    step (20);
    expect (log, "+Espaco -Espaco +Espaco ", "reaperta depois da folga");
    step (50); log.clear();

    // Sustentacao
    e.onNoteOn (40, 0); step (200);
    expect (log, "+Esquerda ", "sustentacao segura enquanto a nota soa");
    e.onNoteOff (40); step (2);
    expect (log, "+Esquerda -Esquerda ", "sustentacao solta no fim da nota");
    log.clear();

    // Trava
    e.onNoteOn (45, 0); step (5); e.onNoteOff (45); step (300);
    expect (log, "+Direita ", "trava continua apertada apos a nota acabar");
    e.onNoteOn (45, 0); step (5);
    expect (log, "+Direita -Direita ", "trava solta no segundo toque");
    e.onNoteOff (45); step (5); log.clear();

    // Desligar sustentacao enquanto segura: solta tudo
    e.onNoteOn (40, 0); e.onNoteOn (45, 0); step (5);
    e.setHoldEnabled (false); step (2);
    expect (log, "+Esquerda +Direita -Esquerda -Direita ", "desligar sustentacao solta holds e travas");
    e.onNoteOff (40); step (2); log.clear();

    // Com sustentacao desligada, tudo vira toque
    e.onNoteOn (40, 0); step (100);
    expect (log, "+Esquerda -Esquerda ", "sustentacao desligada: nota de hold vira toque");
    e.onNoteOff (40); step (2); log.clear();
    e.onNoteOn (45, 0); step (100); e.onNoteOff (45);
    expect (log, "+Direita -Direita ", "sustentacao desligada: trava vira toque");
    step (2); log.clear();

    // Nota de comando liga de volta
    e.onNoteOn (84, 0); step (2);
    expect (log, "", "nota de comando nao manda tecla");
    std::printf ("%s nota de comando religou a sustentacao\n", e.isHoldEnabled() ? "[ok]  " : "[FAIL]");
    if (! e.isHoldEnabled()) ++failures;
    e.onNoteOff (84); step (2);
    e.onNoteOn (40, 0); step (100);
    expect (log, "+Esquerda ", "depois de religar, hold volta a segurar");
    e.onNoteOff (40); step (2); log.clear();

    // Duas notas na mesma tecla: so solta quando as duas acabam
    {
        MappingSettings s2 = s;
        s2.mappings.push_back ({ 52, L, KeyMode::Hold });
        e.setSettings (s2);
        e.onNoteOn (40, 0); e.onNoteOn (52, 0); step (5); e.onNoteOff (40); step (5);
        expect (log, "+Esquerda ", "duas fontes na mesma tecla: continua apertada");
        e.onNoteOff (52); step (2);
        expect (log, "+Esquerda -Esquerda ", "solta quando a ultima fonte acaba");
        e.setSettings (s); log.clear();
    }

    // Pausa geral
    e.onNoteOn (40, 0); step (5);
    e.setOutputEnabled (false); step (2);
    e.onNoteOn (55, 0); step (100);
    expect (log, "+Esquerda -Esquerda ", "pausa solta tudo e ignora notas");
    e.setOutputEnabled (true); step (2); e.onNoteOff (40); e.onNoteOff (55); step (2); log.clear();

    // Panico
    e.onNoteOn (45, 0); step (5); e.panic(); step (2);
    expect (log, "+Direita -Direita ", "panico solta tudo");
    e.onNoteOff (45); step (2); log.clear();

    // Nota sem mapeamento
    e.onNoteOn (70, 0); step (100); e.onNoteOff (70); step (2);
    expect (log, "", "nota sem mapeamento nao faz nada");

    // Thread real: start/stop solta tudo
    {
        Log log2;
        MappingEngine e2 (log2);
        e2.setSettings (s);
        e2.start();
        e2.onNoteOn (40, 0);
        std::this_thread::sleep_for (std::chrono::milliseconds (30));
        e2.stop();
        expect (log2, "+Esquerda -Esquerda ", "thread real: stop solta teclas presas");
    }

    std::printf ("\n%s (%d falhas)\n", failures ? "FALHOU" : "TUDO OK", failures);
    return failures ? 1 : 0;
}
