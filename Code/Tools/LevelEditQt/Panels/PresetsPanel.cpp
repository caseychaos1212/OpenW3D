#include "PresetsPanel.h"

#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace leveledit_qt {

PresetsPanel::PresetsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *filter = new QLineEdit(this);
    filter->setPlaceholderText(QStringLiteral("Filter presets..."));

    auto *list = new QListWidget(this);
    list->addItem(QStringLiteral("Global Preset (placeholder)"));
    list->addItem(QStringLiteral("Mission Preset (placeholder)"));
    list->addItem(QStringLiteral("Object Preset (placeholder)"));

    layout->addWidget(filter);
    layout->addWidget(list, 1);
}

} // namespace leveledit_qt
