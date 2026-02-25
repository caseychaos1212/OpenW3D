#pragma once

#include <QPoint>
#include <QWidget>

#ifdef _WIN32
#include <windows.h>
#else
using HWND = void *;
#endif

namespace leveledit_qt {

class LevelEditViewport final : public QWidget
{
    Q_OBJECT

public:
    explicit LevelEditViewport(QWidget *parent = nullptr);

    HWND nativeHwnd() const;

signals:
    void interactionStatusChanged(const QString &message);

protected:
    QPaintEngine *paintEngine() const override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    bool _leftDown = false;
    bool _rightDown = false;
    QPoint _lastMousePos;
};

} // namespace leveledit_qt
