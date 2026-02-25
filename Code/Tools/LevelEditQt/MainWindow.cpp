#include "MainWindow.h"

#include "Docks/AmbientLightDock.h"
#include "Docks/CameraSettingsDock.h"
#include "Docks/OutputDock.h"
#include "DeviceSelectionDialog.h"
#include "LevelEditViewport.h"
#include "ConfigSettings.h"
#include "Panels/ConversationsPanel.h"
#include "Panels/HeightfieldPanel.h"
#include "Panels/InstancesPanel.h"
#include "Panels/OverlapPanel.h"
#include "Panels/PresetsPanel.h"
#include "RecentFiles.h"
#include "ShortcutHelpers.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>

namespace leveledit_qt {

namespace {

QString CommandLabel(CommandId id)
{
    return QStringLiteral("0x%1").arg(LegacyCommandValue(id), 0, 16);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , _actionStateModel(&_router)
{
    _profile = ParseProfileFromBuildOrSettings();
    setWindowTitle(QStringLiteral("LevelEditQt [%1]").arg(QString::fromLatin1(ToString(_profile))));
    resize(1440, 900);

    _viewport = new LevelEditViewport(this);
    setCentralWidget(_viewport);

    statusBar()->showMessage(QStringLiteral("Initializing LevelEditQt..."));
    connect(_viewport, &LevelEditViewport::interactionStatusChanged,
            this, [this](const QString &message) { statusBar()->showMessage(message, 3000); });

    createMenus();
    createDockSurfaces();
    loadRecentFiles();

    if (!initializeRuntime()) {
        statusBar()->showMessage(QStringLiteral("Runtime initialization canceled."));
    }

    // Keep F3/F4/F5/F6/F7/F8/F9 available even while focus is in dock/panel widgets.
    auto addWindowShortcut = [this](CommandId id, const QList<QKeySequence> &shortcuts) {
        QAction *action = qtcommon::CreateWindowShortcutAction(this, shortcuts);
        if (!action) {
            return;
        }

        connect(action, &QAction::triggered, this, [this, id]() { handleCommand(id); });
    };

    addWindowShortcut(CommandId::ToggleManVisPoints, {QKeySequence(QStringLiteral("F3"))});
    addWindowShortcut(CommandId::ModeCamera, {QKeySequence(QStringLiteral("F5"))});
    addWindowShortcut(CommandId::ObjectManipulate, {QKeySequence(QStringLiteral("F6"))});
    addWindowShortcut(CommandId::ModeOrbit, {QKeySequence(QStringLiteral("F7"))});
    addWindowShortcut(CommandId::ModeWalkthrough, {QKeySequence(QStringLiteral("F8"))});
    addWindowShortcut(CommandId::ToggleWireframeMode, {QKeySequence(QStringLiteral("F9"))});
    addWindowShortcut(CommandId::TogglePerformanceSampling, {QKeySequence(QStringLiteral("F4"))});
    addWindowShortcut(CommandId::VisInvert, {QKeySequence(QStringLiteral("F10"))});
    addWindowShortcut(CommandId::VisDisable, {QKeySequence(QStringLiteral("F11"))});
    addWindowShortcut(CommandId::RotateLeft, {QKeySequence(QStringLiteral(","))});
    addWindowShortcut(CommandId::RotateRight, {QKeySequence(QStringLiteral("."))});
    addWindowShortcut(CommandId::DeleteSelection, {QKeySequence(Qt::Key_Delete)});
    addWindowShortcut(CommandId::Escape, {QKeySequence(Qt::Key_Escape)});
    addWindowShortcut(CommandId::GenVis, {QKeySequence(Qt::Key_Space)});
    addWindowShortcut(CommandId::VolDec, {QKeySequence(QStringLiteral("Ctrl+Q"))});
    addWindowShortcut(CommandId::VolInc, {QKeySequence(QStringLiteral("Ctrl+W"))});
    addWindowShortcut(CommandId::CamSpeedIncrease,
                      {QKeySequence(Qt::CTRL | Qt::Key_Plus), QKeySequence(Qt::CTRL | Qt::Key_Equal)});
    addWindowShortcut(CommandId::CamSpeedDecrease, {QKeySequence(Qt::CTRL | Qt::Key_Minus)});
    addWindowShortcut(CommandId::RestrictX, {QKeySequence(QStringLiteral("Ctrl+X"))});
    addWindowShortcut(CommandId::RestrictY, {QKeySequence(QStringLiteral("Ctrl+Y"))});
    addWindowShortcut(CommandId::RestrictZ, {QKeySequence(QStringLiteral("Ctrl+Z"))});
    addWindowShortcut(CommandId::CamElevateDn, {QKeySequence(Qt::KeypadModifier | Qt::Key_1)});
    addWindowShortcut(CommandId::CamBkwd, {QKeySequence(Qt::KeypadModifier | Qt::Key_2)});
    addWindowShortcut(CommandId::CamLookDn, {QKeySequence(Qt::KeypadModifier | Qt::Key_3)});
    addWindowShortcut(CommandId::CamTurnLeft, {QKeySequence(Qt::KeypadModifier | Qt::Key_4)});
    addWindowShortcut(CommandId::CamReset, {QKeySequence(Qt::KeypadModifier | Qt::Key_5)});
    addWindowShortcut(CommandId::CamTurnRight, {QKeySequence(Qt::KeypadModifier | Qt::Key_6)});
    addWindowShortcut(CommandId::CamElevateUp, {QKeySequence(Qt::KeypadModifier | Qt::Key_7)});
    addWindowShortcut(CommandId::CamFwd, {QKeySequence(Qt::KeypadModifier | Qt::Key_8)});
    addWindowShortcut(CommandId::CamLookUp, {QKeySequence(Qt::KeypadModifier | Qt::Key_9)});

    refreshActionStates();
}

MainWindow::~MainWindow()
{
    _runtime.shutdown();
}

QAction *MainWindow::addCommandAction(QMenu *menu,
                                      const QString &text,
                                      CommandId id,
                                      const QList<QKeySequence> &shortcuts,
                                      bool checkable)
{
    QAction *action = menu->addAction(text);
    action->setData(LegacyCommandValue(id));
    action->setCheckable(checkable);

    if (!shortcuts.isEmpty()) {
        action->setShortcuts(shortcuts);
    }

    connect(action, &QAction::triggered, this, [this, id]() { handleCommand(id); });

    _actions.insert(LegacyCommandValue(id), action);
    return action;
}

bool MainWindow::initializeRuntime()
{
    DeviceSelectionDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    RuntimeInitOptions options;
    options.viewport_hwnd = _viewport->nativeHwnd();
    options.device_index = dialog.deviceIndex();
    options.bits_per_pixel = dialog.bitsPerPixel();
    options.windowed = dialog.isWindowed();
    options.profile = _profile;

    std::string error;
    if (!_runtime.initialize(options, error)) {
        QMessageBox::critical(this,
                              QStringLiteral("Runtime Init Failed"),
                              QString::fromStdString(error));
        return false;
    }

    statusBar()->showMessage(QStringLiteral("Runtime initialized."), 3000);
    if (_outputDock) {
        _outputDock->appendLine(QStringLiteral("[Runtime] Initialized."));

        const RuntimeCapabilities caps = _runtime.capabilities();
        if (caps.source_control_enabled) {
            _outputDock->appendLine(caps.source_control_read_only
                                        ? QStringLiteral("[Runtime] Source control: read-only")
                                        : QStringLiteral("[Runtime] Source control: editable"));
        }
        if (caps.ddb_json_mirror_enabled) {
            _outputDock->appendLine(QStringLiteral("[Runtime] DDB JSON mirror: enabled"));
        }
    }
    return true;
}

void MainWindow::createMenus()
{
    createFileMenu();
    createEditMenu();
    createViewMenu();
    createObjectMenu();
    createVisMenu();
    createPathfindingMenu();
    createLightingMenu();
    createSoundsMenu();
    createCameraMenu();
    createStringsMenu();
    createPresetsMenu();
    createReportMenu();

    if (IsFullProfile(_profile)) {
        createDebugMenu();
    }
}

void MainWindow::createDockSurfaces()
{
    auto *panelTabs = new QTabWidget(this);
    panelTabs->setTabPosition(QTabWidget::North);
    panelTabs->setDocumentMode(true);
    panelTabs->addTab(new PresetsPanel(panelTabs), QStringLiteral("Presets"));
    panelTabs->addTab(new InstancesPanel(panelTabs), QStringLiteral("Instances"));
    panelTabs->addTab(new ConversationsPanel(panelTabs), QStringLiteral("Conversations"));
    panelTabs->addTab(new OverlapPanel(panelTabs), QStringLiteral("Overlap"));
    panelTabs->addTab(new HeightfieldPanel(panelTabs), QStringLiteral("Heightfield"));

    auto *panelDock = new QDockWidget(QStringLiteral("Panels"), this);
    panelDock->setObjectName(QStringLiteral("PanelsDock"));
    panelDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    panelDock->setWidget(panelTabs);
    addDockWidget(Qt::LeftDockWidgetArea, panelDock);

    _outputDock = new OutputDock(this);
    addDockWidget(Qt::BottomDockWidgetArea, _outputDock);

    auto *ambientDock = new AmbientLightDock(this);
    addDockWidget(Qt::RightDockWidgetArea, ambientDock);

    auto *cameraDock = new CameraSettingsDock(this);
    addDockWidget(Qt::RightDockWidgetArea, cameraDock);
    tabifyDockWidget(ambientDock, cameraDock);
}

void MainWindow::createFileMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&File"));

    addCommandAction(menu, QStringLiteral("&New\tCtrl+N"), CommandId::FileNew, {QKeySequence::New});
    addCommandAction(menu, QStringLiteral("&Open...\tCtrl+O"), CommandId::FileOpen, {QKeySequence::Open});
    addCommandAction(menu,
                     IsFullProfile(_profile) ? QStringLiteral("&Save\tCtrl+S")
                                             : QStringLiteral("&Save Current Level\tCtrl+S"),
                     CommandId::FileSave,
                     {QKeySequence::Save});
    addCommandAction(menu,
                     IsFullProfile(_profile) ? QStringLiteral("Save &As...")
                                             : QStringLiteral("Save Current Level &As..."),
                     CommandId::FileSaveAs,
                     {QKeySequence::SaveAs});
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Save Presets..."), CommandId::SavePresets);
    menu->addSeparator();

    if (IsFullProfile(_profile)) {
        addCommandAction(menu, QStringLiteral("&Export..."), CommandId::LevelExport);
        addCommandAction(menu, QStringLiteral("Ex&port Always..."), CommandId::AlwaysExport);
        addCommandAction(menu, QStringLiteral("Export &Local Always..."), CommandId::AlwaysLocalExport);
        addCommandAction(menu, QStringLiteral("&Batch Export..."), CommandId::BatchExport);
        menu->addSeparator();
        addCommandAction(menu, QStringLiteral("Export &Language Version..."), CommandId::ExportLanguage);
        addCommandAction(menu,
                         QStringLiteral("Export &Installer Language Version..."),
                         CommandId::ExportInstallerLanguageVersion);
    } else {
        addCommandAction(menu, QStringLiteral("&Export Mod Package..."), CommandId::ModExport);
    }

    menu->addSeparator();

    _recentFilesMenu = menu->addMenu(QStringLiteral("Recent File"));
    updateRecentFilesMenu();

    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("E&xit"), CommandId::FileExit, {QKeySequence::Quit});
}

void MainWindow::createEditMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&Edit"));

