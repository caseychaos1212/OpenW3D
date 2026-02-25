#include "ConversationsPanel.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace leveledit_qt {

ConversationsPanel::ConversationsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *list = new QListWidget(this);
    list->addItem(QStringLiteral("Conversation_Intro (placeholder)"));
    list->addItem(QStringLiteral("Conversation_Objective (placeholder)"));

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(new QPushButton(QStringLiteral("New"), this));
    buttonRow->addWidget(new QPushButton(QStringLiteral("Edit"), this));
    buttonRow->addWidget(new QPushButton(QStringLiteral("Delete"), this));
    buttonRow->addStretch(1);

    layout->addWidget(list, 1);
    layout->addLayout(buttonRow);
}

} // namespace leveledit_qt
