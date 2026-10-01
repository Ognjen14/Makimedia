#pragma once

class IScreenLock
{
public:
    virtual ~IScreenLock() = default;

    virtual void setKeepScreenOn(bool keepOn) = 0;
    virtual bool keepScreenOn() const = 0;

    virtual void setKeepSystemAwake(bool awake) = 0;
    virtual bool keepSystemAwake() const = 0;
};
