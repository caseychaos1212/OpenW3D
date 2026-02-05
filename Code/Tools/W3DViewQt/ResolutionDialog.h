#pragma once

#include <QDialog>

class QCheckBox;
class QTableWidget;

class ResolutionDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ResolutionDialog(QWidget *parent = nullptr);

    int selectedWidth() const;
    int selectedHeight() const;
    int selectedBitsPerPixel() const;
    bool fullscreen() const;

private slots:
    void onDoubleClicked(int row, int column);

private:
    void selectDefaultRow();

    QTableWidget *_table = nullptr;
    QCheckBox *_fullscreenCheck = nullptr;
};
