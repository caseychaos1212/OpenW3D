#pragma once

#include <QMainWindow>
#include <QString>

class QAction;
class QActionGroup;
class QModelIndex;
class QPoint;
class QMenu;
class QSettings;
class QStandardItem;
class QStandardItemModel;
class QToolBar;
class QTreeView;
class W3DViewport;

class W3DViewMainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit W3DViewMainWindow(QWidget *parent = nullptr);

private slots:
    void newFile();
    void openFile();
    void openRecentFile();
    void openTexturePathsDialog();
    void loadSettingsFile();
    void saveSettingsFile();
    void onCurrentChanged(const QModelIndex &current, const QModelIndex &previous);
    void toggleWireframe(bool enabled);
    void toggleSorting(bool enabled);
    void toggleRestrictAnims(bool enabled);
    void toggleStatusBar(bool visible);
    void toggleBackfaceCulling(bool inverted);
    void setAmbientLight();
    void setSceneLight();
    void increaseAmbientLight();
    void decreaseAmbientLight();
    void increaseSceneLight();
    void decreaseSceneLight();
    void killSceneLight();
    void toggleLightRotateY(bool enabled);
    void toggleLightRotateZ(bool enabled);
    void toggleExposePrelit(bool enabled);
    void setPrelitVertex();
    void setPrelitMultipass();
    void setPrelitMultitex();
    void setBackgroundColor();
    void setBackgroundBitmap();
    void toggleFog(bool enabled);
    void setCameraFront();
    void setCameraBack();
    void setCameraLeft();
    void setCameraRight();
    void setCameraTop();
    void setCameraBottom();
    void resetCamera();
    void setCameraRotateX(bool enabled);
    void setCameraRotateY(bool enabled);
    void setCameraRotateZ(bool enabled);
    void toggleCameraAnimate(bool enabled);
    void toggleCameraResetOnDisplay(bool enabled);
    void toggleCameraBonePosX(bool enabled);
    void openCameraSettings();
    void openCameraDistance();
    void copyScreenSize();
    void openGammaDialog();
    void toggleGammaCorrection(bool enabled);
    void toggleMungeSortOnLoad(bool enabled);
    void openBackgroundObjectDialog();
    void captureScreenshot();
    void makeMovie();
    void selectPrevAsset();
    void selectNextAsset();
    void showTreeContextMenu(const QPoint &pos);
    void startAnimation();
    void pauseAnimation();
    void stopAnimation();
    void stepAnimationForward();
    void stepAnimationBackward();
    void openAnimationSettings();
    void openAdvancedAnimation();
    void generateLod();
    void makeAggregate();
    void renameAggregate();
    void openBoneManagement();
    void autoAssignBoneModels();
    void bindSubobjectLod();
    void createEmitter();
    void scaleEmitter();
    void editEmitter();
    void createSphere();
    void createRing();
    void editPrimitive();
    void createSoundObject();
    void editSoundObject();
    void openAnimatedSoundOptions();
    void importFacialAnims();
    void exportAggregate();
    void exportEmitter();
    void exportLod();
    void exportPrimitive();
    void exportSoundObject();
    void listMissingTextures();
    void copyAssets();
    void addToLineup();
    void showAbout();
    void toggleMainToolbar(bool visible);
    void toggleObjectToolbar(bool visible);
    void toggleAnimationToolbar(bool visible);
    void recordLodScreenArea();
    void toggleLodIncludeNull(bool enabled);
    void selectPrevLod();
    void selectNextLod();
    void toggleLodAutoSwitch(bool enabled);
    void toggleObjectRotateX(bool enabled);
    void toggleObjectRotateY(bool enabled);
    void toggleObjectRotateZ(bool enabled);
    void resetObject();
    void toggleAlternateMaterials();
    void showObjectProperties();
    void setNpatchesLevel(int level);
    void toggleNpatchesGap(bool enabled);

