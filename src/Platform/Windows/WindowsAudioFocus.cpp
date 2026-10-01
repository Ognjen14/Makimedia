#include "Platform/Windows/WindowsAudioFocus.h"

WindowsAudioFocus::WindowsAudioFocus(QObject *parent)
    : IAudioFocus(parent)
{
}

WindowsAudioFocus::~WindowsAudioFocus() = default;

bool WindowsAudioFocus::requestFocus()
{
    return true;
}

void WindowsAudioFocus::abandonFocus()
{
}

bool WindowsAudioFocus::holdsFocus() const
{
    return true;
}
