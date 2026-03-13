#pragma once

#include "RuntimeSession.h"

#include <cstdint>
#include <vector>

#include <QWidget>
#include <QtGlobal>

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace leveledit_qt {

class PresetsPanel final : public QWidget
{
    Q_OBJECT

public:
    struct RowData
    {
        std::uint32_t id = 0;
        std::uint32_t class_id = 0;
        std::uint32_t parent_id = 0;
        bool temporary = false;
        QString name;
    };

    explicit PresetsPanel(QWidget *parent = nullptr);
    void setPresets(const std::vector<PresetRecord> &records,
                    const QString &source,
                    const QString &error = QString());

signals:
    void presetActivated(quint32 id,
                         const QString &name,
                         quint32 class_id,
                         quint32 parent_id,
                         bool temporary);
    void presetOpenRequested(quint32 id,
                             const QString &name,
                             quint32 class_id,
                             quint32 parent_id,
                             bool temporary);

private:
    void rebuildTree();
    void emitPresetActivatedForItem(QTreeWidgetItem *item);
    void emitPresetOpenForItem(QTreeWidgetItem *item);
    bool isPresetRowItem(QTreeWidgetItem *item) const;

    QLineEdit *_filter = nullptr;
    QLabel *_summary = nullptr;
    QTreeWidget *_tree = nullptr;
    QPushButton *_modButton = nullptr;
    std::vector<RowData> _rows;
    QString _baseSummaryText;
};

} // namespace leveledit_qt
