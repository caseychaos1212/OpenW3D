#pragma once

#include <QDockWidget>

namespace leveledit_qt {

class OutputDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit OutputDock(QWidget *parent = nullptr);

    void appendLine(const QString &line);
};

} // namespace leveledit_qt
