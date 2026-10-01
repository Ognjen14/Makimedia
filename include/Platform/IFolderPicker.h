#pragma once

#include <QObject>
#include <QString>

class IFolderPicker : public QObject
{
    Q_OBJECT

public:
    explicit IFolderPicker(QObject *parent = nullptr) : QObject(parent) {}
    ~IFolderPicker() override = default;

    Q_INVOKABLE virtual void requestFolder() = 0;

signals:
    void folderPicked(const QString &rootHandle, const QString &displayName);
    void folderPickCancelled();
    void folderPickFailed(const QString &reason);
};
