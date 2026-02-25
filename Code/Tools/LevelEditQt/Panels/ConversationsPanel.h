#pragma once

#include <QWidget>

namespace leveledit_qt {

class ConversationsPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ConversationsPanel(QWidget *parent = nullptr);
};

} // namespace leveledit_qt
