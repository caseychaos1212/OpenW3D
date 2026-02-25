#include "InstancesPanel.h"

#include <QTreeWidget>
#include <QVBoxLayout>

namespace leveledit_qt {

InstancesPanel::InstancesPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *tree = new QTreeWidget(this);
    tree->setHeaderLabels({QStringLiteral("Instance"), QStringLiteral("Type")});

    auto *group = new QTreeWidgetItem(tree);
    group->setText(0, QStringLiteral("World"));

    auto *item = new QTreeWidgetItem(group);
    item->setText(0, QStringLiteral("SampleObject01"));
    item->setText(1, QStringLiteral("Static"));

    tree->expandAll();
    layout->addWidget(tree, 1);
}

} // namespace leveledit_qt
