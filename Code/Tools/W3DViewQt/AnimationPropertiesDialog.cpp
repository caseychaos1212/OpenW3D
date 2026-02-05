#include "AnimationPropertiesDialog.h"

#include "assetmgr.h"
#include "hanim.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
QLabel *MakeValueLabel(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setText("n/a");
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}
}

AnimationPropertiesDialog::AnimationPropertiesDialog(const QString &animationName, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Animation Properties");

    auto *layout = new QVBoxLayout(this);

    _description = new QLabel(this);
    _description->setWordWrap(true);
    _description->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(_description);

    auto *form = new QFormLayout();
    _frameCount = MakeValueLabel(this);
    _frameRate = MakeValueLabel(this);
    _totalTime = MakeValueLabel(this);
    _hierarchyName = MakeValueLabel(this);

    form->addRow("Frame Count:", _frameCount);
    form->addRow("Frame Rate:", _frameRate);
    form->addRow("Total Time:", _totalTime);
    form->addRow("Hierarchy:", _hierarchyName);
    layout->addLayout(form);

    auto *buttons_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons_box);

    if (animationName.isEmpty()) {
        setErrorState("No animation selected.");
        return;
    }

    _description->setText(QString("Animation: %1").arg(animationName));

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        setErrorState("WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = animationName.toLatin1();
    HAnimClass *animation = asset_manager->Get_HAnim(name_bytes.constData());
    if (!animation) {
        setErrorState("Failed to load animation.");
        return;
    }

    _frameCount->setText(QString::number(animation->Get_Num_Frames()));
    _frameRate->setText(QString("%1 fps").arg(animation->Get_Frame_Rate(), 0, 'f', 2));
    _totalTime->setText(QString("%1 seconds").arg(animation->Get_Total_Time(), 0, 'f', 3));

    const char *hier_name = animation->Get_HName();
    if (hier_name) {
        _hierarchyName->setText(QString::fromLatin1(hier_name));
    } else {
        _hierarchyName->setText("");
    }

    animation->Release_Ref();
}

void AnimationPropertiesDialog::setErrorState(const QString &message)
{
    if (_description) {
        _description->setText(message);
    }
    if (_frameCount) {
        _frameCount->setText("n/a");
    }
    if (_frameRate) {
        _frameRate->setText("n/a");
    }
    if (_totalTime) {
        _totalTime->setText("n/a");
    }
    if (_hierarchyName) {
        _hierarchyName->setText("n/a");
    }
}
