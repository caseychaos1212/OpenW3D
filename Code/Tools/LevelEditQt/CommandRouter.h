#pragma once

#include "CommandIds.h"

#include <QHash>
#include <QString>

namespace leveledit_qt {

class CommandRouter
{
public:
    CommandRouter();

    bool execute(CommandId id);
    bool isEnabled(CommandId id) const;
    bool isChecked(CommandId id) const;

    QString lastStatusMessage() const { return _lastStatusMessage; }
    bool lastExecutionWasStub() const { return _lastExecutionWasStub; }

    void setLevelLoaded(bool loaded) { _levelLoaded = loaded; }
    bool isLevelLoaded() const { return _levelLoaded; }

private:
    bool toggleCommand(CommandId id, const QString &statusLabel);
    bool isToggleCommand(CommandId id) const;
    bool requiresLoadedLevel(CommandId id) const;

    bool _levelLoaded = false;
    QHash<int, bool> _checkStates;
    QString _lastStatusMessage;
    bool _lastExecutionWasStub = false;
};

} // namespace leveledit_qt
