#pragma once
// Tabela de teclas que o GuitarPad pode simular.
// Windows: scancodes (set 1), que funcionam melhor com jogos do que codigos virtuais.
// macOS: codigos de tecla do Carbon (kVK_*).

#include <cstring>

namespace gp
{

struct KeyInfo
{
    const char* name;       // nome mostrado e salvo no preset
    unsigned short winScan; // scancode
    bool winExtended;       // precisa de KEYEVENTF_EXTENDEDKEY (setas etc.)
    unsigned short macCode;
};

// F12 fica de fora: e o atalho global de liga/desliga sustentacao.
inline const KeyInfo kKeys[] = {
    { "Esquerda", 0x4B, true, 123 }, { "Direita", 0x4D, true, 124 },
    { "Cima", 0x48, true, 126 },     { "Baixo", 0x50, true, 125 },
    { "Espaco", 0x39, false, 49 },   { "Enter", 0x1C, false, 36 },
    { "Esc", 0x01, false, 53 },      { "Tab", 0x0F, false, 48 },
    { "Backspace", 0x0E, false, 51 },
    { "Shift", 0x2A, false, 56 },    { "Ctrl", 0x1D, false, 59 },
    { "Alt", 0x38, false, 58 },
    { "A", 0x1E, false, 0 },  { "B", 0x30, false, 11 }, { "C", 0x2E, false, 8 },
    { "D", 0x20, false, 2 },  { "E", 0x12, false, 14 }, { "F", 0x21, false, 3 },
    { "G", 0x22, false, 5 },  { "H", 0x23, false, 4 },  { "I", 0x17, false, 34 },
    { "J", 0x24, false, 38 }, { "K", 0x25, false, 40 }, { "L", 0x26, false, 37 },
    { "M", 0x32, false, 46 }, { "N", 0x31, false, 45 }, { "O", 0x18, false, 31 },
    { "P", 0x19, false, 35 }, { "Q", 0x10, false, 12 }, { "R", 0x13, false, 15 },
    { "S", 0x1F, false, 1 },  { "T", 0x14, false, 17 }, { "U", 0x16, false, 32 },
    { "V", 0x2F, false, 9 },  { "W", 0x11, false, 13 }, { "X", 0x2D, false, 7 },
    { "Y", 0x15, false, 16 }, { "Z", 0x2C, false, 6 },
    { "1", 0x02, false, 18 }, { "2", 0x03, false, 19 }, { "3", 0x04, false, 20 },
    { "4", 0x05, false, 21 }, { "5", 0x06, false, 23 }, { "6", 0x07, false, 22 },
    { "7", 0x08, false, 26 }, { "8", 0x09, false, 28 }, { "9", 0x0A, false, 25 },
    { "0", 0x0B, false, 29 },
    { "F1", 0x3B, false, 122 }, { "F2", 0x3C, false, 120 }, { "F3", 0x3D, false, 99 },
    { "F4", 0x3E, false, 118 }, { "F5", 0x3F, false, 96 },  { "F6", 0x40, false, 97 },
    { "F7", 0x41, false, 98 },  { "F8", 0x42, false, 100 }, { "F9", 0x43, false, 101 },
    { "F10", 0x44, false, 109 }, { "F11", 0x57, false, 103 },
};

inline constexpr int kNumKeys = (int) (sizeof (kKeys) / sizeof (kKeys[0]));

inline int keyIndexFromName (const char* name)
{
    for (int i = 0; i < kNumKeys; ++i)
        if (std::strcmp (kKeys[i].name, name) == 0)
            return i;
    return -1;
}

// Nome de nota no formato E2, F#3 ...
inline void noteName (int midi, char* out, int outSize)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (midi < 0) { std::strncpy (out, "-", (size_t) outSize); return; }
    const int octave = midi / 12 - 1;
    // snprintf sem incluir <cstdio> no header publico
    const char* n = names[midi % 12];
    int i = 0;
    for (; n[i] && i < outSize - 3; ++i) out[i] = n[i];
    if (octave < 0) { out[i++] = '-'; out[i++] = (char) ('0' - octave); }
    else out[i++] = (char) ('0' + octave);
    out[i] = 0;
}

// Interface de saida: quem implementa manda a tecla para o sistema.
struct KeySink
{
    virtual ~KeySink() = default;
    virtual void keyDown (int keyIndex) = 0;
    virtual void keyUp (int keyIndex) = 0;
};

} // namespace gp
