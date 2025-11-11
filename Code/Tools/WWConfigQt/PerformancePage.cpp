#include "PerformancePage.h"

#include <algorithm>
#include <array>
#include <iterator>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include "../../ww3d2/ww3d.h"
#include "../../ww3d2/texture.h"

namespace
{
struct PresetLevel
{
    int geometry;
    int character;
    int texture;
    int surface;
    int particle;
    int textureFilter;
    int lightingMode;
    bool terrainShadows;
};

constexpr PresetLevel kPresets[] = {
    {0, 0, 0, 0, 0, 0, 0, false},
    {0, 1, 1, 0, 0, 0, WW3D::PRELIT_MODE_LIGHTMAP_MULTI_TEXTURE, false},
    {1, 2, 2, 1, 1, 1, WW3D::PRELIT_MODE_LIGHTMAP_MULTI_TEXTURE, true},
    {2, 3, 2, 2, 2, 1, WW3D::PRELIT_MODE_LIGHTMAP_MULTI_TEXTURE, true},
};
constexpr int kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

int textureSliderFromResolution(int textureRes)
{
    return std::clamp(2 - textureRes, 0, 2);
}

int textureResolutionFromSlider(int sliderValue)
{
    return std::max(2 - sliderValue, 0);
}
} // namespace

PerformancePage::PerformancePage(WWConfigBackend &backend, QWidget *parent)
    : QWidget(parent),
      m_backend(backend)
{
    buildUi();
    refresh();
}

void PerformancePage::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *overallLayout = new QHBoxLayout();
    overallLayout->addWidget(new QLabel(tr("Detail:"), this));

    m_overallSlider = new QSlider(Qt::Horizontal, this);
    m_overallSlider->setRange(0, 3);
    overallLayout->addWidget(m_overallSlider, 1);

    m_expertCheck = new QCheckBox(tr("Expert Mode"), this);
    overallLayout->addWidget(m_expertCheck);

    m_autoButton = new QPushButton(tr("Auto Config"), this);
    overallLayout->addWidget(m_autoButton);

    mainLayout->addLayout(overallLayout);

    m_expertGroup = new QGroupBox(tr("Expert Settings"), this);
    auto *groupLayout = new QGridLayout(m_expertGroup);

    auto createSlider = [this]() {
        auto *slider = new QSlider(Qt::Horizontal, m_expertGroup);
        slider->setRange(0, 2);
        return slider;
    };

    m_geometrySlider = createSlider();
    m_shadowSlider = new QSlider(Qt::Horizontal, m_expertGroup);
    m_shadowSlider->setRange(0, 3);
    m_textureSlider = createSlider();
    m_particleSlider = createSlider();
    m_surfaceSlider = createSlider();

    int row = 0;
    groupLayout->addWidget(new QLabel(tr("Geometry Detail"), m_expertGroup), row, 0);
    groupLayout->addWidget(m_geometrySlider, row, 1);
    ++row;

    groupLayout->addWidget(new QLabel(tr("Character Shadows"), m_expertGroup), row, 0);
    groupLayout->addWidget(m_shadowSlider, row, 1);
    ++row;

    groupLayout->addWidget(new QLabel(tr("Texture Detail"), m_expertGroup), row, 0);
    groupLayout->addWidget(m_textureSlider, row, 1);
    ++row;

    groupLayout->addWidget(new QLabel(tr("Particle Detail"), m_expertGroup), row, 0);
    groupLayout->addWidget(m_particleSlider, row, 1);
    ++row;

    groupLayout->addWidget(new QLabel(tr("Surface Effect Detail"), m_expertGroup), row, 0);
    groupLayout->addWidget(m_surfaceSlider, row, 1);
    ++row;

    m_lightingCombo = new QComboBox(m_expertGroup);
    m_lightingCombo->addItems({tr("Vertex"), tr("Multi-pass"), tr("Multi-texture")});
    groupLayout->addWidget(new QLabel(tr("Lighting Mode"), m_expertGroup), row, 0);
    groupLayout->addWidget(m_lightingCombo, row, 1);
    ++row;

    m_filterCombo = new QComboBox(m_expertGroup);
    m_filterCombo->addItems({tr("Bilinear"), tr("Trilinear")});
    groupLayout->addWidget(new QLabel(tr("Texture Filter"), m_expertGroup), row, 0);
    groupLayout->addWidget(m_filterCombo, row, 1);
    ++row;

    m_terrainCheck = new QCheckBox(tr("Terrain Casts Shadows"), m_expertGroup);
    groupLayout->addWidget(m_terrainCheck, row, 0, 1, 2);

    mainLayout->addWidget(m_expertGroup);

    setExpertControlsEnabled(false);

    connect(m_overallSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_blockSignals) {
            return;
        }
        if (!m_expertCheck->isChecked()) {
            applyPreset(value);
        }
    });

    connect(m_expertCheck, &QCheckBox::toggled, this, [this](bool checked) {
        setExpertControlsEnabled(checked);
        if (!checked) {
            applyPreset(m_overallSlider->value());
        }
    });

    connect(m_autoButton, &QPushButton::clicked, this, [this]() {
        m_backend.autoConfigRenderSettings();
        refresh();
        emit settingsChanged();
    });

    auto sliderChanged = [this]() {
        if (m_blockSignals) {
            return;
        }
        applySettingsFromControls();
    };

    connect(m_geometrySlider, &QSlider::valueChanged, this, sliderChanged);
    connect(m_shadowSlider, &QSlider::valueChanged, this, sliderChanged);
    connect(m_textureSlider, &QSlider::valueChanged, this, sliderChanged);
    connect(m_surfaceSlider, &QSlider::valueChanged, this, sliderChanged);
    connect(m_particleSlider, &QSlider::valueChanged, this, sliderChanged);
    connect(m_lightingCombo, &QComboBox::currentIndexChanged, this, sliderChanged);
    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, sliderChanged);
    connect(m_terrainCheck, &QCheckBox::toggled, this, sliderChanged);
}

