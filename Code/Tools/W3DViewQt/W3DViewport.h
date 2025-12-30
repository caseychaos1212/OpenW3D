#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

class W3DViewport final : public QWidget
{
    Q_OBJECT

public:
    explicit W3DViewport(QWidget *parent = nullptr);
    ~W3DViewport() override;

protected:
    QPaintEngine *paintEngine() const override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void renderFrame();

private:
    void initWW3D();
    void shutdownWW3D();

    QTimer _timer;
    QElapsedTimer _elapsed;
    bool _initialized = false;
};
