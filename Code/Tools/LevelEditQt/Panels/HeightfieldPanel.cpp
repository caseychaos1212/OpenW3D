#include "HeightfieldPanel.h"

#include <QFormLayout>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace leveledit_qt {

HeightfieldPanel::HeightfieldPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *form = new QFormLayout;

    auto *brushSize = new QSlider(Qt::Horizontal, this);
    brushSize->setRange(1, 100);
    brushSize->setValue(25);

    auto *strength = new QSpinBox(this);
    strength->setRange(1, 100);
    strength->setValue(50);

    form->addRow(QStringLiteral("Brush size"), brushSize);
    form->addRow(QStringLiteral("Strength"), strength);

    layout->addLayout(form);
    layout->addStretch(1);
}

} // namespace leveledit_qt
