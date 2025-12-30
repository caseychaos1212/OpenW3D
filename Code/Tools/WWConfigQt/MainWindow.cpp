#include "MainWindow.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "WWConfigBackend.h"
#include "PerformancePage.h"
#include "VideoPage.h"
#include "AudioPage.h"
#include "../WWConfig/wwconfig_ids.h"

MainWindow::MainWindow(WWConfigBackend &backend, QWidget *parent)
    : QMainWindow(parent),
      m_backend(backend)
{
    setupUi();
    updateStatusText();
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *banner = new QLabel(central);
    banner->setFrameShape(QFrame::Panel);
    banner->setFrameShadow(QFrame::Sunken);
    banner->setAlignment(Qt::AlignCenter);
    banner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QPixmap logo(QStringLiteral(":/wwconfig/logo.bmp"));
    if (!logo.isNull()) {
        banner->setPixmap(logo);
        banner->setScaledContents(true);
        banner->setMinimumHeight(logo.height());
    } else {
        banner->setText(tr("Renegade Config"));
        banner->setMinimumHeight(40);
    }
    layout->addWidget(banner);

    m_tabWidget = new QTabWidget(central);
    m_tabWidget->setDocumentMode(true);
    layout->addWidget(m_tabWidget);

    m_videoPage = new VideoPage(m_backend, m_tabWidget);
    m_tabWidget->addTab(m_videoPage, tr("Video"));

    m_audioPage = new AudioPage(m_backend, m_tabWidget);
    m_tabWidget->addTab(m_audioPage, tr("Audio"));

    m_performancePage = new PerformancePage(m_backend, m_tabWidget);
    m_tabWidget->addTab(m_performancePage, tr("Performance"));
    connect(m_performancePage, &PerformancePage::settingsChanged, this, [this]() {
        updateStatusText();
    });

    m_statusLabel = new QLabel(tr("Loading locale data..."), central);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(m_statusLabel);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto *okButton = new QPushButton(tr("OK"), central);
    auto *cancelButton = new QPushButton(tr("Cancel"), central);
    buttonRow->addWidget(okButton);
    buttonRow->addWidget(cancelButton);
    layout->addLayout(buttonRow);

    connect(okButton, &QPushButton::clicked, this, &QWidget::close);
    connect(cancelButton, &QPushButton::clicked, this, &QWidget::close);

    setCentralWidget(central);
    resize(420, 480);
    setMinimumSize(360, 360);
    setWindowTitle(tr("Renegade Config"));
    setWindowIcon(QIcon(QStringLiteral(":/wwconfig/wwconfig.ico")));
}

void MainWindow::updateStatusText()
{
    const QString localizedTitle = m_backend.localizedString(IDS_WWCONFIG_TITLE);
    if (!localizedTitle.isEmpty()) {
        setWindowTitle(localizedTitle);
    }

    const QString state = m_backend.isLocaleReady()
                              ? tr("Locale bank loaded.")
                              : tr("Locale bank unavailable, using fallback strings.");

    const RenderSettings settings = m_backend.loadRenderSettings();
    const QString renderSummary = tr("LOD %1  Filter %2  Shadows %3")
                                      .arg(settings.dynamicLOD)
                                      .arg(settings.textureFilter)
                                      .arg(settings.shadowMode);

    m_statusLabel->setText(tr("%1 · %2").arg(state, renderSummary));

    refreshTabs();
}

void MainWindow::refreshTabs()
{
    if (m_performancePage) {
        m_performancePage->refresh();
    }
    if (m_videoPage) {
        m_videoPage->refresh();
    }
    if (m_audioPage) {
        m_audioPage->refresh();
    }
}
