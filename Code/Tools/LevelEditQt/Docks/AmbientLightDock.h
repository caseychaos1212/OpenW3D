#pragma once

#include <QDockWidget>

namespace leveledit_qt {

class AmbientLightDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit AmbientLightDock(QWidget *parent = nullptr);
};

} // namespace leveledit_qt