void PerformancePage::refresh()
{
    loadSettings();
    updateControlsFromSettings();
}

void PerformancePage::loadSettings()
{
    m_settings = m_backend.loadRenderSettings();
}

void PerformancePage::updateControlsFromSettings()
{
    m_blockSignals = true;

    m_geometrySlider->setValue(geometryLevelFromSettings());
    m_shadowSlider->setValue(std::clamp(m_settings.shadowMode, 0, 3));
    m_textureSlider->setValue(textureSliderFromResolution(m_settings.textureResolution));
    m_surfaceSlider->setValue(std::clamp(m_settings.surfaceEffect, 0, 2));
    m_particleSlider->setValue(std::clamp(m_settings.particleDetail, 0, 2));
    m_lightingCombo->setCurrentIndex(std::clamp(m_settings.prelitMode, 0, 2));
    m_filterCombo->setCurrentIndex(std::clamp(m_settings.textureFilter, 0, 1));
    m_terrainCheck->setChecked(m_settings.staticShadows != 0);

    const int presetLevel = determinePresetLevel();
    m_overallSlider->setValue(presetLevel);

    m_blockSignals = false;
}

void PerformancePage::applySettingsFromControls()
{
    const int lodValue = [this]() {
        switch (m_geometrySlider->value()) {
        case 0: return 0;
        case 1: return 5000;
        default: return 10000;
        }
    }();
    m_settings.dynamicLOD = lodValue;
    m_settings.staticLOD = lodValue;

    m_settings.shadowMode = m_shadowSlider->value();
    m_settings.dynamicShadows = (m_settings.shadowMode != 0) ? 1 : 0;
    m_settings.textureResolution = textureResolutionFromSlider(m_textureSlider->value());
    m_settings.surfaceEffect = m_surfaceSlider->value();
    m_settings.particleDetail = m_particleSlider->value();
    m_settings.prelitMode = m_lightingCombo->currentIndex();
    m_settings.textureFilter = m_filterCombo->currentIndex();
    m_settings.staticShadows = m_terrainCheck->isChecked() ? 1 : 0;

    m_backend.saveRenderSettings(m_settings);
    emit settingsChanged();
}

void PerformancePage::applyPreset(int level)
{
    const int index = std::clamp(level, 0, kPresetCount - 1);
    const PresetLevel &preset = kPresets[index];

    m_blockSignals = true;
    m_geometrySlider->setValue(preset.geometry);
    m_shadowSlider->setValue(preset.character);
    m_textureSlider->setValue(preset.texture);
    m_surfaceSlider->setValue(preset.surface);
    m_particleSlider->setValue(preset.particle);
    m_filterCombo->setCurrentIndex(preset.textureFilter);
    m_lightingCombo->setCurrentIndex(preset.lightingMode);
    m_terrainCheck->setChecked(preset.terrainShadows);
    m_blockSignals = false;

    applySettingsFromControls();
}

int PerformancePage::determinePresetLevel() const
{
    const int geometry = m_geometrySlider->value();
    const int shadow = m_shadowSlider->value();
    const int texture = m_textureSlider->value();
    const int surface = m_surfaceSlider->value();
    const int particle = m_particleSlider->value();
    const int filter = m_filterCombo->currentIndex();
    const int lighting = m_lightingCombo->currentIndex();
    const bool terrain = m_terrainCheck->isChecked();

    for (int i = 0; i < kPresetCount; ++i) {
        const auto &preset = kPresets[i];
        if (preset.geometry == geometry &&
            preset.character == shadow &&
            preset.texture == texture &&
            preset.surface == surface &&
            preset.particle == particle &&
            preset.textureFilter == filter &&
            preset.lightingMode == lighting &&
            preset.terrainShadows == terrain) {
            return i;
        }
    }

    return geometry;
}

int PerformancePage::geometryLevelFromSettings() const
{
    if (m_settings.dynamicLOD < 1000 && m_settings.staticLOD < 1000) {
        return 0;
    }
    if (m_settings.dynamicLOD <= 5000 && m_settings.staticLOD <= 5000) {
        return 1;
    }
    return 2;
}

void PerformancePage::setExpertControlsEnabled(bool enabled)
{
    m_expertGroup->setEnabled(enabled);
}
