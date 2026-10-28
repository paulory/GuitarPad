#pragma once
// Saida de teclado por sistema operacional.
//   Windows: SendInput com scancode (funciona com a maioria dos jogos)
//   macOS:   CGEventPost (precisa de permissao de Acessibilidade)
//   outros:  nao faz nada (so para compilar)

#include "../Core/Keys.h"

namespace gp
{

class KeySender : public KeySink
{
public:
    KeySender();
    ~KeySender() override;

    void keyDown (int keyIndex) override;
    void keyUp (int keyIndex) override;

    // Atalho global de liga/desliga sustentacao (F12), lido mesmo com o jogo em foco.
    static bool isToggleHotkeyDown();

    // macOS: pede a permissao de Acessibilidade se ainda nao tiver. Retorna se ja tem.
    static bool ensurePermission();

    static const char* platformName();
};

} // namespace gp
