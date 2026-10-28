#include "KeySender.h"

// Sistemas sem suporte: compila, mas nao manda teclas.
namespace gp
{
KeySender::KeySender() {}
KeySender::~KeySender() {}
void KeySender::keyDown (int) {}
void KeySender::keyUp (int) {}
bool KeySender::isToggleHotkeyDown() { return false; }
bool KeySender::ensurePermission() { return true; }
const char* KeySender::platformName() { return "sem suporte de teclado"; }
} // namespace gp