    addCommandAction(menu, QStringLiteral("&Undo\tAlt+Backspace"), CommandId::EditUndo, {QKeySequence::Undo});
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Cu&t\tShift+Del"), CommandId::EditCut, {QKeySequence::Cut});
    addCommandAction(menu, QStringLiteral("&Copy\tCtrl+C"), CommandId::EditCopy, {QKeySequence::Copy});
    addCommandAction(menu, QStringLiteral("&Paste\tCtrl+V"), CommandId::EditPaste, {QKeySequence::Paste});
    menu->addSeparator();

    if (IsFullProfile(_profile)) {
        addCommandAction(menu, QStringLiteral("Select &Asset Database..."), CommandId::SpecifyAssetDatabase);
        addCommandAction(menu, QStringLiteral("Change Asset &Tree..."), CommandId::ChangeBase);
    }

    addCommandAction(menu, QStringLiteral("&Include Files..."), CommandId::EditIncludes);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Repartition Culling Systems"), CommandId::Repartition,
                     {QKeySequence(QStringLiteral("Ctrl+E"))});
    addCommandAction(menu, QStringLiteral("&Verify Culling Systems"), CommandId::VerifyCulling);
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("Terrain &Selectable"),
                     CommandId::ToggleTerrainSelection,
                     {},
                     true);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Background Settings..."), CommandId::BackgroundSettings);
    addCommandAction(menu, QStringLiteral("Level Settings..."), CommandId::EditLevelSettings);
}

