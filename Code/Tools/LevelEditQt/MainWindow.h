#pragma once

#include "ActionStateModel.h"
#include "CommandRouter.h"
#include "FeatureProfile.h"
#include "RuntimeSession.h"
#include "SettingsBridge.h"

#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QMainWindow>
#include <QStringList>

class QAction;
class QMenu;

namespace leveledit_qt {

class LevelEditViewport;
class OutputDock;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    QAction *addCommandAction(QMenu *menu,
                              const QString &text,
                              CommandId id,
                              const QList<QKeySequence> &shortcuts = {},
                              bool checkable = false);

    bool initializeRuntime();
    void createMenus();
    void createDockSurfaces();
    void createFileMenu();
    void createEditMenu();
    void createViewMenu();
    void createObjectMenu();
    void createVisMenu();
    void createPathfindingMenu();
    void createLightingMenu();
    void createSoundsMenu();
    void createCameraMenu();
    void createStringsMenu();
    void createPresetsMenu();
    void createReportMenu();
    void createDebugMenu();

    void handleCommand(CommandId id);
    void refreshActionStates();

    void setCurrentLevelPath(const QString &path);
    void loadRecentFiles();
    void updateRecentFilesMenu();
    void addRecentFile(const QString &path);
    QString pickLevelPathForOpen() const;
    QString pickLevelPathForSave(const QString &initialPath = QString()) const;

    void openLevelFromPath(const QString &path);
    void saveLevelToPath(const QString &path);

    LevelEditViewport *_viewport = nullptr;
    OutputDock *_outputDock = nullptr;
    QMenu *_recentFilesMenu = nullptr;
    QStringList _recentFiles;
    QString _currentLevelPath;

    SettingsBridge _settings;
    RuntimeSession _runtime;
    CommandRouter _router;
    ActionStateModel _actionStateModel;

    QHash<int, QAction *> _actions;
    LevelEditQtProfile _profile = LevelEditQtProfile::Public;
};

} // namespace leveledit_qt
