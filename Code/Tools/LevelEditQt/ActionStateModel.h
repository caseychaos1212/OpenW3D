#pragma once

#include "CommandIds.h"

#include <QString>

namespace leveledit_qt {

class CommandRouter;

struct ActionState {
    bool enabled = false;
    bool checked = false;
    QString status_text;
};

class ActionStateModel
{
public:
    explicit ActionStateModel(const CommandRouter *router) : _router(router) {}

    ActionState stateFor(CommandId id) const;

private:
    const CommandRouter *_router = nullptr;
};

} // namespace leveledit_qt
