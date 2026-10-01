#pragma once

#include <QRect>

#include <qglobal.h>

class WindowsMiniPlayer
{
public:
    static bool supported();

    static bool enter(quintptr windowHandle, int videoWidth, int videoHeight);
    static bool leave(quintptr windowHandle);

    static bool active();
};
