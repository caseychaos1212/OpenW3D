#pragma once

#include <QDialog>

class QComboBox;
class W3DViewport;

class AddToLineupDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AddToLineupDialog(W3DViewport *viewport, QWidget *parent = nullptr);
    QString selectedName() const;

protected:
    void accept() override;

private:
    void populateObjects();

    W3DViewport *_viewport = nullptr;
    QComboBox *_combo = nullptr;
};
