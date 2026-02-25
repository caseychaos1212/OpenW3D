#include "AmbientLightDock.h"

#include <QFormLayout>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

namespace leveledit_qt {

AmbientLightDock::AmbientLightDock(QWidget *parent)
    : QDockWidget(QStringLiteral("Ambient Light"), parent)
{
    setObjectName(QStringLiteral("AmbientLightDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *form = new QFormLayout;

    auto *red = new QSlider(Qt::Horizontal, container);
    red->setRange(0, 255);
    red->setValue(128);

    auto *green = new QSlider(Qt::Horizontal, container);
    green->setRange(0, 255);
    green->setValue(128);

    auto *blue = new QSlider(Qt::Horizontal, container);
    blue->setRange(0, 255);
    blue->setValue(128);

    form->addRow(QStringLiteral("Red"), red);
    form->addRow(QStringLiteral("Green"), green);
    form->addRow(QStringLiteral("Blue"), blue);

    layout->addLayout(form);
    layout->addStretch(1);
    setWidget(container);
}

} // namespace leveledit_qt
