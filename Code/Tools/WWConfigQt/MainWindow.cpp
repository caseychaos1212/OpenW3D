#include "MainWindow.h"

#include <QLabel>
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
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_statusLabel = new QLabel(tr("Loading locale data..."), central);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_tabWidget = new QTabWidget(central);
    layout->addWidget(m_tabWidget);

    m_performancePage = new PerformancePage(m_backend, m_tabWidget);
    m_tabWidget->addTab(m_performancePage, tr("Performance"));
    connect(m_performancePage, &PerformancePage::settingsChanged, this, [this]() {
        updateStatusText();
    });

    m_videoPage = new VideoPage(m_backend, m_tabWidget);
    m_tabWidget->addTab(m_videoPage, tr("Video"));

    m_audioPage = new AudioPage(m_backend, m_tabWidget);
    m_tabWidget->addTab(m_audioPage, tr("Audio"));

    layout->addStretch();

    setCentralWidget(central);
    setMinimumSize(420, 280);
    setWindowTitle(tr("Renegade Configuration (Qt)"));
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
    const QString renderSummary = tr("LOD budget: %1\nTexture filter: %2\nShadow mode: %3")
                                      .arg(settings.dynamicLOD)
                                      .arg(settings.textureFilter)
                                      .arg(settings.shadowMode);

    m_statusLabel->setText(
        tr("Qt port work-in-progress.\n\n%1\n\nCurrent render settings:\n%2\n\nMore panels coming soon.")
            .arg(state, renderSummary));

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
