#pragma once

#include <QWidget>

namespace leveledit_qt {

class PresetsPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit PresetsPanel(QWidget *parent = nullptr);
};

} // namespace leveledit_qt
