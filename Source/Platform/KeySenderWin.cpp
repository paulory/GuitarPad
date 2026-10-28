#include "KeySender.h"

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

namespace gp
{

KeySender::KeySender()
{
    // timer do Windows a 1 ms, para os toques terem duracao precisa
    timeBeginPeriod (1);
}

KeySender::~KeySender() { timeEndPeriod (1); }

static void sendScan (int keyIndex, bool down)
{
    if (keyIndex < 0 || keyIndex >= kNumKeys)
        return;
    const KeyInfo& k = kKeys[keyIndex];
    INPUT in {};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = k.winScan;
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (k.winExtended ? KEYEVENTF_EXTENDEDKEY : 0) | (down ? 0 : KEYEVENTF_KEYUP);
    SendInput (1, &in, sizeof (INPUT));
}

void KeySender::keyDown (int keyIndex) { sendScan (keyIndex, true); }
void KeySender::keyUp (int keyIndex)   { sendScan (keyIndex, false); }

bool KeySender::isToggleHotkeyDown() { return (GetAsyncKeyState (VK_F12) & 0x8000) != 0; }
bool KeySender::ensurePermission()   { return true; }
const char* KeySender::platformName() { return "Windows"; }

} // namespace gp
