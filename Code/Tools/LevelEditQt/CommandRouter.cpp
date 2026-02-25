#include "CommandRouter.h"

namespace leveledit_qt {

namespace {

bool IsBasicFileCommand(CommandId id)
{
    return id == CommandId::FileNew || id == CommandId::FileOpen || id == CommandId::FileSave ||
           id == CommandId::FileSaveAs || id == CommandId::FileExit;
}

} // namespace

CommandRouter::CommandRouter()
{
    _checkStates.insert(LegacyCommandValue(CommandId::ToggleTerrainSelection), true);
    _checkStates.insert(LegacyCommandValue(CommandId::ViewToolbar), true);
    _checkStates.insert(LegacyCommandValue(CommandId::ViewStatusBar), true);
    _checkStates.insert(LegacyCommandValue(CommandId::CameraPerspective), true);
}

bool CommandRouter::execute(CommandId id)
{
    _lastExecutionWasStub = false;

    if (!isEnabled(id)) {
        _lastStatusMessage = QStringLiteral("Command unavailable in current state.");
        return false;
    }

    if (isToggleCommand(id)) {
        return toggleCommand(id, QStringLiteral("Toggled command"));
    }

    if (IsBasicFileCommand(id) || id == CommandId::EditUndo || id == CommandId::EditCut ||
        id == CommandId::EditCopy || id == CommandId::EditPaste || id == CommandId::ModeCamera ||
        id == CommandId::ObjectManipulate || id == CommandId::ModeOrbit ||
        id == CommandId::ModeWalkthrough) {
        _lastStatusMessage = QStringLiteral("Core command routed.");
        return true;
    }

    // Remaining commands are intentional stubs during parity bring-up.
    _lastExecutionWasStub = true;
    _lastStatusMessage = QStringLiteral("Command is stubbed for LevelEditQt parity bring-up.");
    return true;
}

bool CommandRouter::isEnabled(CommandId id) const
{
    if (id == CommandId::FileSave) {
        return _levelLoaded;
    }

    if (id == CommandId::FileSaveAs) {
        return _levelLoaded;
    }

    if (requiresLoadedLevel(id)) {
        return _levelLoaded;
    }

    return true;
}

bool CommandRouter::isChecked(CommandId id) const
{
    const auto it = _checkStates.constFind(LegacyCommandValue(id));
    if (it == _checkStates.constEnd()) {
        return false;
    }

    return it.value();
}

bool CommandRouter::toggleCommand(CommandId id, const QString &statusLabel)
{
    const int legacyId = LegacyCommandValue(id);
    const bool newValue = !_checkStates.value(legacyId, false);
    _checkStates.insert(legacyId, newValue);

    _lastStatusMessage = QStringLiteral("%1: %2")
                             .arg(statusLabel)
                             .arg(newValue ? QStringLiteral("On") : QStringLiteral("Off"));
    return true;
}

bool CommandRouter::isToggleCommand(CommandId id) const
{
    switch (id) {
    case CommandId::ViewToolbar:
    case CommandId::ViewStatusBar:
    case CommandId::ToggleTerrainSelection:
    case CommandId::ToggleWireframeMode:
    case CommandId::DisplayStaticAnimObjects:
    case CommandId::ShowEditorObjects:
    case CommandId::ViewSoundSpheres:
    case CommandId::ViewLightSpheres:
    case CommandId::ToggleAttenuationSpheres:
    case CommandId::BuildingPowerOn:
    case CommandId::UseVisCamera:
    case CommandId::EnableVisSectorFallback:
    case CommandId::ViewVisPoints:
    case CommandId::ViewVisWindow:
    case CommandId::ToggleManVisPoints:
    case CommandId::DisplayPathfindSectors:
    case CommandId::DisplayPathfindPortals:
    case CommandId::DisplayPaths:
    case CommandId::DisplayFullPaths:
    case CommandId::DisplayPathfindRawData:
    case CommandId::DisplayWeb:
    case CommandId::ToggleLights:
    case CommandId::DisplayLightVectors:
    case CommandId::ToggleSunlight:
    case CommandId::PrelitVertex:
    case CommandId::PrelitMultipass:
    case CommandId::PrelitMultitex:
    case CommandId::ToggleMusic:
    case CommandId::ToggleSounds:
    case CommandId::CameraPerspective:
    case CommandId::CameraOrthographic:
    case CommandId::ImmediatePresetCheckIn:
    case CommandId::DebugScriptsMode:
        return true;
    default:
        return false;
    }
}

bool CommandRouter::requiresLoadedLevel(CommandId id) const
{
    switch (id) {
    case CommandId::ViewToolbar:
    case CommandId::ViewStatusBar:
    case CommandId::FileNew:
    case CommandId::FileOpen:
    case CommandId::FileExit:
    case CommandId::ModeCamera:
    case CommandId::ObjectManipulate:
    case CommandId::ModeOrbit:
    case CommandId::ModeWalkthrough:
    case CommandId::ToggleWireframeMode:
    case CommandId::ViewChangeDevice:
    case CommandId::SpecifyAssetDatabase:
    case CommandId::ChangeBase:
    case CommandId::ImmediatePresetCheckIn:
    case CommandId::CheckInPresetChanges:
    case CommandId::DebugScriptsMode:
    case CommandId::CreateProxies:
    case CommandId::CheckMemLog:
    case CommandId::ExtractRcStrings:
    case CommandId::ExtractInstallerRcStrings:
    case CommandId::ImportSounds:
        return false;
    default:
        return true;
    }
}

} // namespace leveledit_qt
