#include "LevelEditViewport.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QWheelEvent>

namespace leveledit_qt {

LevelEditViewport::LevelEditViewport(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
}

HWND LevelEditViewport::nativeHwnd() const
{
    return reinterpret_cast<HWND>(winId());
}

QPaintEngine *LevelEditViewport::paintEngine() const
{
    return nullptr;
}

void LevelEditViewport::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
}

void LevelEditViewport::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        _leftDown = true;
    } else if (event->button() == Qt::RightButton) {
        _rightDown = true;
    }

    _lastMousePos = event->pos();
    emit interactionStatusChanged(QStringLiteral("Viewport: mouse press"));
    event->accept();
}

void LevelEditViewport::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        _leftDown = false;
    } else if (event->button() == Qt::RightButton) {
        _rightDown = false;
    }

    emit interactionStatusChanged(QStringLiteral("Viewport: mouse release"));
    event->accept();
}

void LevelEditViewport::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint current = event->pos();
    const QPoint delta = current - _lastMousePos;
    _lastMousePos = current;

    if (_leftDown || _rightDown) {
        emit interactionStatusChanged(
            QStringLiteral("Viewport drag dx=%1 dy=%2").arg(delta.x()).arg(delta.y()));
    }

    event->accept();
}

void LevelEditViewport::wheelEvent(QWheelEvent *event)
{
    emit interactionStatusChanged(
        QStringLiteral("Viewport wheel delta=%1").arg(event->angleDelta().y()));
    event->accept();
}

} // namespace leveledit_qt
