#pragma once

#include <QWidget>

namespace leveledit_qt {

class InstancesPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit InstancesPanel(QWidget *parent = nullptr);
};

} // namespace leveledit_qt
