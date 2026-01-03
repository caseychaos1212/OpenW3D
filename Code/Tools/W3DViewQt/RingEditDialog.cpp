#include "RingEditDialog.h"

#include "KeyframeTableUtils.h"

#include "assetmgr.h"
#include "ringobj.h"
#include "shader.h"
#include "texture.h"
#include "vector2.h"
#include "vector3.h"

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <optional>

namespace {
struct ShaderPreset {
    const char *label;
    ShaderClass shader;
};

ShaderPreset BuildPreset(const char *label, const ShaderClass &shader)
{
    ShaderPreset preset{label, shader};
    return preset;
}

const ShaderPreset *ShaderPresets(int &count)
{
    static ShaderPreset presets[] = {
        BuildPreset("Additive", ShaderClass::_PresetAdditiveShader),
        BuildPreset("Alpha", ShaderClass::_PresetAlphaShader),
        BuildPreset("Opaque", ShaderClass::_PresetOpaqueShader),
        BuildPreset("Multiplicative", ShaderClass::_PresetMultiplicativeShader),
    };

    count = static_cast<int>(sizeof(presets) / sizeof(presets[0]));
    return presets;
}

bool ShaderMatches(const ShaderClass &a, const ShaderClass &b)
{
    return a.Get_Alpha_Test() == b.Get_Alpha_Test() &&
           a.Get_Dst_Blend_Func() == b.Get_Dst_Blend_Func() &&
           a.Get_Src_Blend_Func() == b.Get_Src_Blend_Func();
}

QDoubleSpinBox *MakeSpin(double min, double max, double value, int decimals, QWidget *parent)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setValue(value);
    return spin;
}

QVector<QVector<double>> SortedRows(const QTableWidget *table)
{
    QVector<QVector<double>> rows = GetKeyframeRows(table);
    std::sort(rows.begin(), rows.end(), [](const QVector<double> &a, const QVector<double> &b) {
        const double time_a = a.isEmpty() ? 0.0 : a[0];
        const double time_b = b.isEmpty() ? 0.0 : b[0];
        return time_a < time_b;
    });
    return rows;
}

std::optional<double> PromptKeyTime(QWidget *parent, const QString &title)
{
    bool ok = false;
    const double time = QInputDialog::getDouble(parent, title, "Time (0-1):", 0.0, 0.0, 1.0, 3, &ok);
    if (!ok) {
        return std::nullopt;
    }
    return time;
}

RingColorChannelClass BuildColorChannel(QTableWidget *table, const Vector3 &fallback)
{
    RingColorChannelClass channel;
    channel.Reset();

    const QVector<QVector<double>> rows = SortedRows(table);
    if (rows.isEmpty()) {
        channel.Add_Key(fallback, 0.0f);
        return channel;
    }

    for (const QVector<double> &row : rows) {
        if (row.size() < 4) {
            continue;
        }
        channel.Add_Key(Vector3(row[1], row[2], row[3]), static_cast<float>(row[0]));
    }

    return channel;
}

RingAlphaChannelClass BuildAlphaChannel(QTableWidget *table, float fallback)
{
    RingAlphaChannelClass channel;
    channel.Reset();

    const QVector<QVector<double>> rows = SortedRows(table);
    if (rows.isEmpty()) {
        channel.Add_Key(fallback, 0.0f);
        return channel;
    }

    for (const QVector<double> &row : rows) {
        if (row.size() < 2) {
            continue;
        }
        channel.Add_Key(static_cast<float>(row[1]), static_cast<float>(row[0]));
    }

    return channel;
}

RingScaleChannelClass BuildScaleChannel(QTableWidget *table, const Vector2 &fallback)
{
    RingScaleChannelClass channel;
    channel.Reset();

    const QVector<QVector<double>> rows = SortedRows(table);
    if (rows.isEmpty()) {
        channel.Add_Key(fallback, 0.0f);
        return channel;
    }

    for (const QVector<double> &row : rows) {
        if (row.size() < 3) {
            continue;
        }
        channel.Add_Key(Vector2(row[1], row[2]), static_cast<float>(row[0]));
    }

    return channel;
}
}