void MainWindow::createViewMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("Vie&w"));

    addCommandAction(menu, QStringLiteral("&Toolbar"), CommandId::ViewToolbar, {}, true);
    addCommandAction(menu, QStringLiteral("&Status Bar"), CommandId::ViewStatusBar, {}, true);
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("&View Fullscreen\tCtrl+Shift+F"),
                     CommandId::ViewFullscreen,
                     {QKeySequence(QStringLiteral("Ctrl+Shift+F"))});
    addCommandAction(menu, QStringLiteral("Change &Device..."), CommandId::ViewChangeDevice);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Display &Options..."), CommandId::VisualOptions);
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("Show Static &Anim Objects\tCtrl+Shift+T"),
                     CommandId::DisplayStaticAnimObjects,
                     {QKeySequence(QStringLiteral("Ctrl+Shift+T"))},
                     true);
    addCommandAction(menu, QStringLiteral("Show &Editor-Only Objects"), CommandId::ShowEditorObjects, {}, true);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("View Sound S&pheres"), CommandId::ViewSoundSpheres, {}, true);
    addCommandAction(menu, QStringLiteral("View &Light Spheres"), CommandId::ViewLightSpheres, {}, true);
    addCommandAction(menu,
                     QStringLiteral("Toggle &Attenuation Spheres"),
                     CommandId::ToggleAttenuationSpheres,
                     {QKeySequence(QStringLiteral("Ctrl+1"))},
                     true);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Building Power On"), CommandId::BuildingPowerOn, {}, true);
}

