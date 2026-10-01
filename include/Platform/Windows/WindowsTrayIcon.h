#pragma once

#include <QObject>
#include <QString>

class WindowsTrayIcon : public QObject
{
    Q_OBJECT

public:
    static bool supported();

    explicit WindowsTrayIcon(QObject *parent = nullptr);
    ~WindowsTrayIcon() override;

    bool visible() const;

    bool showIcon(const QString &tooltip);
    void hideIcon();

    void setTooltip(const QString &tooltip);
    void setStreaming(bool streaming);

    void notify(const QString &title, const QString &text);

    qintptr handleMessage(unsigned int message, quintptr wParam, qintptr lParam,
                          bool *handled);

signals:
    void openRequested();
    void quitRequested();
    void stopStreamingRequested();

private:
    bool ensureWindow();
    bool addIcon();
    void showMenu(int x, int y);

    quintptr m_window = 0;
    quintptr m_icon = 0;
    bool m_sharedIcon = false;
    QString m_tooltip;
    bool m_visible = false;
    bool m_streaming = false;
};
