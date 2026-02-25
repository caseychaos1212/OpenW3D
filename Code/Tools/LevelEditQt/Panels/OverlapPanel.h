#pragma once

#include <QWidget>

namespace leveledit_qt {

class OverlapPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit OverlapPanel(QWidget *parent = nullptr);
};

} // namespace leveledit_qt
