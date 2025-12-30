#include "W3DViewport.h"

#include "ww3d.h"

#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>

W3DViewport::W3DViewport(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);

    _timer.setInterval(16);
    connect(&_timer, &QTimer::timeout, this, &W3DViewport::renderFrame);
}

W3DViewport::~W3DViewport()
{
    shutdownWW3D();
}

QPaintEngine *W3DViewport::paintEngine() const
{
    return nullptr;
}

void W3DViewport::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    initWW3D();

    if (_initialized && !_timer.isActive()) {
        _elapsed.restart();
        _timer.start();
    }
}

void W3DViewport::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (_initialized) {
        WW3D::Set_Device_Resolution(width(), height(), -1, 1, true);
    }
}

void W3DViewport::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (_timer.isActive()) {
        _timer.stop();
    }
}

void W3DViewport::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
}

void W3DViewport::renderFrame()
{
    if (!_initialized) {
        return;
    }

    const qint64 elapsed_ms = _elapsed.restart();
    if (elapsed_ms > 0) {
        const auto next_time = WW3D::Get_Sync_Time() + static_cast<unsigned int>(elapsed_ms);
        WW3D::Sync(next_time);
    }

    const Vector3 clear_color(0.08f, 0.08f, 0.08f);
    WW3D::Begin_Render(true, true, clear_color);
    WW3D::End_Render(true);
}

void W3DViewport::initWW3D()
{
    if (_initialized) {
        return;
    }

    createWinId();
    void *hwnd = reinterpret_cast<void *>(winId());

    if (WW3D::Init(hwnd, nullptr, false) != WW3D_ERROR_OK) {
        return;
    }

    if (WW3D::Set_Render_Device(-1, width(), height(), 32, 1, true) != WW3D_ERROR_OK) {
        WW3D::Shutdown();
        return;
    }

    _initialized = true;
}

void W3DViewport::shutdownWW3D()
{
    if (!_initialized) {
        return;
    }

    _timer.stop();
    WW3D::Shutdown();
    _initialized = false;
}