void MainWindow::createObjectMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&Object"));

    addCommandAction(menu,
                     QStringLiteral("&Drop to ground\tCtrl+D"),
                     CommandId::DropToGround,
                     {QKeySequence(QStringLiteral("Ctrl+D"))});
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("&Lock Objects\tEnter"),
                     CommandId::LockObjects,
                     {QKeySequence(Qt::Key_Return)});
    addCommandAction(menu,
                     QStringLiteral("&Unlock Objects\tCtrl+U"),
                     CommandId::UnlockObjects,
                     {QKeySequence(QStringLiteral("Ctrl+U"))});
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("Increase Attenuation Spheres\tCtrl+0"),
                     CommandId::IncreaseSphere,
                     {QKeySequence(QStringLiteral("Ctrl+0"))});
    addCommandAction(menu,
                     QStringLiteral("Decrease Attenuation Spheres\tCtrl+9"),
                     CommandId::DecreaseSphere,
                     {QKeySequence(QStringLiteral("Ctrl+9"))});
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Import Dynamic..."), CommandId::ImportDynObjs);
    addCommandAction(menu, QStringLiteral("&Export Dynamic..."), CommandId::ExportDynObjs);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Im&port Static..."), CommandId::ImportStatic);
    addCommandAction(menu, QStringLiteral("Export &Static..."), CommandId::ExportStatic);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Export Tile List..."), CommandId::ExportTileList);
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("&Replace Selection...\tCtrl+H"),
                     CommandId::BulkReplace,
                     {QKeySequence(QStringLiteral("Ctrl+H"))});
    addCommandAction(menu,
                     QStringLiteral("Add Point...\tCtrl+P"),
                     CommandId::AddChildNode,
                     {QKeySequence(QStringLiteral("Ctrl+P"))});
    addCommandAction(menu, QStringLiteral("Goto Object..."), CommandId::GotoObject);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Set Start ID..."), CommandId::SetNodeIdStart);
    addCommandAction(menu, QStringLiteral("&Check IDs..."), CommandId::CheckIds);
    addCommandAction(menu, QStringLiteral("Fix ID Collisions..."), CommandId::FixIdCollisions);
    addCommandAction(menu, QStringLiteral("Remap Unimportant IDs"), CommandId::RemapUnimportantIds);
    addCommandAction(menu, QStringLiteral("&Remap IDs..."), CommandId::RemapIds);
}