RingEditDialog::RingEditDialog(RingRenderObjClass *ring, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Ring Properties");

    if (ring) {
        _ring = ring;
        _ring->Add_Ref();
    } else {
        _ring = new RingRenderObjClass;
        _ring->Set_Name("Ring");
    }

    if (_ring && _ring->Get_Name()) {
        _oldName = QString::fromLatin1(_ring->Get_Name());
    }

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    auto *general_tab = new QWidget(tabs);
    auto *general_layout = new QFormLayout(general_tab);

    _nameEdit = new QLineEdit(general_tab);
    general_layout->addRow("Name:", _nameEdit);

    auto *texture_row = new QWidget(general_tab);
    auto *texture_layout = new QHBoxLayout(texture_row);
    texture_layout->setContentsMargins(0, 0, 0, 0);
    _textureEdit = new QLineEdit(texture_row);
    auto *browse_button = new QPushButton("Browse...", texture_row);
    connect(browse_button, &QPushButton::clicked, this, &RingEditDialog::browseTexture);
    texture_layout->addWidget(_textureEdit);
    texture_layout->addWidget(browse_button);
    general_layout->addRow("Texture:", texture_row);

    _shaderCombo = new QComboBox(general_tab);
    int preset_count = 0;
    const ShaderPreset *presets = ShaderPresets(preset_count);
    for (int i = 0; i < preset_count; ++i) {
        _shaderCombo->addItem(presets[i].label, i);
    }
    general_layout->addRow("Shader:", _shaderCombo);

    _lifetimeSpin = MakeSpin(0.0, 1000.0, 0.0, 2, general_tab);
    general_layout->addRow("Lifetime:", _lifetimeSpin);

    _tilingSpin = new QSpinBox(general_tab);
    _tilingSpin->setRange(0, 8);
    general_layout->addRow("Texture Tiling:", _tilingSpin);

    auto *flags_row = new QWidget(general_tab);
    auto *flags_layout = new QHBoxLayout(flags_row);
    flags_layout->setContentsMargins(0, 0, 0, 0);
    _cameraAlignCheck = new QCheckBox("Camera Align", flags_row);
    _loopCheck = new QCheckBox("Looping", flags_row);
    flags_layout->addWidget(_cameraAlignCheck);
    flags_layout->addWidget(_loopCheck);
    general_layout->addRow("Flags:", flags_row);

    tabs->addTab(general_tab, "General");

    auto *color_tab = new QWidget(tabs);
    auto *color_layout = new QVBoxLayout(color_tab);

    const QVector<KeyframeColumnSpec> color_specs = {
        {0.0, 1.0, 3},
        {0.0, 1.0, 3},
        {0.0, 1.0, 3},
        {0.0, 1.0, 3},
    };
    auto *color_group = new QGroupBox("Color Keys", color_tab);
    auto *color_group_layout = new QVBoxLayout(color_group);
    _colorKeysTable = CreateKeyframeTable({"Time", "R", "G", "B"}, color_specs, color_group);
    color_group_layout->addWidget(_colorKeysTable);
    auto *color_buttons = new QHBoxLayout();
    auto *color_add = new QPushButton("Add", color_group);
    auto *color_remove = new QPushButton("Remove", color_group);
    auto *color_sort = new QPushButton("Sort", color_group);
    color_buttons->addWidget(color_add);
    color_buttons->addWidget(color_remove);
    color_buttons->addWidget(color_sort);
    color_group_layout->addLayout(color_buttons);
    color_layout->addWidget(color_group);

    const QVector<KeyframeColumnSpec> alpha_specs = {
        {0.0, 1.0, 3},
        {0.0, 1.0, 3},
    };
    auto *alpha_group = new QGroupBox("Opacity Keys", color_tab);
    auto *alpha_group_layout = new QVBoxLayout(alpha_group);
    _alphaKeysTable = CreateKeyframeTable({"Time", "Alpha"}, alpha_specs, alpha_group);
    alpha_group_layout->addWidget(_alphaKeysTable);
    auto *alpha_buttons = new QHBoxLayout();
    auto *alpha_add = new QPushButton("Add", alpha_group);
    auto *alpha_remove = new QPushButton("Remove", alpha_group);
    auto *alpha_sort = new QPushButton("Sort", alpha_group);
    alpha_buttons->addWidget(alpha_add);
    alpha_buttons->addWidget(alpha_remove);
    alpha_buttons->addWidget(alpha_sort);
    alpha_group_layout->addLayout(alpha_buttons);
    color_layout->addWidget(alpha_group);

    tabs->addTab(color_tab, "Color");

    auto *size_tab = new QWidget(tabs);
    auto *size_layout = new QVBoxLayout(size_tab);
    auto *extent_form = new QFormLayout();
    _innerXSpin = MakeSpin(0.0, 10000.0, 0.0, 2, size_tab);
    _innerYSpin = MakeSpin(0.0, 10000.0, 0.0, 2, size_tab);
    _outerXSpin = MakeSpin(0.0, 10000.0, 0.0, 2, size_tab);
    _outerYSpin = MakeSpin(0.0, 10000.0, 0.0, 2, size_tab);
    extent_form->addRow("Inner Extent X:", _innerXSpin);
    extent_form->addRow("Inner Extent Y:", _innerYSpin);
    extent_form->addRow("Outer Extent X:", _outerXSpin);
    extent_form->addRow("Outer Extent Y:", _outerYSpin);
    size_layout->addLayout(extent_form);

    const QVector<KeyframeColumnSpec> scale_specs = {
        {0.0, 1.0, 3},
        {0.0, 10000.0, 3},
        {0.0, 10000.0, 3},
    };
    auto *inner_group = new QGroupBox("Inner Scale Keys", size_tab);
    auto *inner_layout = new QVBoxLayout(inner_group);
    _innerScaleTable = CreateKeyframeTable({"Time", "X", "Y"}, scale_specs, inner_group);
    inner_layout->addWidget(_innerScaleTable);
    auto *inner_buttons = new QHBoxLayout();
    auto *inner_add = new QPushButton("Add", inner_group);
    auto *inner_remove = new QPushButton("Remove", inner_group);
    auto *inner_sort = new QPushButton("Sort", inner_group);
    inner_buttons->addWidget(inner_add);
    inner_buttons->addWidget(inner_remove);
    inner_buttons->addWidget(inner_sort);
    inner_layout->addLayout(inner_buttons);
    size_layout->addWidget(inner_group);

    auto *outer_group = new QGroupBox("Outer Scale Keys", size_tab);
    auto *outer_layout = new QVBoxLayout(outer_group);
    _outerScaleTable = CreateKeyframeTable({"Time", "X", "Y"}, scale_specs, outer_group);
    outer_layout->addWidget(_outerScaleTable);
    auto *outer_buttons = new QHBoxLayout();
    auto *outer_add = new QPushButton("Add", outer_group);
    auto *outer_remove = new QPushButton("Remove", outer_group);
    auto *outer_sort = new QPushButton("Sort", outer_group);
    outer_buttons->addWidget(outer_add);
    outer_buttons->addWidget(outer_remove);
    outer_buttons->addWidget(outer_sort);
    outer_layout->addLayout(outer_buttons);
    size_layout->addWidget(outer_group);

    tabs->addTab(size_tab, "Size");

    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &RingEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(color_add, &QPushButton::clicked, this, [this, color_specs]() {
        const auto time = PromptKeyTime(this, "Add Color Key");
        if (!time) {
            return;
        }
        RingColorChannelClass channel = BuildColorChannel(_colorKeysTable, _ring->Get_Color());
        const Vector3 value = channel.Evaluate(static_cast<float>(*time));
        AddKeyframeRow(_colorKeysTable, {*time, value.X, value.Y, value.Z}, color_specs);
        SortKeyframeRows(_colorKeysTable, color_specs);
    });
    connect(color_remove, &QPushButton::clicked, this, [this]() {
        RemoveSelectedKeyframeRows(_colorKeysTable);
    });
    connect(color_sort, &QPushButton::clicked, this, [this, color_specs]() {
        SortKeyframeRows(_colorKeysTable, color_specs);
    });

    connect(alpha_add, &QPushButton::clicked, this, [this, alpha_specs]() {
        const auto time = PromptKeyTime(this, "Add Opacity Key");
        if (!time) {
            return;
        }
        RingAlphaChannelClass channel = BuildAlphaChannel(_alphaKeysTable, _ring->Get_Alpha());
        const float value = channel.Evaluate(static_cast<float>(*time));
        AddKeyframeRow(_alphaKeysTable, {*time, value}, alpha_specs);
        SortKeyframeRows(_alphaKeysTable, alpha_specs);
    });
    connect(alpha_remove, &QPushButton::clicked, this, [this]() {
        RemoveSelectedKeyframeRows(_alphaKeysTable);
    });
    connect(alpha_sort, &QPushButton::clicked, this, [this, alpha_specs]() {
        SortKeyframeRows(_alphaKeysTable, alpha_specs);
    });

    connect(inner_add, &QPushButton::clicked, this, [this, scale_specs]() {
        const auto time = PromptKeyTime(this, "Add Inner Scale Key");
        if (!time) {
            return;
        }
        RingScaleChannelClass channel = BuildScaleChannel(_innerScaleTable, _ring->Get_Inner_Scale());
        const Vector2 value = channel.Evaluate(static_cast<float>(*time));
        AddKeyframeRow(_innerScaleTable, {*time, value.X, value.Y}, scale_specs);
        SortKeyframeRows(_innerScaleTable, scale_specs);
    });
    connect(inner_remove, &QPushButton::clicked, this, [this]() {
        RemoveSelectedKeyframeRows(_innerScaleTable);
    });
    connect(inner_sort, &QPushButton::clicked, this, [this, scale_specs]() {
        SortKeyframeRows(_innerScaleTable, scale_specs);
    });

    connect(outer_add, &QPushButton::clicked, this, [this, scale_specs]() {
        const auto time = PromptKeyTime(this, "Add Outer Scale Key");
        if (!time) {
            return;
        }
        RingScaleChannelClass channel = BuildScaleChannel(_outerScaleTable, _ring->Get_Outer_Scale());
        const Vector2 value = channel.Evaluate(static_cast<float>(*time));
        AddKeyframeRow(_outerScaleTable, {*time, value.X, value.Y}, scale_specs);
        SortKeyframeRows(_outerScaleTable, scale_specs);
    });
    connect(outer_remove, &QPushButton::clicked, this, [this]() {
        RemoveSelectedKeyframeRows(_outerScaleTable);
    });
    connect(outer_sort, &QPushButton::clicked, this, [this, scale_specs]() {
        SortKeyframeRows(_outerScaleTable, scale_specs);
    });

    loadFromRing();
}

