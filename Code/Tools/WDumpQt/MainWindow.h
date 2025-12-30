/*
**  Command & Conquer Renegade(tm)
**  Copyright 2025 Electronic Arts Inc.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <memory>
#include <QMainWindow>
#include <QStringList>

#include "wdump_core.h"

class QAction;
class QDragEnterEvent;
class QDropEvent;
class QMenu;
class QPlainTextEdit;
class QSplitter;
class QStandardItem;
class QTableView;
class QTreeView;
class QStandardItemModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    bool loadFile(const QString &path);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void buildUi();
    void buildMenus();
    void openFileDialog();
    void openRecentFile();
    void updateRecentFilesMenu();
    void addRecentFile(const QString &path);
    QStringList recentFiles() const;
    void setRecentFiles(const QStringList &files);
    void clearViews();
    QString windowTitleForPath(const QString &path) const;
    void rebuildTree();
    void onTreeSelectionChanged(const QModelIndex &current);
    QStandardItem *addChunkItem(QStandardItem *parent, const wdump::Chunk &chunk);
    void showChunk(const wdump::Chunk *chunk);

    static constexpr int kMaxRecentFiles = 10;

    wdump::ChunkFile _file;
    QString _currentFile;
    QTreeView *_treeView = nullptr;
    QTableView *_tableView = nullptr;
    QPlainTextEdit *_hexView = nullptr;
    QStandardItemModel *_treeModel = nullptr;
    QStandardItemModel *_tableModel = nullptr;
    QMenu *_recentMenu = nullptr;
};