void MainWindow::createVisMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&Vis"));

    addCommandAction(menu, QStringLiteral("&Render with Vis Camera"), CommandId::UseVisCamera, {}, true);
    addCommandAction(menu,
                     QStringLiteral("Enable Vis Sector Fallback"),
                     CommandId::EnableVisSectorFallback,
                     {},
                     true);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Debug Report..."), CommandId::ViewVisErrors);
    addCommandAction(menu, QStringLiteral("Statistics..."), CommandId::VisStats);
    addCommandAction(menu, QStringLiteral("View &Points"), CommandId::ViewVisPoints, {}, true);
    addCommandAction(menu, QStringLiteral("&View Vis Window"), CommandId::ViewVisWindow, {}, true);
    addCommandAction(menu,
                     QStringLiteral("&Toggle Manual Vis Points\tF3"),
                     CommandId::ToggleManVisPoints,
                     {QKeySequence(QStringLiteral("F3"))},
                     true);
    addCommandAction(menu,
                     QStringLiteral("&Make Manual Vis Point\tCtrl+`"),
                     CommandId::MakeVisPoint,
                     {QKeySequence(QStringLiteral("Ctrl+`"))});
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Discard Vis Data"), CommandId::DiscardVis);
    addCommandAction(menu, QStringLiteral("Reset D&ynamic Culling System"), CommandId::ResetDynaCullSystem);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Import &Farm Data..."), CommandId::ImportVis);
    addCommandAction(menu, QStringLiteral("Run &Job File..."), CommandId::RunJob);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Import Remap Data..."), CommandId::ImportVisRemapData);
    addCommandAction(menu, QStringLiteral("E&xport Remap Data..."), CommandId::ExportVisRemapData);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Run Manual Vis Points..."), CommandId::RunManualVisPoints);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Build Dynamic Culling System..."), CommandId::BuildDynaCullsys);
    addCommandAction(menu, QStringLiteral("Auto Generate &Vis..."), CommandId::AutoGenVis);
    addCommandAction(menu, QStringLiteral("&Optimize Vis Data..."), CommandId::OptimizeVisData);
}

void MainWindow::createPathfindingMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&Pathfinding"));

    addCommandAction(menu, QStringLiteral("&Generate Sectors..."), CommandId::GenerateObstacleVolumes);
    addCommandAction(menu, QStringLiteral("Generate Flight Data..."), CommandId::BuildFlightInfo);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Display &Sectors"), CommandId::DisplayPathfindSectors, {}, true);
    addCommandAction(menu, QStringLiteral("Display &Portals"), CommandId::DisplayPathfindPortals, {}, true);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Display P&aths"), CommandId::DisplayPaths, {}, true);
    addCommandAction(menu, QStringLiteral("Display &Full Paths"), CommandId::DisplayFullPaths, {}, true);
    addCommandAction(menu, QStringLiteral("&Test Pathfind"), CommandId::TestPathfind);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Test G&oto"), CommandId::TestGoto);
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("Display &Raw Sectors"),
                     CommandId::DisplayPathfindRawData,
                     {},
                     true);
    addCommandAction(menu, QStringLiteral("Display &Web"), CommandId::DisplayWeb, {}, true);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Discard Data"), CommandId::DiscardPathfind);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Import Data..."), CommandId::ImportPathfind);
    addCommandAction(menu, QStringLiteral("&Export Data..."), CommandId::ExportPathfind);
}