RingEditDialog::~RingEditDialog()
{
    if (_ring) {
        _ring->Release_Ref();
        _ring = nullptr;
    }
}

RingRenderObjClass *RingEditDialog::ring() const
{
    if (_ring) {
        _ring->Add_Ref();
    }
    return _ring;
}

QString RingEditDialog::oldName() const
{
    return _oldName;
}

void RingEditDialog::accept()
{
    if (!_ring) {
        QDialog::reject();
        return;
    }

    const QString name = _nameEdit ? _nameEdit->text().trimmed() : QString();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Ring", "Invalid ring name. Please enter a new name.");
        return;
    }

    TextureClass *texture = nullptr;
    const QString texture_path = _textureEdit ? _textureEdit->text().trimmed() : QString();
    if (!texture_path.isEmpty()) {
        const QString file_name_only = QFileInfo(texture_path).fileName();
        if (file_name_only.isEmpty()) {
            QMessageBox::warning(this, "Ring", "Invalid texture filename.");
            return;
        }

        auto *asset_manager = WW3DAssetManager::Get_Instance();
        if (!asset_manager) {
            QMessageBox::warning(this, "Ring", "WW3D asset manager is not available.");
            return;
        }

        const QByteArray texture_bytes = file_name_only.toLatin1();
        texture = asset_manager->Get_Texture(texture_bytes.constData());
    }

    _ring->Set_Texture(texture);
    if (texture) {
        texture->Release_Ref();
    }

    int preset_count = 0;
    const ShaderPreset *presets = ShaderPresets(preset_count);
    const int shader_index = _shaderCombo ? _shaderCombo->currentIndex() : -1;
    if (shader_index >= 0 && shader_index < preset_count) {
        ShaderClass shader = presets[shader_index].shader;
        _ring->Set_Shader(shader);
    }

    const float lifetime = _lifetimeSpin ? static_cast<float>(_lifetimeSpin->value()) : 0.0f;
    _ring->Set_Animation_Duration(lifetime);

    if (_tilingSpin) {
        _ring->Set_Texture_Tiling(_tilingSpin->value());
    }

    if (_cameraAlignCheck) {
        _ring->Set_Flag(RingRenderObjClass::USE_CAMERA_ALIGN, _cameraAlignCheck->isChecked());
    }
    if (_loopCheck) {
        _ring->Set_Flag(RingRenderObjClass::USE_ANIMATION_LOOP, _loopCheck->isChecked());
    }

    const RingColorChannelClass color_channel = BuildColorChannel(_colorKeysTable, _ring->Get_Color());
    const RingAlphaChannelClass alpha_channel = BuildAlphaChannel(_alphaKeysTable, _ring->Get_Alpha());
    _ring->Set_Color_Channel(color_channel);
    _ring->Set_Alpha_Channel(alpha_channel);

    const float inner_x = _innerXSpin ? static_cast<float>(_innerXSpin->value()) : 0.0f;
    const float inner_y = _innerYSpin ? static_cast<float>(_innerYSpin->value()) : 0.0f;
    const float outer_x = _outerXSpin ? static_cast<float>(_outerXSpin->value()) : 0.0f;
    const float outer_y = _outerYSpin ? static_cast<float>(_outerYSpin->value()) : 0.0f;
    _ring->Set_Inner_Extent(Vector2(inner_x, inner_y));
    _ring->Set_Outer_Extent(Vector2(outer_x, outer_y));

    const RingScaleChannelClass inner_scale = BuildScaleChannel(_innerScaleTable, _ring->Get_Inner_Scale());
    const RingScaleChannelClass outer_scale = BuildScaleChannel(_outerScaleTable, _ring->Get_Outer_Scale());
    _ring->Set_Inner_Scale_Channel(inner_scale);
    _ring->Set_Outer_Scale_Channel(outer_scale);

    const QByteArray name_bytes = name.toLatin1();
    _ring->Set_Name(name_bytes.constData());
    _ring->Restart_Animation();

    QDialog::accept();
}

