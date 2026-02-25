#include "OutputDock.h"

#include <QPlainTextEdit>

namespace leveledit_qt {

OutputDock::OutputDock(QWidget *parent)
    : QDockWidget(QStringLiteral("Output"), parent)
{
    setObjectName(QStringLiteral("OutputDock"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    auto *output = new QPlainTextEdit(this);
    output->setReadOnly(true);
    output->setLineWrapMode(QPlainTextEdit::NoWrap);
    output->appendPlainText(QStringLiteral("[LevelEditQt] Output dock initialized."));
    setWidget(output);
}

void OutputDock::appendLine(const QString &line)
{
    auto *output = qobject_cast<QPlainTextEdit *>(widget());
    if (!output) {
        return;
    }

    output->appendPlainText(line);
}

} // namespace leveledit_qt
