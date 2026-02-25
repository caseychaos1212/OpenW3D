#pragma once

#include <QWidget>

namespace leveledit_qt {

class HeightfieldPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit HeightfieldPanel(QWidget *parent = nullptr);
};

} // namespace leveledit_qt
