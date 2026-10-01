#pragma once

#include "Platform/IAudioFocus.h"

class WindowsAudioFocus : public IAudioFocus
{
    Q_OBJECT

public:
    explicit WindowsAudioFocus(QObject *parent = nullptr);
    ~WindowsAudioFocus() override;

    bool requestFocus() override;
    void abandonFocus() override;
    bool holdsFocus() const override;
};
