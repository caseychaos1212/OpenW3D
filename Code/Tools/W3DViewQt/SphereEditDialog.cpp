#include "SphereEditDialog.h"

#include "KeyframeTableUtils.h"
#include "OpacityVectorEditDialog.h"

#include "aabox.h"
#include "assetmgr.h"
#include "euler.h"
#include "quat.h"
#include "shader.h"
#include "sphereobj.h"
#include "texture.h"
#include "vector3.h"
#include "wwmath.h"

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
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <optional>

namespace {
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

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

AlphaVectorStruct BuildAlphaVector(float intensity, float y_deg, float z_deg)
{
    Matrix3 rot_mat(true);
    rot_mat.Rotate_Y(y_deg * kDegToRad);
    rot_mat.Rotate_Z(z_deg * kDegToRad);

    AlphaVectorStruct value;
    value.intensity = intensity;
    value.angle = Build_Quaternion(rot_mat);
    return value;
}

void AlphaVectorAngles(const AlphaVectorStruct &value, float &y_deg, float &z_deg)
{
    Matrix3D rotation = Build_Matrix3D(value.angle);
    EulerAnglesClass euler(rotation, EulerOrderXYZr);
    y_deg = static_cast<float>(euler.Get_Angle(1) * kRadToDeg);
    z_deg = static_cast<float>(euler.Get_Angle(2) * kRadToDeg);
    y_deg = static_cast<float>(WWMath::Wrap(y_deg, 0.0f, 360.0f));
    z_deg = static_cast<float>(WWMath::Wrap(z_deg, 0.0f, 360.0f));
}

SphereColorChannelClass BuildColorChannel(QTableWidget *table, const Vector3 &fallback)
{
    SphereColorChannelClass channel;
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

SphereAlphaChannelClass BuildAlphaChannel(QTableWidget *table, float fallback)
{
    SphereAlphaChannelClass channel;
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

SphereVectorChannelClass BuildVectorChannel(QTableWidget *table, const AlphaVectorStruct &fallback)
{
    SphereVectorChannelClass channel;
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
        const float intensity = static_cast<float>(row[1]);
        const float y_deg = static_cast<float>(row[2]);
        const float z_deg = static_cast<float>(row[3]);
        channel.Add_Key(BuildAlphaVector(intensity, y_deg, z_deg), static_cast<float>(row[0]));
    }

    return channel;
}

SphereScaleChannelClass BuildScaleChannel(QTableWidget *table, const Vector3 &fallback)
{
    SphereScaleChannelClass channel;
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
}

SphereEditDialog::SphereEditDialog(SphereRenderObjClass *sphere, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Sphere Properties");

    if (sphere) {
        _sphere = sphere;
        _sphere->Add_Ref();
    } else {
        _sphere = new SphereRenderObjClass;
        _sphere->Set_Name("Sphere");
    }

    if (_sphere && _sphere->Get_Name()) {
        _oldName = QString::fromLatin1(_sphere->Get_Name());
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
    connect(browse_button, &QPushButton::clicked, this, &SphereEditDialog::browseTexture);
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

    const QVector<KeyframeColumnSpec> vector_specs = {
        {0.0, 1.0, 3},
        {0.0, 10.0, 2},
        {0.0, 179.0, 0},
        {0.0, 179.0, 0},
    };
    auto *vector_group = new QGroupBox("Opacity Vector Keys", color_tab);
    auto *vector_group_layout = new QVBoxLayout(vector_group);
    auto *vector_flags = new QWidget(vector_group);
    auto *vector_flags_layout = new QHBoxLayout(vector_flags);
    vector_flags_layout->setContentsMargins(0, 0, 0, 0);
    _useVectorCheck = new QCheckBox("Use Opacity Vector", vector_flags);
    _invertVectorCheck = new QCheckBox("Invert", vector_flags);
    vector_flags_layout->addWidget(_useVectorCheck);
    vector_flags_layout->addWidget(_invertVectorCheck);
    vector_group_layout->addWidget(vector_flags);

    _vectorKeysTable = CreateKeyframeTable({"Time", "Intensity", "Y", "Z"}, vector_specs, vector_group);
    vector_group_layout->addWidget(_vectorKeysTable);
    auto *vector_buttons = new QHBoxLayout();
    auto *vector_add = new QPushButton("Add", vector_group);
    auto *vector_edit = new QPushButton("Edit", vector_group);
    auto *vector_remove = new QPushButton("Remove", vector_group);
    auto *vector_sort = new QPushButton("Sort", vector_group);
    vector_buttons->addWidget(vector_add);
    vector_buttons->addWidget(vector_edit);
    vector_buttons->addWidget(vector_remove);
    vector_buttons->addWidget(vector_sort);
    vector_group_layout->addLayout(vector_buttons);
    color_layout->addWidget(vector_group);

    tabs->addTab(color_tab, "Color");

    auto *size_tab = new QWidget(tabs);
    auto *size_layout = new QVBoxLayout(size_tab);
    auto *extent_form = new QFormLayout();
    _sizeXSpin = MakeSpin(0.0, 10000.0, 0.0, 2, size_tab);
    _sizeYSpin = MakeSpin(0.0, 10000.0, 0.0, 2, size_tab);
    _sizeZSpin = MakeSpin(0.0, 10000.0, 0.0, 2, size_tab);
    extent_form->addRow("Extent X:", _sizeXSpin);
    extent_form->addRow("Extent Y:", _sizeYSpin);
    extent_form->addRow("Extent Z:", _sizeZSpin);
    size_layout->addLayout(extent_form);

    const QVector<KeyframeColumnSpec> scale_specs = {
        {0.0, 1.0, 3},
        {0.0, 10000.0, 3},
        {0.0, 10000.0, 3},
        {0.0, 10000.0, 3},
    };
    auto *scale_group = new QGroupBox("Scale Keys", size_tab);
    auto *scale_group_layout = new QVBoxLayout(scale_group);
    _scaleKeysTable = CreateKeyframeTable({"Time", "X", "Y", "Z"}, scale_specs, scale_group);
    scale_group_layout->addWidget(_scaleKeysTable);
    auto *scale_buttons = new QHBoxLayout();
    auto *scale_add = new QPushButton("Add", scale_group);
    auto *scale_remove = new QPushButton("Remove", scale_group);
    auto *scale_sort = new QPushButton("Sort", scale_group);
    scale_buttons->addWidget(scale_add);
    scale_buttons->addWidget(scale_remove);
    scale_buttons->addWidget(scale_sort);
    scale_group_layout->addLayout(scale_buttons);
    size_layout->addWidget(scale_group);

    tabs->addTab(size_tab, "Size");

    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SphereEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(_useVectorCheck, &QCheckBox::toggled, this, [this]() {
        const bool enabled = _useVectorCheck && _useVectorCheck->isChecked();
        if (_vectorKeysTable) {
            _vectorKeysTable->setEnabled(enabled);
        }
        if (_invertVectorCheck) {
            _invertVectorCheck->setEnabled(enabled);
        }
    });

    connect(color_add, &QPushButton::clicked, this, [this, color_specs]() {
        const auto time = PromptKeyTime(this, "Add Color Key");
        if (!time) {
            return;
        }
        SphereColorChannelClass channel = BuildColorChannel(_colorKeysTable, _sphere->Get_Color());
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
        SphereAlphaChannelClass channel = BuildAlphaChannel(_alphaKeysTable, _sphere->Get_Alpha());
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

    connect(vector_add, &QPushButton::clicked, this, [this, vector_specs]() {
        const auto time = PromptKeyTime(this, "Add Opacity Vector Key");
        if (!time) {
            return;
        }
        SphereVectorChannelClass channel = BuildVectorChannel(_vectorKeysTable, _sphere->Get_Vector());
        const AlphaVectorStruct value = channel.Evaluate(static_cast<float>(*time));
        float y_deg = 0.0f;
        float z_deg = 0.0f;
        AlphaVectorAngles(value, y_deg, z_deg);
        AddKeyframeRow(_vectorKeysTable, {*time, value.intensity, y_deg, z_deg}, vector_specs);
        SortKeyframeRows(_vectorKeysTable, vector_specs);
    });
    connect(vector_edit, &QPushButton::clicked, this, [this, vector_specs]() {
        if (!_vectorKeysTable) {
            return;
        }
        const int row = _vectorKeysTable->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Opacity Vector", "Select a vector key to edit.");
            return;
        }
        const QVector<QVector<double>> rows = GetKeyframeRows(_vectorKeysTable);
        if (row >= rows.size() || rows[row].size() < 4) {
            return;
        }
        const double intensity = rows[row][1];
        const double y_deg = rows[row][2];
        const double z_deg = rows[row][3];
        OpacityVectorEditDialog dialog(BuildAlphaVector(static_cast<float>(intensity),
                                                        static_cast<float>(y_deg),
                                                        static_cast<float>(z_deg)),
                                       this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        const AlphaVectorStruct updated = dialog.value();
        float new_y = 0.0f;
        float new_z = 0.0f;
        AlphaVectorAngles(updated, new_y, new_z);
        if (auto *spin = qobject_cast<QDoubleSpinBox *>(_vectorKeysTable->cellWidget(row, 1))) {
            spin->setValue(updated.intensity);
        }
        if (auto *spin = qobject_cast<QDoubleSpinBox *>(_vectorKeysTable->cellWidget(row, 2))) {
            spin->setValue(new_y);
        }
        if (auto *spin = qobject_cast<QDoubleSpinBox *>(_vectorKeysTable->cellWidget(row, 3))) {
            spin->setValue(new_z);
        }
    });
    connect(vector_remove, &QPushButton::clicked, this, [this]() {
        RemoveSelectedKeyframeRows(_vectorKeysTable);
    });
    connect(vector_sort, &QPushButton::clicked, this, [this, vector_specs]() {
        SortKeyframeRows(_vectorKeysTable, vector_specs);
    });

    connect(scale_add, &QPushButton::clicked, this, [this, scale_specs]() {
        const auto time = PromptKeyTime(this, "Add Scale Key");
        if (!time) {
            return;
        }
        SphereScaleChannelClass channel = BuildScaleChannel(_scaleKeysTable, _sphere->Get_Scale());
        const Vector3 value = channel.Evaluate(static_cast<float>(*time));
        AddKeyframeRow(_scaleKeysTable, {*time, value.X, value.Y, value.Z}, scale_specs);
        SortKeyframeRows(_scaleKeysTable, scale_specs);
    });
    connect(scale_remove, &QPushButton::clicked, this, [this]() {
        RemoveSelectedKeyframeRows(_scaleKeysTable);
    });
    connect(scale_sort, &QPushButton::clicked, this, [this, scale_specs]() {
        SortKeyframeRows(_scaleKeysTable, scale_specs);
    });

    loadFromSphere();
}

SphereEditDialog::~SphereEditDialog()
{
    if (_sphere) {
        _sphere->Release_Ref();
        _sphere = nullptr;
    }
}

SphereRenderObjClass *SphereEditDialog::sphere() const
{
    if (_sphere) {
        _sphere->Add_Ref();
    }
    return _sphere;
}

QString SphereEditDialog::oldName() const
{
    return _oldName;
}

void SphereEditDialog::accept()
{
    if (!_sphere) {
        QDialog::reject();
        return;
    }

    const QString name = _nameEdit ? _nameEdit->text().trimmed() : QString();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Sphere", "Invalid sphere name. Please enter a new name.");
        return;
    }

    TextureClass *texture = nullptr;
    const QString texture_path = _textureEdit ? _textureEdit->text().trimmed() : QString();
    if (!texture_path.isEmpty()) {
        const QString file_name_only = QFileInfo(texture_path).fileName();
        if (file_name_only.isEmpty()) {
            QMessageBox::warning(this, "Sphere", "Invalid texture filename.");
            return;
        }

        auto *asset_manager = WW3DAssetManager::Get_Instance();
        if (!asset_manager) {
            QMessageBox::warning(this, "Sphere", "WW3D asset manager is not available.");
            return;
        }

        const QByteArray texture_bytes = file_name_only.toLatin1();
        texture = asset_manager->Get_Texture(texture_bytes.constData());
    }

    _sphere->Set_Texture(texture);
    if (texture) {
        texture->Release_Ref();
    }

    int preset_count = 0;
    const ShaderPreset *presets = ShaderPresets(preset_count);
    const int shader_index = _shaderCombo ? _shaderCombo->currentIndex() : -1;
    if (shader_index >= 0 && shader_index < preset_count) {
        ShaderClass shader = presets[shader_index].shader;
        _sphere->Set_Shader(shader);
    }

    const float lifetime = _lifetimeSpin ? static_cast<float>(_lifetimeSpin->value()) : 0.0f;
    _sphere->Set_Animation_Duration(lifetime);

    if (_cameraAlignCheck) {
        _sphere->Set_Flag(SphereRenderObjClass::USE_CAMERA_ALIGN, _cameraAlignCheck->isChecked());
    }
    if (_loopCheck) {
        _sphere->Set_Flag(SphereRenderObjClass::USE_ANIMATION_LOOP, _loopCheck->isChecked());
    }

    const SphereColorChannelClass color_channel = BuildColorChannel(_colorKeysTable, _sphere->Get_Color());
    const SphereAlphaChannelClass alpha_channel = BuildAlphaChannel(_alphaKeysTable, _sphere->Get_Alpha());
    const SphereVectorChannelClass vector_channel = BuildVectorChannel(_vectorKeysTable, _sphere->Get_Vector());
    _sphere->Set_Color_Channel(color_channel);
    _sphere->Set_Alpha_Channel(alpha_channel);
    _sphere->Set_Vector_Channel(vector_channel);

    if (_useVectorCheck) {
        _sphere->Set_Flag(SphereRenderObjClass::USE_ALPHA_VECTOR, _useVectorCheck->isChecked());
    }
    if (_invertVectorCheck) {
        _sphere->Set_Flag(SphereRenderObjClass::USE_INVERSE_ALPHA, _invertVectorCheck->isChecked());
    }

    const float x = _sizeXSpin ? static_cast<float>(_sizeXSpin->value()) : 0.0f;
    const float y = _sizeYSpin ? static_cast<float>(_sizeYSpin->value()) : 0.0f;
    const float z = _sizeZSpin ? static_cast<float>(_sizeZSpin->value()) : 0.0f;
    _sphere->Set_Extent(Vector3(x, y, z));

    const SphereScaleChannelClass scale_channel = BuildScaleChannel(_scaleKeysTable, _sphere->Get_Scale());
    _sphere->Set_Scale_Channel(scale_channel);

    const QByteArray name_bytes = name.toLatin1();
    _sphere->Set_Name(name_bytes.constData());
    _sphere->Restart_Animation();

    QDialog::accept();
}

void SphereEditDialog::browseTexture()
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

void SphereEditDialog::loadFromSphere()
{
    if (!_sphere) {
        return;
    }

    if (_nameEdit) {
        const char *name = _sphere->Get_Name();
        _nameEdit->setText(name ? QString::fromLatin1(name) : QString());
    }

    if (_textureEdit) {
        TextureClass *texture = _sphere->Peek_Texture();
        if (texture) {
            const StringClass &name = texture->Get_Texture_Name();
            const QByteArray name_bytes(name.Peek_Buffer(), static_cast<int>(name.Get_Length()));
            _textureEdit->setText(QString::fromLatin1(name_bytes));
        }
    }

    if (_lifetimeSpin) {
        _lifetimeSpin->setValue(_sphere->Get_Animation_Duration());
    }

    if (_shaderCombo) {
        _shaderCombo->setCurrentIndex(findShaderIndex());
    }

    const unsigned int flags = _sphere->Get_Flags();
    if (_cameraAlignCheck) {
        _cameraAlignCheck->setChecked((flags & SphereRenderObjClass::USE_CAMERA_ALIGN) != 0);
    }
    if (_loopCheck) {
        _loopCheck->setChecked((flags & SphereRenderObjClass::USE_ANIMATION_LOOP) != 0);
    }

    if (_useVectorCheck) {
        _useVectorCheck->setChecked((flags & SphereRenderObjClass::USE_ALPHA_VECTOR) != 0);
    }
    if (_invertVectorCheck) {
        _invertVectorCheck->setChecked((flags & SphereRenderObjClass::USE_INVERSE_ALPHA) != 0);
    }
    if (_vectorKeysTable) {
        _vectorKeysTable->setEnabled(_useVectorCheck && _useVectorCheck->isChecked());
    }
    if (_invertVectorCheck) {
        _invertVectorCheck->setEnabled(_useVectorCheck && _useVectorCheck->isChecked());
    }

    AABoxClass box;
    _sphere->Get_Obj_Space_Bounding_Box(box);
    if (_sizeXSpin) {
        _sizeXSpin->setValue(box.Extent.X);
    }
    if (_sizeYSpin) {
        _sizeYSpin->setValue(box.Extent.Y);
    }
    if (_sizeZSpin) {
        _sizeZSpin->setValue(box.Extent.Z);
    }

    SphereColorChannelClass color_channel = _sphere->Get_Color_Channel();
    if (color_channel.Get_Key_Count() == 0) {
        color_channel.Add_Key(_sphere->Get_Color(), 0.0f);
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

    SphereAlphaChannelClass alpha_channel = _sphere->Get_Alpha_Channel();
    if (alpha_channel.Get_Key_Count() == 0) {
        alpha_channel.Add_Key(_sphere->Get_Alpha(), 0.0f);
    }
    QVector<QVector<double>> alpha_rows;
    for (int i = 0; i < alpha_channel.Get_Key_Count(); ++i) {
        const auto &key = alpha_channel.Get_Key(i);
        alpha_rows.push_back({key.Get_Time(), key.Get_Value()});
    }
    SetKeyframeRows(_alphaKeysTable,
                    alpha_rows,
                    QVector<KeyframeColumnSpec>{{0.0, 1.0, 3}, {0.0, 1.0, 3}});

    SphereVectorChannelClass vector_channel = _sphere->Get_Vector_Channel();
    if (vector_channel.Get_Key_Count() == 0) {
        vector_channel.Add_Key(_sphere->Get_Vector(), 0.0f);
    }
    QVector<QVector<double>> vector_rows;
    for (int i = 0; i < vector_channel.Get_Key_Count(); ++i) {
        const auto &key = vector_channel.Get_Key(i);
        const AlphaVectorStruct value = key.Get_Value();
        float y_deg = 0.0f;
        float z_deg = 0.0f;
        AlphaVectorAngles(value, y_deg, z_deg);
        vector_rows.push_back({key.Get_Time(), value.intensity, y_deg, z_deg});
    }
    SetKeyframeRows(_vectorKeysTable,
                    vector_rows,
                    QVector<KeyframeColumnSpec>{{0.0, 1.0, 3},
                                                {0.0, 10.0, 2},
                                                {0.0, 179.0, 0},
                                                {0.0, 179.0, 0}});

    SphereScaleChannelClass scale_channel = _sphere->Get_Scale_Channel();
    if (scale_channel.Get_Key_Count() == 0) {
        scale_channel.Add_Key(_sphere->Get_Scale(), 0.0f);
    }
    QVector<QVector<double>> scale_rows;
    for (int i = 0; i < scale_channel.Get_Key_Count(); ++i) {
        const auto &key = scale_channel.Get_Key(i);
        const Vector3 value = key.Get_Value();
        scale_rows.push_back({key.Get_Time(), value.X, value.Y, value.Z});
    }
    SetKeyframeRows(_scaleKeysTable,
                    scale_rows,
                    QVector<KeyframeColumnSpec>{{0.0, 1.0, 3},
                                                {0.0, 10000.0, 3},
                                                {0.0, 10000.0, 3},
                                                {0.0, 10000.0, 3}});
}

int SphereEditDialog::findShaderIndex() const
{
    if (!_sphere) {
        return 0;
    }

    int preset_count = 0;
    const ShaderPreset *presets = ShaderPresets(preset_count);
    const ShaderClass &shader = _sphere->Get_Shader();
    for (int index = 0; index < preset_count; ++index) {
        if (ShaderMatches(presets[index].shader, shader)) {
            return index;
        }
    }

    return 0;
}
