#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

class HAnimClass;
class QListWidget;
class QTabWidget;
class QTableWidget;
class W3DViewport;

class AdvancedAnimationDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AdvancedAnimationDialog(W3DViewport *viewport,
                                     const QString &renderObjectName,
                                     QWidget *parent = nullptr);
    ~AdvancedAnimationDialog() override;

protected:
    void accept() override;

private slots:
    void updateReport();
    void onTabChanged(int index);

private:
    void loadAnimations();
    void populateMixingList();
    QString makeChannelString(int boneIndex, HAnimClass *anim) const;

    W3DViewport *_viewport = nullptr;
    QString _renderObjectName;
    QVector<HAnimClass *> _animations;
    QListWidget *_mixingList = nullptr;
    QTableWidget *_reportTable = nullptr;
    QTabWidget *_tabs = nullptr;
    bool _hasHierarchy = false;
    QString _hierarchyName;
};
