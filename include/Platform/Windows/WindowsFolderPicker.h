#pragma once

#include "Platform/IFolderPicker.h"

class WindowsFolderPicker : public IFolderPicker
{
    Q_OBJECT

public:
    explicit WindowsFolderPicker(QObject *parent = nullptr);
    ~WindowsFolderPicker() override;

    void requestFolder() override;
};
