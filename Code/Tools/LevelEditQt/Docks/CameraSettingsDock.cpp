#include "CameraSettingsDock.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QWidget>

namespace leveledit_qt {

CameraSettingsDock::CameraSettingsDock(QWidget *parent)
    : QDockWidget(QStringLiteral("Camera Settings"), parent)
{
    setObjectName(QStringLiteral("CameraSettingsDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *form = new QFormLayout;

    auto *moveSpeed = new QDoubleSpinBox(container);
    moveSpeed->setRange(0.1, 1000.0);
    moveSpeed->setValue(12.0);
    moveSpeed->setSingleStep(0.5);

    auto *turnSpeed = new QDoubleSpinBox(container);
    turnSpeed->setRange(0.1, 360.0);
    turnSpeed->setValue(45.0);
    turnSpeed->setSingleStep(1.0);

    auto *depth = new QDoubleSpinBox(container);
    depth->setRange(10.0, 100000.0);
    depth->setValue(5000.0);
    depth->setSingleStep(10.0);

    form->addRow(QStringLiteral("Move speed"), moveSpeed);
    form->addRow(QStringLiteral("Turn speed"), turnSpeed);
    form->addRow(QStringLiteral("Depth"), depth);

    layout->addLayout(form);
    layout->addStretch(1);
    setWidget(container);
}

} // namespace leveledit_qt
