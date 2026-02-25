#include "ActionStateModel.h"

#include "CommandRouter.h"

namespace leveledit_qt {

ActionState ActionStateModel::stateFor(CommandId id) const
{
    ActionState state;
    if (!_router) {
        state.status_text = QStringLiteral("No command router attached.");
        return state;
    }

    state.enabled = _router->isEnabled(id);
    state.checked = _router->isChecked(id);
    state.status_text = _router->lastStatusMessage();
    return state;
}

} // namespace leveledit_qt
