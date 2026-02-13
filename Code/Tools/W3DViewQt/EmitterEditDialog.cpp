#include "EmitterEditDialog.h"

#include "part_ldr.h"
#include "shader.h"
#include "vector3.h"
#include "w3d_file.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

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
        BuildPreset("Additive", ShaderClass::_PresetAdditiveSpriteShader),
        BuildPreset("Alpha", ShaderClass::_PresetAlphaSpriteShader),
        BuildPreset("Alpha-Test", ShaderClass::_PresetATestSpriteShader),
        BuildPreset("Alpha-Test-Blend", ShaderClass::_PresetATestBlendSpriteShader),
        BuildPreset("Screen", ShaderClass::_PresetScreenSpriteShader),
        BuildPreset("Multiplicative", ShaderClass::_PresetMultiplicativeSpriteShader),
        BuildPreset("Opaque", ShaderClass::_PresetOpaqueSpriteShader),
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

QWidget *MakeVectorRow(QDoubleSpinBox *&xSpin,
                       QDoubleSpinBox *&ySpin,
                       QDoubleSpinBox *&zSpin,
                       QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("X", row));
    xSpin = MakeSpin(-10000.0, 10000.0, 0.0, 2, row);
    layout->addWidget(xSpin);

    layout->addWidget(new QLabel("Y", row));
    ySpin = MakeSpin(-10000.0, 10000.0, 0.0, 2, row);
    layout->addWidget(ySpin);

    layout->addWidget(new QLabel("Z", row));
    zSpin = MakeSpin(-10000.0, 10000.0, 0.0, 2, row);
    layout->addWidget(zSpin);

    return row;
}
}

EmitterEditDialog::EmitterEditDialog(const ParticleEmitterDefClass &definition, QWidget *parent)
    : QDialog(parent),
      _definition(definition)
{
    setWindowTitle("Emitter Properties");

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
    connect(browse_button, &QPushButton::clicked, this, &EmitterEditDialog::browseTexture);
    texture_layout->addWidget(_textureEdit);
    texture_layout->addWidget(browse_button);
    general_layout->addRow("Texture:", texture_row);

    auto *lifetime_row = new QWidget(general_tab);
    auto *lifetime_layout = new QHBoxLayout(lifetime_row);
    lifetime_layout->setContentsMargins(0, 0, 0, 0);
    _useLifetimeCheck = new QCheckBox("Use Lifetime", lifetime_row);
    _lifetimeSpin = MakeSpin(0.0, 10000.0, 0.0, 2, lifetime_row);
    lifetime_layout->addWidget(_useLifetimeCheck);
    lifetime_layout->addWidget(_lifetimeSpin);
    general_layout->addRow("Lifetime:", lifetime_row);
    connect(_useLifetimeCheck, &QCheckBox::toggled, this, &EmitterEditDialog::toggleLifetime);

    _shaderCombo = new QComboBox(general_tab);
    int preset_count = 0;
    const ShaderPreset *presets = ShaderPresets(preset_count);
    for (int i = 0; i < preset_count; ++i) {
        _shaderCombo->addItem(presets[i].label, i);
    }
    general_layout->addRow("Shader:", _shaderCombo);

    _renderModeCombo = new QComboBox(general_tab);
    _renderModeCombo->addItem("Triangles", W3D_EMITTER_RENDER_MODE_TRI_PARTICLES);
    _renderModeCombo->addItem("Quads", W3D_EMITTER_RENDER_MODE_QUAD_PARTICLES);
    _renderModeCombo->addItem("Line", W3D_EMITTER_RENDER_MODE_LINE);
    _renderModeCombo->addItem("Line Group (Tetra)", W3D_EMITTER_RENDER_MODE_LINEGRP_TETRA);
    _renderModeCombo->addItem("Line Group (Prism)", W3D_EMITTER_RENDER_MODE_LINEGRP_PRISM);
    general_layout->addRow("Render Mode:", _renderModeCombo);

    _frameModeCombo = new QComboBox(general_tab);
    _frameModeCombo->addItem("1x1", W3D_EMITTER_FRAME_MODE_1x1);
    _frameModeCombo->addItem("2x2", W3D_EMITTER_FRAME_MODE_2x2);
    _frameModeCombo->addItem("4x4", W3D_EMITTER_FRAME_MODE_4x4);
    _frameModeCombo->addItem("8x8", W3D_EMITTER_FRAME_MODE_8x8);
    _frameModeCombo->addItem("16x16", W3D_EMITTER_FRAME_MODE_16x16);
    general_layout->addRow("Frame Mode:", _frameModeCombo);

    tabs->addTab(general_tab, "General");

    auto *emission_tab = new QWidget(tabs);
    auto *emission_layout = new QFormLayout(emission_tab);

    _emissionRateSpin = MakeSpin(-10000.0, 10000.0, 0.0, 2, emission_tab);
    emission_layout->addRow("Emission Rate:", _emissionRateSpin);

    _burstSizeSpin = MakeSpin(0.0, 10000.0, 0.0, 0, emission_tab);
    emission_layout->addRow("Burst Size:", _burstSizeSpin);

    auto *max_row = new QWidget(emission_tab);
    auto *max_layout = new QHBoxLayout(max_row);
    max_layout->setContentsMargins(0, 0, 0, 0);
    _limitParticlesCheck = new QCheckBox("Limit", max_row);
    _maxParticlesSpin = MakeSpin(0.0, 10000.0, 0.0, 0, max_row);
    max_layout->addWidget(_limitParticlesCheck);
    max_layout->addWidget(_maxParticlesSpin);
    emission_layout->addRow("Max Particles:", max_row);
    connect(_limitParticlesCheck, &QCheckBox::toggled, this, &EmitterEditDialog::toggleMaxParticles);

    _fadeTimeSpin = MakeSpin(0.0, 10000.0, 0.0, 2, emission_tab);
    emission_layout->addRow("Fade Time:", _fadeTimeSpin);

    tabs->addTab(emission_tab, "Emission");

    auto *physics_tab = new QWidget(tabs);
    auto *physics_layout = new QFormLayout(physics_tab);

    physics_layout->addRow("Velocity:", MakeVectorRow(_velocityXSpin, _velocityYSpin, _velocityZSpin, physics_tab));
    physics_layout->addRow("Acceleration:", MakeVectorRow(_accelXSpin, _accelYSpin, _accelZSpin, physics_tab));

    _outwardVelSpin = MakeSpin(-10000.0, 10000.0, 0.0, 2, physics_tab);
    physics_layout->addRow("Outward Velocity:", _outwardVelSpin);

    _inheritVelSpin = MakeSpin(-10000.0, 10000.0, 0.0, 2, physics_tab);
    physics_layout->addRow("Velocity Inherit:", _inheritVelSpin);

    _gravitySpin = MakeSpin(-10000.0, 10000.0, 0.0, 2, physics_tab);
    physics_layout->addRow("Gravity:", _gravitySpin);

    _elasticitySpin = MakeSpin(-10000.0, 10000.0, 0.0, 2, physics_tab);
    physics_layout->addRow("Elasticity:", _elasticitySpin);

    tabs->addTab(physics_tab, "Physics");

    auto *user_tab = new QWidget(tabs);
    auto *user_layout = new QFormLayout(user_tab);

    _userTypeSpin = MakeSpin(-100000.0, 100000.0, 0.0, 0, user_tab);
    user_layout->addRow("User Type:", _userTypeSpin);

    _userStringEdit = new QLineEdit(user_tab);
    user_layout->addRow("User String:", _userStringEdit);

    tabs->addTab(user_tab, "User");

    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &EmitterEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    loadFromDefinition();
}

