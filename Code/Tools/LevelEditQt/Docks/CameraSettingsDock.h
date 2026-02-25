#pragma once

#include <QDockWidget>

namespace leveledit_qt {

class CameraSettingsDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit CameraSettingsDock(QWidget *parent = nullptr);
};

} // namespace leveledit_qt