void MainWindow::createLightingMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&Lighting"));

    addCommandAction(menu,
                     QStringLiteral("&Toggle Lights\tCtrl+I"),
                     CommandId::ToggleLights,
                     {QKeySequence(QStringLiteral("Ctrl+I"))},
                     true);
    addCommandAction(menu, QStringLiteral("Display Light &Vectors"), CommandId::DisplayLightVectors, {}, true);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Toggle Sun&light"), CommandId::ToggleSunlight, {}, true);
    addCommandAction(menu, QStringLiteral("&Edit Sunlight..."), CommandId::EditSunlight);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Ambient Light..."), CommandId::ViewAmbientLightDlg);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Vertex Lighting"), CommandId::PrelitVertex, {}, true);
    addCommandAction(menu, QStringLiteral("Multi-&Pass Lighting"), CommandId::PrelitMultipass, {}, true);
    addCommandAction(menu, QStringLiteral("Multi-Te&xture Lighting"), CommandId::PrelitMultitex, {}, true);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&Import..."), CommandId::ImportLights);
    addCommandAction(menu, QStringLiteral("Import &Sunlight..."), CommandId::ImportSunlight);
    addCommandAction(menu, QStringLiteral("E&xport..."), CommandId::ExportLights);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Compute Vertex Solve"), CommandId::ComputeVertexSolve);
}

void MainWindow::createSoundsMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&Sounds"));

    addCommandAction(menu, QStringLiteral("Toggle &Music"), CommandId::ToggleMusic, {}, true);
    addCommandAction(menu, QStringLiteral("Toggle &Sound Effects"), CommandId::ToggleSounds, {}, true);
}

void MainWindow::createCameraMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&Camera"));

    if (IsFullProfile(_profile)) {
        addCommandAction(menu, QStringLiteral("Perspective"), CommandId::CameraPerspective, {}, true);
        addCommandAction(menu, QStringLiteral("Orthographic"), CommandId::CameraOrthographic, {}, true);
        menu->addSeparator();
    }

    addCommandAction(menu,
                     QStringLiteral("&Top\tCtrl+T"),
                     CommandId::CameraTop,
                     {QKeySequence(QStringLiteral("Ctrl+T"))});
    addCommandAction(menu,
                     QStringLiteral("B&ottom\tCtrl+M"),
                     CommandId::CameraBottom,
                     {QKeySequence(QStringLiteral("Ctrl+M"))});
    addCommandAction(menu,
                     QStringLiteral("&Front\tCtrl+F"),
                     CommandId::CameraFront,
                     {QKeySequence(QStringLiteral("Ctrl+F"))});
    addCommandAction(menu,
                     QStringLiteral("&Back\tCtrl+B"),
                     CommandId::CameraBack,
                     {QKeySequence(QStringLiteral("Ctrl+B"))});
    addCommandAction(menu,
                     QStringLiteral("&Left\tCtrl+L"),
                     CommandId::CameraLeft,
                     {QKeySequence(QStringLiteral("Ctrl+L"))});
    addCommandAction(menu,
                     QStringLiteral("&Right\tCtrl+R"),
                     CommandId::CameraRight,
                     {QKeySequence(QStringLiteral("Ctrl+R"))});
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("&Auto level\tCtrl+A"),
                     CommandId::AutoLevel,
                     {QKeySequence(QStringLiteral("Ctrl+A"))});
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("Depth -50\tAlt+Minus"),
                     CommandId::CameraDepthLess,
                     {QKeySequence(QStringLiteral("Alt+-"))});
    addCommandAction(menu,
                     QStringLiteral("Depth +50\tAlt+Plus"),
                     CommandId::CameraDepthMore,
                     {QKeySequence(QStringLiteral("Alt++"))});
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("Goto Location...\tCtrl+G"),
                     CommandId::GotoLocation,
                     {QKeySequence(QStringLiteral("Ctrl+G"))});
}

void MainWindow::createStringsMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&Strings"));

    addCommandAction(menu, QStringLiteral("&Edit Table..."), CommandId::EditStringsTable);

    if (IsFullProfile(_profile)) {
        menu->addSeparator();
        addCommandAction(menu, QStringLiteral("&Import IDs..."), CommandId::ImportStringIds);
        addCommandAction(menu, QStringLiteral("&Export IDs..."), CommandId::ExportStringIds);
    }

    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Export for &Translation..."), CommandId::ExportTranslationData);
    addCommandAction(menu, QStringLiteral("Im&port Translation..."), CommandId::ImportTranslationData);
}