ParticleEmitterDefClass *EmitterEditDialog::definition() const
{
    return new ParticleEmitterDefClass(_definition);
}

QString EmitterEditDialog::originalName() const
{
    return _originalName;
}

void EmitterEditDialog::accept()
{
    const QString name = _nameEdit ? _nameEdit->text().trimmed() : QString();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Emitter", "Invalid emitter name. Please enter a name.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    _definition.Set_Name(name_bytes.constData());

    const QString texture = _textureEdit ? _textureEdit->text().trimmed() : QString();
    if (!texture.isEmpty()) {
        const QByteArray texture_bytes = texture.toLatin1();
        _definition.Set_Texture_Filename(texture_bytes.constData());
    } else {
        _definition.Set_Texture_Filename("");
    }

    const bool use_lifetime = _useLifetimeCheck && _useLifetimeCheck->isChecked();
    const float lifetime = use_lifetime ? static_cast<float>(_lifetimeSpin->value()) : 5000000.0f;
    _definition.Set_Lifetime(lifetime);

    if (_shaderCombo) {
        const int preset_index = _shaderCombo->currentData().toInt();
        int preset_count = 0;
        const ShaderPreset *presets = ShaderPresets(preset_count);
        if (preset_index >= 0 && preset_index < preset_count) {
            _definition.Set_Shader(presets[preset_index].shader);
        }
    }

    if (_renderModeCombo) {
        _definition.Set_Render_Mode(_renderModeCombo->currentData().toInt());
    }
    if (_frameModeCombo) {
        _definition.Set_Frame_Mode(_frameModeCombo->currentData().toInt());
    }

    _definition.Set_Emission_Rate(static_cast<float>(_emissionRateSpin->value()));
    _definition.Set_Burst_Size(static_cast<unsigned int>(_burstSizeSpin->value()));

    float max_particles = 0.0f;
    if (_limitParticlesCheck && _limitParticlesCheck->isChecked()) {
        max_particles = static_cast<float>(_maxParticlesSpin->value());
    }
    _definition.Set_Max_Emissions(max_particles);
    _definition.Set_Fade_Time(static_cast<float>(_fadeTimeSpin->value()));

    Vector3 velocity(static_cast<float>(_velocityXSpin->value()),
                     static_cast<float>(_velocityYSpin->value()),
                     static_cast<float>(_velocityZSpin->value()));
    _definition.Set_Velocity(velocity);

    Vector3 accel(static_cast<float>(_accelXSpin->value()),
                  static_cast<float>(_accelYSpin->value()),
                  static_cast<float>(_accelZSpin->value()));
    _definition.Set_Acceleration(accel);

    _definition.Set_Outward_Vel(static_cast<float>(_outwardVelSpin->value()));
    _definition.Set_Vel_Inherit(static_cast<float>(_inheritVelSpin->value()));
    _definition.Set_Gravity(static_cast<float>(_gravitySpin->value()));
    _definition.Set_Elasticity(static_cast<float>(_elasticitySpin->value()));

    if (_userStringEdit) {
        const QByteArray user_string = _userStringEdit->text().trimmed().toLatin1();
        _definition.Set_User_String(user_string.constData());
    }
    if (_userTypeSpin) {
        _definition.Set_User_Type(static_cast<int>(_userTypeSpin->value()));
    }

    QDialog::accept();
}

