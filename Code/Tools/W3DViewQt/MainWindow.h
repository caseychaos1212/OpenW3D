#pragma once

#include <QMainWindow>

class W3DViewport;

class W3DViewMainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit W3DViewMainWindow(QWidget *parent = nullptr);
};
