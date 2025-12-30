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

#include "MainWindow.h"

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTreeView>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    buildMenus();
    setAcceptDrops(true);
    setWindowTitle(windowTitleForPath(QString()));
    resize(1024, 768);
}

void MainWindow::buildUi()
{
    _treeModel = new QStandardItemModel(this);
    _treeModel->setHorizontalHeaderLabels({QStringLiteral("Chunks")});

    _tableModel = new QStandardItemModel(this);
    _tableModel->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Type"), QStringLiteral("Value")});

    _treeView = new QTreeView(this);
    _treeView->setModel(_treeModel);
    _treeView->header()->setStretchLastSection(true);
    connect(_treeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::onTreeSelectionChanged);

    _tableView = new QTableView(this);
    _tableView->setModel(_tableModel);
    _tableView->horizontalHeader()->setStretchLastSection(true);
    _tableView->verticalHeader()->setVisible(false);

    _hexView = new QPlainTextEdit(this);
    _hexView->setReadOnly(true);

    auto *rightSplit = new QSplitter(Qt::Vertical, this);
    rightSplit->addWidget(_tableView);
    rightSplit->addWidget(_hexView);
    rightSplit->setStretchFactor(0, 3);
    rightSplit->setStretchFactor(1, 2);

    auto *mainSplit = new QSplitter(Qt::Horizontal, this);
    mainSplit->addWidget(_treeView);
    mainSplit->addWidget(rightSplit);
    mainSplit->setStretchFactor(0, 1);
    mainSplit->setStretchFactor(1, 2);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(mainSplit);

    setCentralWidget(central);
}

void MainWindow::buildMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open..."), this, &MainWindow::openFileDialog, QKeySequence::Open);

    _recentMenu = fileMenu->addMenu(tr("Open &Recent"));
    updateRecentFilesMenu();

    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);
}

bool MainWindow::loadFile(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        QMessageBox::warning(this, tr("WDump Qt"), tr("File not found:\n%1").arg(path));
        return false;
    }

    if (!_file.load(path.toStdString())) {
        QMessageBox::warning(this, tr("WDump Qt"), tr("Failed to load file:\n%1").arg(path));
        return false;
    }

    rebuildTree();
    _currentFile = fileInfo.absoluteFilePath();
    setWindowTitle(windowTitleForPath(_currentFile));
    addRecentFile(_currentFile);
    return true;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const auto urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            event->acceptProposedAction();
            return;
        }
    }

    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        if (loadFile(url.toLocalFile())) {
            event->acceptProposedAction();
        }
        return;
    }
}

void MainWindow::openFileDialog()
{
    const QString startDir = _currentFile.isEmpty() ? QString() : QFileInfo(_currentFile).absolutePath();
    const QString filter = tr("W3D Files (*.w3d *.wlt *.wht *.wha *.wtm);;All Files (*.*)");
    const QString filename = QFileDialog::getOpenFileName(this, tr("Open W3D File"), startDir, filter);
    if (filename.isEmpty()) {
        return;
    }
    loadFile(filename);
}

void MainWindow::openRecentFile()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action) {
        return;
    }

    const QString path = action->data().toString();
    if (!loadFile(path)) {
        auto files = recentFiles();
        files.removeAll(path);
        setRecentFiles(files);
        updateRecentFilesMenu();
    }
}

void MainWindow::updateRecentFilesMenu()
{
    if (!_recentMenu) {
        return;
    }

    _recentMenu->clear();
    const auto files = recentFiles();
    if (files.isEmpty()) {
        auto *emptyAction = _recentMenu->addAction(tr("(No recent files)"));
        emptyAction->setEnabled(false);
        return;
    }

    int index = 1;
    for (const QString &path : files) {
        const QFileInfo info(path);
        const QString label = tr("&%1 %2").arg(index++).arg(info.fileName());
        auto *action = _recentMenu->addAction(label, this, &MainWindow::openRecentFile);
        action->setData(path);
        action->setToolTip(path);
    }
}

void MainWindow::addRecentFile(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    auto files = recentFiles();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecentFiles) {
        files.removeLast();
    }
    setRecentFiles(files);
    updateRecentFilesMenu();
}

QStringList MainWindow::recentFiles() const
{
    QSettings settings;
    return settings.value(QStringLiteral("recentFiles")).toStringList();
}

void MainWindow::setRecentFiles(const QStringList &files)
{
    QSettings settings;
    settings.setValue(QStringLiteral("recentFiles"), files);
}

void MainWindow::clearViews()
{
    _treeModel->removeRows(0, _treeModel->rowCount());
    _tableModel->removeRows(0, _tableModel->rowCount());
    _hexView->clear();
}

QString MainWindow::windowTitleForPath(const QString &path) const
{
    if (path.isEmpty()) {
        return QStringLiteral("WDump Qt");
    }

    return QStringLiteral("WDump Qt - %1").arg(QFileInfo(path).fileName());
}

void MainWindow::rebuildTree()
{
    clearViews();

    for (const auto &root : _file.roots())
    {
        addChunkItem(nullptr, *root);
    }
    _treeView->expandAll();
    if (_treeModel->rowCount() > 0) {
        _treeView->setCurrentIndex(_treeModel->index(0, 0));
    }
}

QStandardItem *MainWindow::addChunkItem(QStandardItem *parent, const wdump::Chunk &chunk)
{
    const QString idText = QString::number(chunk.id, 16).toUpper();
    const QString lengthText = QString::number(chunk.length);
    const char *name = wdump::chunk_name(chunk.id);
    QString label;
    if (name) {
        label = QStringLiteral("%1 (0x%2, %3 bytes)")
                    .arg(QString::fromLatin1(name))
                    .arg(idText)
                    .arg(lengthText);
    } else {
        label = QStringLiteral("Chunk 0x%1 (%2 bytes)").arg(idText).arg(lengthText);
    }
    auto *item = new QStandardItem(label);
    item->setData(QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(&chunk)), Qt::UserRole + 1);

    if (parent)
    {
        parent->appendRow(item);
    }
    else
    {
        _treeModel->appendRow(item);
    }

    for (const auto &child : chunk.children)
    {
        addChunkItem(item, *child);
    }

    return item;
}

void MainWindow::onTreeSelectionChanged(const QModelIndex &current)
{
    const auto chunkPtr = reinterpret_cast<const wdump::Chunk *>(current.data(Qt::UserRole + 1).value<quintptr>());
    showChunk(chunkPtr);
}

void MainWindow::showChunk(const wdump::Chunk *chunk)
{
    _tableModel->removeRows(0, _tableModel->rowCount());
    if (!chunk)
    {
        _hexView->clear();
        return;
    }

    auto addRow = [&](const QString &name, const QString &type, const QString &value) {
        QList<QStandardItem *> row;
        row << new QStandardItem(name);
        row << new QStandardItem(type);
        row << new QStandardItem(value);
        _tableModel->appendRow(row);
    };

    addRow(QStringLiteral("ID"), QStringLiteral("uint32"), QString::number(chunk->id));
    const char *name = wdump::chunk_name(chunk->id);
    if (name) {
        addRow(QStringLiteral("Name"), QStringLiteral("string"), QString::fromLatin1(name));
    }
    addRow(QStringLiteral("Length"), QStringLiteral("bytes"), QString::number(chunk->length));

    const auto fields = wdump::describe_chunk(*chunk);
    for (const auto &field : fields) {
        addRow(QString::fromStdString(field.name),
               QString::fromStdString(field.type),
               QString::fromStdString(field.value));
    }

    _hexView->setPlainText(QString::fromStdString(wdump::build_hex_view(*chunk)));
}