void MainWindow::createPresetsMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("Prese&ts"));

    if (IsFullProfile(_profile)) {
        addCommandAction(menu, QStringLiteral("&Immediate Check In"), CommandId::ImmediatePresetCheckIn, {}, true);
        addCommandAction(menu, QStringLiteral("&Check In..."), CommandId::CheckInPresetChanges);
        menu->addSeparator();
    }

    addCommandAction(menu, QStringLiteral("&Export..."), CommandId::ExportPresets);
    addCommandAction(menu, QStringLiteral("Im&port..."), CommandId::ImportPresets);
    menu->addSeparator();
    addCommandAction(menu,
                     QStringLiteral("Export &File Dependencies..."),
                     CommandId::ExportPresetFileDependencies);
}

void MainWindow::createReportMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&Report"));

    addCommandAction(menu, QStringLiteral("File Usage..."), CommandId::ExportFileUsageReport);
    addCommandAction(menu,
                     QStringLiteral("Missing Translations..."),
                     CommandId::ExportMissingTranslationReport);
}

void MainWindow::createDebugMenu()
{
    QMenu *menu = menuBar()->addMenu(QStringLiteral("&Debug"));
    addCommandAction(menu, QStringLiteral("&Debug Scripts Mode"), CommandId::DebugScriptsMode, {}, true);
    addCommandAction(menu, QStringLiteral("&Create Proxy Objects"), CommandId::CreateProxies);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("&View Memory Log..."), CommandId::CheckMemLog);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("E&xtract RC Strings..."), CommandId::ExtractRcStrings);
    addCommandAction(menu,
                     QStringLiteral("Extract &Installer RC Strings..."),
                     CommandId::ExtractInstallerRcStrings);
    menu->addSeparator();
    addCommandAction(menu, QStringLiteral("Generic Debug Fn 1"), CommandId::ImportSounds);
}

void MainWindow::handleCommand(CommandId id)
{
    switch (id) {
    case CommandId::FileNew:
        _router.execute(id);
        setCurrentLevelPath(QString());
        _router.setLevelLoaded(true);
        statusBar()->showMessage(QStringLiteral("Created new level (placeholder)."), 3000);
        break;
    case CommandId::FileOpen: {
        const QString path = pickLevelPathForOpen();
        if (!path.isEmpty()) {
            openLevelFromPath(path);
        }
        break;
    }
    case CommandId::FileSave:
        if (_currentLevelPath.isEmpty()) {
            const QString path = pickLevelPathForSave();
            if (!path.isEmpty()) {
                saveLevelToPath(path);
            }
        } else {
            saveLevelToPath(_currentLevelPath);
        }
        break;
    case CommandId::FileSaveAs: {
        const QString path = pickLevelPathForSave(_currentLevelPath);
        if (!path.isEmpty()) {
            saveLevelToPath(path);
        }
        break;
    }
    case CommandId::FileExit:
        close();
        break;
    case CommandId::ViewChangeDevice:
        _runtime.shutdown();
        initializeRuntime();
        break;
    default: {
        if (!_router.execute(id)) {
            statusBar()->showMessage(_router.lastStatusMessage(), 4000);
            break;
        }

        if (_router.lastExecutionWasStub()) {
            std::string runtime_error;
            if (_runtime.executeLegacyCommand(LegacyCommandValue(id), runtime_error)) {
                const QString message =
                    QStringLiteral("Command %1 routed through runtime session.")
                        .arg(CommandLabel(id));
                statusBar()->showMessage(message, 4000);
                break;
            }

            const QString message = QStringLiteral(
                                        "Command %1 is stubbed (runtime fallback failed: %2).")
                                        .arg(CommandLabel(id), QString::fromStdString(runtime_error));
            statusBar()->showMessage(message, 5000);
            QMessageBox::information(this, QStringLiteral("LevelEditQt Stub"), message);
        } else {
            statusBar()->showMessage(_router.lastStatusMessage(), 3000);
        }
        break;
    }
    }

    if (_outputDock && !statusBar()->currentMessage().isEmpty()) {
        _outputDock->appendLine(statusBar()->currentMessage());
    }

    refreshActionStates();
}

void MainWindow::refreshActionStates()
{
    for (auto it = _actions.begin(); it != _actions.end(); ++it) {
        QAction *action = it.value();
        if (!action) {
            continue;
        }

        const CommandId id = static_cast<CommandId>(it.key());
        const ActionState state = _actionStateModel.stateFor(id);
        action->setEnabled(state.enabled);
        if (action->isCheckable()) {
            action->setChecked(state.checked);
        }
    }

    // Mirror status bar visibility to the actual status bar.
    if (QAction *statusAction = _actions.value(LegacyCommandValue(CommandId::ViewStatusBar), nullptr)) {
        statusBar()->setVisible(statusAction->isChecked());
    }
}

