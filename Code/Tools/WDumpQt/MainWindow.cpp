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

#include <QHeaderView>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTreeView>
#include <QVariant>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    setWindowTitle(QStringLiteral("WDump Qt (stub)"));
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

bool MainWindow::loadFile(const QString &path)
{
    if (!_file.load(path.toStdString()))
    {
        return false;
    }

    rebuildTree();
    return true;
}

void MainWindow::rebuildTree()
{
    _treeModel->removeRows(0, _treeModel->rowCount());

    for (const auto &root : _file.roots())
    {
        addChunkItem(nullptr, *root);
    }
    _treeView->expandAll();
}

QStandardItem *MainWindow::addChunkItem(QStandardItem *parent, const wdump::Chunk &chunk)
{
    QString label = QStringLiteral("Chunk 0x%1 (%2 bytes)").arg(QString::number(chunk.id, 16).toUpper()).arg(chunk.length);
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
    addRow(QStringLiteral("Length"), QStringLiteral("bytes"), QString::number(chunk->length));

    _hexView->setPlainText(QString::fromStdString(wdump::build_hex_view(*chunk)));
}
