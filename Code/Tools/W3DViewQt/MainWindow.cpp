#include "MainWindow.h"

#include "W3DViewport.h"

#include <QSplitter>
#include <QStandardItemModel>
#include <QTreeView>

W3DViewMainWindow::W3DViewMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("W3DViewQt");

    auto *splitter = new QSplitter(this);
    splitter->setChildrenCollapsible(false);

    auto *tree_view = new QTreeView(splitter);
    auto *tree_model = new QStandardItemModel(tree_view);
    tree_model->setHorizontalHeaderLabels(QStringList() << "Scene");
    tree_view->setModel(tree_model);

    auto *viewport = new W3DViewport(splitter);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 800});

    setCentralWidget(splitter);
    resize(1200, 800);
}
