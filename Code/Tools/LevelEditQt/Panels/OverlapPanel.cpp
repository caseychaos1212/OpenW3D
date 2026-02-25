#include "OverlapPanel.h"

#include <QCheckBox>
#include <QListWidget>
#include <QVBoxLayout>

namespace leveledit_qt {

OverlapPanel::OverlapPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *showCollisions = new QCheckBox(QStringLiteral("Show overlap volumes"), this);
    showCollisions->setChecked(true);
    auto *autoRefresh = new QCheckBox(QStringLiteral("Auto refresh"), this);
    autoRefresh->setChecked(true);

    auto *list = new QListWidget(this);
    list->addItem(QStringLiteral("No overlaps detected (placeholder)"));

    layout->addWidget(showCollisions);
    layout->addWidget(autoRefresh);
    layout->addWidget(list, 1);
}

} // namespace leveledit_qt