void EmitterEditDialog::browseTexture()
{
    const QString start = _textureEdit ? _textureEdit->text() : QString();
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Select Texture",
        start,
        "Texture Files (*.tga *.dds *.png *.jpg *.jpeg);;All Files (*.*)");
    if (!path.isEmpty() && _textureEdit) {
        _textureEdit->setText(path);
    }
}

void EmitterEditDialog::toggleLifetime(bool enabled)
{
    if (_lifetimeSpin) {
        _lifetimeSpin->setEnabled(enabled);
    }
}

void EmitterEditDialog::toggleMaxParticles(bool enabled)
{
    if (_maxParticlesSpin) {
        _maxParticlesSpin->setEnabled(enabled);
    }
}

void EmitterEditDialog::loadFromDefinition()
{
    const char *name = _definition.Get_Name();
    if (name) {
        _nameEdit->setText(QString::fromLatin1(name));
        _originalName = QString::fromLatin1(name);
    }

    const char *texture = _definition.Get_Texture_Filename();
    if (texture) {
        _textureEdit->setText(QString::fromLatin1(texture));
    }

    const float lifetime = _definition.Get_Lifetime();
    const bool use_lifetime = lifetime < 100.0f;
    _useLifetimeCheck->setChecked(use_lifetime);
    _lifetimeSpin->setEnabled(use_lifetime);
    _lifetimeSpin->setValue(use_lifetime ? lifetime : 0.0);

    const int shader_index = findShaderIndex();
    if (shader_index >= 0) {
        _shaderCombo->setCurrentIndex(shader_index);
    }

    const int render_mode = _definition.Get_Render_Mode();
    for (int i = 0; i < _renderModeCombo->count(); ++i) {
        if (_renderModeCombo->itemData(i).toInt() == render_mode) {
            _renderModeCombo->setCurrentIndex(i);
            break;
        }
    }

    const int frame_mode = _definition.Get_Frame_Mode();
    for (int i = 0; i < _frameModeCombo->count(); ++i) {
        if (_frameModeCombo->itemData(i).toInt() == frame_mode) {
            _frameModeCombo->setCurrentIndex(i);
            break;
        }
    }

    _emissionRateSpin->setValue(_definition.Get_Emission_Rate());
    _burstSizeSpin->setValue(_definition.Get_Burst_Size());

    const float max_emissions = _definition.Get_Max_Emissions();
    const bool limit_particles = max_emissions != 0.0f;
    _limitParticlesCheck->setChecked(limit_particles);
    _maxParticlesSpin->setEnabled(limit_particles);
    _maxParticlesSpin->setValue(limit_particles ? max_emissions : 0.0f);

    _fadeTimeSpin->setValue(_definition.Get_Fade_Time());

    const Vector3 velocity = _definition.Get_Velocity();
    _velocityXSpin->setValue(velocity.X);
    _velocityYSpin->setValue(velocity.Y);
    _velocityZSpin->setValue(velocity.Z);

    const Vector3 accel = _definition.Get_Acceleration();
    _accelXSpin->setValue(accel.X);
    _accelYSpin->setValue(accel.Y);
    _accelZSpin->setValue(accel.Z);

    _outwardVelSpin->setValue(_definition.Get_Outward_Vel());
    _inheritVelSpin->setValue(_definition.Get_Vel_Inherit());
    _gravitySpin->setValue(_definition.Get_Gravity());
    _elasticitySpin->setValue(_definition.Get_Elasticity());

    const char *user_string = _definition.Get_User_String();
    if (user_string) {
        _userStringEdit->setText(QString::fromLatin1(user_string));
    }
    _userTypeSpin->setValue(_definition.Get_User_Type());
}

int EmitterEditDialog::findShaderIndex() const
{
    ShaderClass current;
    _definition.Get_Shader(current);

    int preset_count = 0;
    const ShaderPreset *presets = ShaderPresets(preset_count);
    for (int i = 0; i < preset_count; ++i) {
        if (ShaderMatches(current, presets[i].shader)) {
            return i;
        }
    }

    return -1;
}
