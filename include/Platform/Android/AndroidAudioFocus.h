#pragma once

#include "Platform/IAudioFocus.h"

class AndroidAudioFocus : public IAudioFocus
{
    Q_OBJECT

public:
    explicit AndroidAudioFocus(QObject *parent = nullptr);
    ~AndroidAudioFocus() override;

    bool requestFocus() override;
    void abandonFocus() override;
    bool holdsFocus() const override;

    void onFocusChanged(int change);
    void onBecomingNoisy();
};