void RingEditDialog::browseTexture()
{
    const QString start = _textureEdit ? _textureEdit->text() : QString();
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Select Texture",
        start,
        "Texture Files (*.tga);;All Files (*.*)");
    if (!path.isEmpty() && _textureEdit) {
        _textureEdit->setText(path);
    }
}

void RingEditDialog::loadFromRing()
{
    if (!_ring) {
        return;
    }

    if (_nameEdit) {
        const char *name = _ring->Get_Name();
        _nameEdit->setText(name ? QString::fromLatin1(name) : QString());
    }

    if (_textureEdit) {
        TextureClass *texture = _ring->Peek_Texture();
        if (texture) {
            const StringClass &name = texture->Get_Texture_Name();
            const QByteArray name_bytes(name.Peek_Buffer(), static_cast<int>(name.Get_Length()));
            _textureEdit->setText(QString::fromLatin1(name_bytes));
        }
    }

    if (_lifetimeSpin) {
        _lifetimeSpin->setValue(_ring->Get_Animation_Duration());
    }

    if (_shaderCombo) {
        _shaderCombo->setCurrentIndex(findShaderIndex());
    }

    if (_tilingSpin) {
        _tilingSpin->setValue(_ring->Get_Texture_Tiling());
    }

    const unsigned int flags = _ring->Get_Flags();
    if (_cameraAlignCheck) {
        _cameraAlignCheck->setChecked((flags & RingRenderObjClass::USE_CAMERA_ALIGN) != 0);
    }
    if (_loopCheck) {
        _loopCheck->setChecked((flags & RingRenderObjClass::USE_ANIMATION_LOOP) != 0);
    }

    if (_innerXSpin && _innerYSpin) {
        const Vector2 inner = _ring->Get_Inner_Extent();
        _innerXSpin->setValue(inner.X);
        _innerYSpin->setValue(inner.Y);
    }

    if (_outerXSpin && _outerYSpin) {
        const Vector2 outer = _ring->Get_Outer_Extent();
        _outerXSpin->setValue(outer.X);
        _outerYSpin->setValue(outer.Y);
    }

    RingColorChannelClass color_channel = _ring->Get_Color_Channel();
    if (color_channel.Get_Key_Count() == 0) {
        color_channel.Add_Key(_ring->Get_Color(), 0.0f);
    }
    QVector<QVector<double>> color_rows;
    for (int i = 0; i < color_channel.Get_Key_Count(); ++i) {
        const auto &key = color_channel.Get_Key(i);
        const Vector3 value = key.Get_Value();
        color_rows.push_back({key.Get_Time(), value.X, value.Y, value.Z});
    }
    SetKeyframeRows(_colorKeysTable,
                    color_rows,
                    QVector<KeyframeColumnSpec>{{0.0, 1.0, 3},
                                                {0.0, 1.0, 3},
                                                {0.0, 1.0, 3},
                                                {0.0, 1.0, 3}});

    RingAlphaChannelClass alpha_channel = _ring->Get_Alpha_Channel();
    if (alpha_channel.Get_Key_Count() == 0) {
        alpha_channel.Add_Key(_ring->Get_Alpha(), 0.0f);
    }
    QVector<QVector<double>> alpha_rows;
    for (int i = 0; i < alpha_channel.Get_Key_Count(); ++i) {
        const auto &key = alpha_channel.Get_Key(i);
        alpha_rows.push_back({key.Get_Time(), key.Get_Value()});
    }
    SetKeyframeRows(_alphaKeysTable,
                    alpha_rows,
                    QVector<KeyframeColumnSpec>{{0.0, 1.0, 3}, {0.0, 1.0, 3}});

    RingScaleChannelClass inner_scale = _ring->Get_Inner_Scale_Channel();
    if (inner_scale.Get_Key_Count() == 0) {
        inner_scale.Add_Key(_ring->Get_Inner_Scale(), 0.0f);
    }
    QVector<QVector<double>> inner_rows;
    for (int i = 0; i < inner_scale.Get_Key_Count(); ++i) {
        const auto &key = inner_scale.Get_Key(i);
        const Vector2 value = key.Get_Value();
        inner_rows.push_back({key.Get_Time(), value.X, value.Y});
    }
    SetKeyframeRows(_innerScaleTable,
                    inner_rows,
                    QVector<KeyframeColumnSpec>{{0.0, 1.0, 3},
                                                {0.0, 10000.0, 3},
                                                {0.0, 10000.0, 3}});

    RingScaleChannelClass outer_scale = _ring->Get_Outer_Scale_Channel();
    if (outer_scale.Get_Key_Count() == 0) {
        outer_scale.Add_Key(_ring->Get_Outer_Scale(), 0.0f);
    }
    QVector<QVector<double>> outer_rows;
    for (int i = 0; i < outer_scale.Get_Key_Count(); ++i) {
        const auto &key = outer_scale.Get_Key(i);
        const Vector2 value = key.Get_Value();
        outer_rows.push_back({key.Get_Time(), value.X, value.Y});
    }
    SetKeyframeRows(_outerScaleTable,
                    outer_rows,
                    QVector<KeyframeColumnSpec>{{0.0, 1.0, 3},
                                                {0.0, 10000.0, 3},
                                                {0.0, 10000.0, 3}});
}

int RingEditDialog::findShaderIndex() const
{
    if (!_ring) {
        return 0;
    }

    int preset_count = 0;
    const ShaderPreset *presets = ShaderPresets(preset_count);
    const ShaderClass &shader = _ring->Get_Shader();
    for (int index = 0; index < preset_count; ++index) {
        if (ShaderMatches(presets[index].shader, shader)) {
            return index;
        }
    }

    return 0;
}