private:
    void applySettings(QSettings &settings);
    void writeSettings(QSettings &settings) const;
    bool loadAssetsFromFile(const QString &path);
    void rebuildAssetTree();
    void addMaterialItems(QStandardItem *parent);
    void addRenderObjectItems(QStandardItem *meshParent,
                              QStandardItem *hierarchyParent,
                              QStandardItem *hlodParent,
                              QStandardItem *collectionParent,
                              QStandardItem *aggregateParent,
                              QStandardItem *emitterParent,
                              QStandardItem *primitivesParent,
                              QStandardItem *soundParent);
    void addAnimationItems(QStandardItem *hierarchyParent,
                           QStandardItem *hlodParent,
                           QStandardItem *aggregateParent);
    void loadAppSettings();
    void loadDefaultSettings();
    void applyTexturePath(const QString &path);
    void setTexturePaths(const QString &path1, const QString &path2);
    void reloadLightmapModels();
    void reloadDisplayedObject();
    void updateRecentFilesMenu();
    void addRecentFile(const QString &path);

    QTreeView *_treeView = nullptr;
    QStandardItemModel *_treeModel = nullptr;
    W3DViewport *_viewport = nullptr;
    QMenu *_recentFilesMenu = nullptr;
    QToolBar *_mainToolbar = nullptr;
    QToolBar *_objectToolbar = nullptr;
    QToolBar *_animationToolbar = nullptr;
    QAction *_toolbarMainAction = nullptr;
    QAction *_toolbarObjectAction = nullptr;
    QAction *_toolbarAnimationAction = nullptr;
    QAction *_newAction = nullptr;
    QAction *_openAction = nullptr;
    QAction *_texturePathsAction = nullptr;
    QAction *_loadSettingsAction = nullptr;
    QAction *_saveSettingsAction = nullptr;
    QAction *_enableGammaAction = nullptr;
    QAction *_mungeSortAction = nullptr;
    QAction *_exportAggregateAction = nullptr;
    QAction *_exportEmitterAction = nullptr;
    QAction *_exportLodAction = nullptr;
    QAction *_exportPrimitiveAction = nullptr;
    QAction *_exportSoundObjectAction = nullptr;
    QAction *_editSoundObjectAction = nullptr;
    QAction *_editEmitterAction = nullptr;
    QAction *_scaleEmitterAction = nullptr;
    QAction *_editPrimitiveAction = nullptr;
    QAction *_listMissingTexturesAction = nullptr;
    QAction *_copyAssetsAction = nullptr;
    QAction *_addToLineupAction = nullptr;
    QAction *_aboutAction = nullptr;
    QAction *_wireframeAction = nullptr;
    QAction *_sortingAction = nullptr;
    QAction *_restrictAnimsAction = nullptr;
    QAction *_statusBarAction = nullptr;
    QAction *_fogAction = nullptr;
    QAction *_gammaAction = nullptr;
    QAction *_invertBackfaceCullingAction = nullptr;
    QAction *_backgroundObjectAction = nullptr;
    QAction *_captureScreenshotAction = nullptr;
    QAction *_makeMovieAction = nullptr;
    QAction *_slideshowPrevAction = nullptr;
    QAction *_slideshowNextAction = nullptr;
    QAction *_objectRotateXAction = nullptr;
    QAction *_objectRotateYAction = nullptr;
    QAction *_objectRotateZAction = nullptr;
    QAction *_objectResetAction = nullptr;
    QAction *_objectAlternateAction = nullptr;
    QAction *_objectPropertiesAction = nullptr;
    QAction *_cameraFrontAction = nullptr;
    QAction *_cameraBackAction = nullptr;
    QAction *_cameraLeftAction = nullptr;
    QAction *_cameraRightAction = nullptr;
    QAction *_cameraTopAction = nullptr;
    QAction *_cameraBottomAction = nullptr;
    QAction *_cameraRotateXAction = nullptr;
    QAction *_cameraRotateYAction = nullptr;
    QAction *_cameraRotateZAction = nullptr;
    QAction *_cameraCopyScreenAction = nullptr;
    QAction *_cameraAnimateAction = nullptr;
    QAction *_cameraResetOnDisplayAction = nullptr;
    QAction *_cameraResetAction = nullptr;
    QAction *_cameraBonePosXAction = nullptr;
    QAction *_cameraSettingsAction = nullptr;
    QAction *_cameraDistanceAction = nullptr;
    QActionGroup *_npatchesGroup = nullptr;
    QAction *_npatchesGapAction = nullptr;
    QAction *_lightRotateYAction = nullptr;
    QAction *_lightRotateZAction = nullptr;
    QAction *_exposePrelitAction = nullptr;
    QActionGroup *_prelitGroup = nullptr;
    QAction *_prelitVertexAction = nullptr;
    QAction *_prelitMultipassAction = nullptr;
    QAction *_prelitMultitexAction = nullptr;
    QString _lastOpenedPath;
    QString _texturePath1;
    QString _texturePath2;
    bool _restrictAnims = true;
    bool _sortingEnabled = true;
    bool _animateCamera = false;
    bool _autoResetCamera = true;
};
