#include "KeySender.h"

#import <ApplicationServices/ApplicationServices.h>

namespace gp
{

KeySender::KeySender() {}
KeySender::~KeySender() {}

static void sendKey (int keyIndex, bool down)
{
    if (keyIndex < 0 || keyIndex >= kNumKeys)
        return;
    CGEventRef ev = CGEventCreateKeyboardEvent (nullptr, (CGKeyCode) kKeys[keyIndex].macCode, down);
    if (ev != nullptr)
    {
        CGEventPost (kCGHIDEventTap, ev);
        CFRelease (ev);
    }
}

void KeySender::keyDown (int keyIndex) { sendKey (keyIndex, true); }
void KeySender::keyUp (int keyIndex)   { sendKey (keyIndex, false); }

bool KeySender::isToggleHotkeyDown()
{
    return CGEventSourceKeyState (kCGEventSourceStateHIDSystemState, (CGKeyCode) 111 /* kVK_F12 */);
}

bool KeySender::ensurePermission()
{
    const void* keys[] = { kAXTrustedCheckOptionPrompt };
    const void* values[] = { kCFBooleanTrue };
    CFDictionaryRef opts = CFDictionaryCreate (nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const bool trusted = AXIsProcessTrustedWithOptions (opts);
    CFRelease (opts);
    return trusted;
}

const char* KeySender::platformName() { return "macOS"; }

} // namespace gp