void MainWindow::setCurrentLevelPath(const QString &path)
{
    _currentLevelPath = path;

    QString title = QStringLiteral("LevelEditQt [%1]").arg(QString::fromLatin1(ToString(_profile)));
    if (!_currentLevelPath.isEmpty()) {
        title += QStringLiteral(" - ") + QFileInfo(_currentLevelPath).fileName();
    }

    setWindowTitle(title);
}

void MainWindow::loadRecentFiles()
{
    QSettings settings = OpenLevelEditSettings();
    _recentFiles = qtcommon::ReadRecentFiles(settings, QStringLiteral("recentFiles"), 10);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    if (!_recentFilesMenu) {
        return;
    }

    _recentFilesMenu->clear();

    if (_recentFiles.isEmpty()) {
        QAction *empty = _recentFilesMenu->addAction(QStringLiteral("(Empty)"));
        empty->setEnabled(false);
        return;
    }

    for (const QString &file : _recentFiles) {
        QAction *action = _recentFilesMenu->addAction(QDir::toNativeSeparators(file));
        action->setData(file);
        connect(action, &QAction::triggered, this, [this, file]() { openLevelFromPath(file); });
    }
}

void MainWindow::addRecentFile(const QString &path)
{
    _recentFiles = qtcommon::AddRecentFile(_recentFiles, path, 10);

    QSettings settings = OpenLevelEditSettings();
    qtcommon::WriteRecentFiles(settings, _recentFiles, QStringLiteral("recentFiles"), 10);

    updateRecentFilesMenu();
}

QString MainWindow::pickLevelPathForOpen() const
{
    const std::string savedDir = _settings.readString("Config", "Last save dir", "");
    const QString initialDir = QString::fromStdString(savedDir);

    return QFileDialog::getOpenFileName(const_cast<MainWindow *>(this),
                                        QStringLiteral("Open Level"),
                                        initialDir,
                                        QStringLiteral("Level Files (*.lvl)"));
}

QString MainWindow::pickLevelPathForSave(const QString &initialPath) const
{
    QString startPath = initialPath;
    if (startPath.isEmpty()) {
        const std::string savedDir = _settings.readString("Config", "Last save dir", "");
        startPath = QString::fromStdString(savedDir);
    }

    return QFileDialog::getSaveFileName(const_cast<MainWindow *>(this),
                                        QStringLiteral("Save Level"),
                                        startPath,
                                        QStringLiteral("Level Files (*.lvl)"));
}

void MainWindow::openLevelFromPath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    std::string error;
    if (!_runtime.openLevel(path.toStdString(), error)) {
        QMessageBox::warning(this,
                             QStringLiteral("Open Failed"),
                             QString::fromStdString(error));
        return;
    }

    _router.setLevelLoaded(true);
    setCurrentLevelPath(path);
    addRecentFile(path);

    _settings.writeString("Config", "Last save dir", QFileInfo(path).absolutePath().toStdString());

    statusBar()->showMessage(QStringLiteral("Opened level: %1").arg(QFileInfo(path).fileName()), 4000);
    if (_outputDock) {
        _outputDock->appendLine(statusBar()->currentMessage());
    }
    refreshActionStates();
}

void MainWindow::saveLevelToPath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    std::string error;
    if (!_runtime.saveLevel(path.toStdString(), error)) {
        QMessageBox::warning(this,
                             QStringLiteral("Save Failed"),
                             QString::fromStdString(error));
        return;
    }

    _router.setLevelLoaded(true);
    setCurrentLevelPath(path);
    addRecentFile(path);

    _settings.writeString("Config", "Last save dir", QFileInfo(path).absolutePath().toStdString());

    statusBar()->showMessage(QStringLiteral("Saved level: %1").arg(QFileInfo(path).fileName()), 4000);
    if (_outputDock) {
        _outputDock->appendLine(statusBar()->currentMessage());
    }
    refreshActionStates();
}

} // namespace leveledit_qt
