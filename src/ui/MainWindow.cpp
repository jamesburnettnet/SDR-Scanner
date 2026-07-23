#include "MainWindow.h"
#include "DeviceBar.h"
#include "LcdPanel.h"
#include "SignalStripChart.h"
#include "ActivityLogWidget.h"
#include "FrequencyTableView.h"
#include "ExploreDialog.h"
#include "../sdr/SdrDeviceManager.h"
#include "../sdr/ISdrDevice.h"
#include "../core/AudioOutput.h"
#include "../core/ScanEngine.h"
#include "../core/SquelchCalibrator.h"
#include "../core/FrequencyListStore.h"
#include "../core/DebugLog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QMessageBox>
#include <QCloseEvent>
#include <QWidget>

#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "dev"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SDR Scanner v%1").arg(QStringLiteral(APP_VERSION_STRING)));

    m_deviceManager = new SdrDeviceManager(this);
    m_audioOutput = new AudioOutput(this);
    m_audioOutput->start();

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);

    m_deviceBar = new DeviceBar(m_deviceManager, this);
    rootLayout->addWidget(m_deviceBar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *leftPane = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftPane);
    m_lcdPanel = new LcdPanel(this);
    m_lcdPanel->setStateHolder(&m_stateHolder);
    leftLayout->addWidget(m_lcdPanel);

    auto *controlRow = new QHBoxLayout;
    m_scanButton = new QPushButton(QStringLiteral("Start Scan"), this);
    m_stopButton = new QPushButton(QStringLiteral("Stop"), this);
    m_exploreButton = new QPushButton(QStringLiteral("Explore..."), this);
    m_stopButton->setEnabled(false);
    controlRow->addWidget(m_scanButton);
    controlRow->addWidget(m_stopButton);
    controlRow->addWidget(m_exploreButton);
    leftLayout->addLayout(controlRow);

    auto *audioRow = new QHBoxLayout;
    m_muteCheck = new QCheckBox(QStringLiteral("Mute"), this);
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    audioRow->addWidget(m_muteCheck);
    audioRow->addWidget(new QLabel(QStringLiteral("Volume:"), this));
    audioRow->addWidget(m_volumeSlider, 1);
    leftLayout->addLayout(audioRow);

    auto *humRow = new QHBoxLayout;
    m_humFilterCheck = new QCheckBox(QStringLiteral("Hum Filter"), this);
    m_humFilterCheck->setChecked(m_humFilterEnabled);
    m_humFilterCheck->setToolTip(QStringLiteral(
        "Notches 50/60Hz mains hum (and its harmonics) out of the demodulated audio.\n"
        "Fixes a low hum riding along with clean audio -- usually caused by power/ground\n"
        "noise coupled into the SDR, not something in the received signal itself."));
    m_humFilterCombo = new QComboBox(this);
    m_humFilterCombo->addItem(QStringLiteral("60 Hz"), 60);
    m_humFilterCombo->addItem(QStringLiteral("50 Hz"), 50);
    m_humFilterCombo->setCurrentIndex(0); // default 60Hz
    humRow->addWidget(m_humFilterCheck);
    humRow->addWidget(m_humFilterCombo);
    humRow->addStretch(1);
    leftLayout->addLayout(humRow);

    m_signalChart = new SignalStripChart(this);
    m_signalChart->setStateHolder(&m_stateHolder);
    leftLayout->addWidget(m_signalChart, 1);

    m_activityLog = new ActivityLogWidget(this);
    leftLayout->addWidget(m_activityLog, 1);

    splitter->addWidget(leftPane);

    m_freqTable = new FrequencyTableView(this);
    splitter->addWidget(m_freqTable);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    rootLayout->addWidget(splitter, 1);

    setCentralWidget(central);
    resize(1280, 720);
    statusBar()->showMessage(QStringLiteral("Ready"));

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    QAction *toggleAnalyserAction = viewMenu->addAction(QStringLiteral("Show dBFS Analyser"));
    toggleAnalyserAction->setCheckable(true);
    toggleAnalyserAction->setChecked(true);
    connect(toggleAnalyserAction, &QAction::toggled, m_signalChart, &QWidget::setVisible);

    QAction *toggleLogAction = viewMenu->addAction(QStringLiteral("Show Activity Log"));
    toggleLogAction->setCheckable(true);
    toggleLogAction->setChecked(true);
    connect(toggleLogAction, &QAction::toggled, m_activityLog, &QWidget::setVisible);

    // Keeps the frequency table's active-row highlight in sync. Separate
    // from (and much slower than) the LCD/chart's 30fps timers -- a
    // highlighted row doesn't need to be frame-accurate.
    m_highlightTimer = new QTimer(this);
    connect(m_highlightTimer, &QTimer::timeout, this, [this]() {
        m_freqTable->setActiveFrequencyId(m_stateHolder.read().activeFrequencyId);
    });
    m_highlightTimer->start(150);

    connect(m_deviceBar, &DeviceBar::connectRequested, this, &MainWindow::onDeviceConnectRequested);
    connect(m_deviceBar, &DeviceBar::disconnectRequested, this, &MainWindow::onDeviceDisconnectRequested);
    connect(m_deviceBar, &DeviceBar::detectRequested, this, [this]() {
        SDR_LOG("ui") << "Detect clicked";
        // Stop any active scan first: rescanning closes the currently open
        // device, and doing that while ScanEngine's worker thread is still
        // blocked inside the device's async read call would race with
        // rtlsdr_close() on another thread.
        stopScan();
        m_deviceManager->rescan();
    });
    connect(m_scanButton, &QPushButton::clicked, this, [this]() { startScan(false); });
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopScan);
    connect(m_exploreButton, &QPushButton::clicked, this, &MainWindow::onExploreClicked);
    connect(m_muteCheck, &QCheckBox::toggled, this, [this](bool checked) { m_audioOutput->setMuted(checked); });
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int v) { m_audioOutput->setVolume(v / 100.0); });
    m_audioOutput->setVolume(0.8);

    connect(m_humFilterCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_humFilterEnabled = checked;
        if (m_engine)
            m_engine->setHumFilterEnabled(checked);
    });
    connect(m_humFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_humFilterHz = m_humFilterCombo->itemData(idx).toInt();
        if (m_engine)
            m_engine->setHumFilterHz(m_humFilterHz);
    });

    connect(m_deviceManager, &SdrDeviceManager::errorOccurred, this, [this](const QString &msg) {
        statusBar()->showMessage(QStringLiteral("Device error: %1").arg(msg), 6000);
    });

    // Persist on every add/edit/remove, not just on a clean exit -- saving
    // only from the destructor/closeEvent() means any edit made during a
    // session that later crashes (e.g. the SDR streaming crashes this app
    // has been chasing) is silently lost even though it never touched the
    // SDR device at all.
    connect(m_freqTable, &FrequencyTableView::listChanged, this, &MainWindow::saveFrequencies);

    m_freqTable->setCalibrationHandler([this](qint64 freqHz, Modulation mod,
                                               FrequencyTableView::CalibrationResultFn onDone) {
        SDR_LOG("ui") << "Auto Tune requested, freqHz=" << freqHz;
        if (m_engine) {
            SDR_LOG("ui") << "Auto Tune blocked: scan engine still running";
            onDone(false, 0.0, 0.0, QStringLiteral("Stop scanning before using Auto Tune."));
            return;
        }
        if (!m_deviceManager->isDeviceOpen()) {
            SDR_LOG("ui") << "Auto Tune blocked: no device open";
            onDone(false, 0.0, 0.0, QStringLiteral("No SDR device open. Use Detect first."));
            return;
        }
        if (!m_deviceManager->reopenCurrentDevice()) {
            SDR_LOG("ui") << "Auto Tune blocked: failed to reopen device";
            onDone(false, 0.0, 0.0, QStringLiteral("Failed to reopen SDR device."));
            return;
        }

        auto *calibrator = new SquelchCalibrator(this);
        connect(calibrator, &SquelchCalibrator::calibrationFinished, this,
                [onDone](bool ok, double noiseFloorDb, double suggestedDb, const QString &error) {
                    SDR_LOG("calib") << "calibrationFinished ok=" << ok << "noiseFloorDb=" << noiseFloorDb
                                      << "suggestedDb=" << suggestedDb << "error=" << error;
                    onDone(ok, noiseFloorDb, suggestedDb, error);
                });
        connect(calibrator, &QThread::finished, calibrator, &QObject::deleteLater);
        calibrator->configure(m_deviceManager->device(), freqHz, mod);
        SDR_LOG("calib") << "Starting SquelchCalibrator thread";
        calibrator->start();
    });

    loadFrequencies();
    m_deviceManager->rescan();
}

MainWindow::~MainWindow()
{
    stopScan();
    saveFrequencies();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    stopScan();
    saveFrequencies();
    QMainWindow::closeEvent(event);
}

void MainWindow::onDeviceConnectRequested(int index)
{
    SDR_LOG("ui") << "Connect requested, index=" << index;
    // Make sure the scan thread isn't still using the device before we
    // touch it (see the comment on the Detect handler above for why).
    stopScan();

    if (!m_deviceManager->openDevice(index)) {
        SDR_LOG("ui") << "Device open failed";
        statusBar()->showMessage(QStringLiteral("Failed to open device"), 6000);
    } else {
        SDR_LOG("ui") << "Device open OK:" << m_deviceManager->currentDeviceName();
        statusBar()->showMessage(QStringLiteral("Device connected: %1").arg(m_deviceManager->currentDeviceName()), 4000);
    }
}

void MainWindow::onDeviceDisconnectRequested()
{
    SDR_LOG("ui") << "Disconnect requested";
    stopScan();
    m_deviceManager->closeDevice();
    statusBar()->showMessage(QStringLiteral("Device disconnected"), 4000);
}

void MainWindow::onExploreClicked()
{
    SDR_LOG("ui") << "Explore... clicked";
    ExploreDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QVector<Frequency> sweep = dlg.buildSweep();
    if (sweep.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Explore"), QStringLiteral("Invalid range/step."));
        return;
    }

    stopScan();

    if (!m_deviceManager->isDeviceOpen()) {
        QMessageBox::warning(this, QStringLiteral("Explore"), QStringLiteral("No SDR device open. Use Detect first."));
        return;
    }
    if (!m_deviceManager->reopenCurrentDevice()) {
        SDR_LOG("ui") << "Explore: failed to reopen device";
        statusBar()->showMessage(QStringLiteral("Failed to reopen SDR device."), 6000);
        return;
    }

    SDR_LOG("ui") << "Starting explore engine, sweep points=" << sweep.size();
    m_engine = new ScanEngine(this);
    wireEngineSignals();
    m_engine->configure(m_deviceManager->device(), m_audioOutput, &m_stateHolder, sweep, true);
    m_engine->start();
    updateButtonsForState(true);
    statusBar()->showMessage(QStringLiteral("Exploring %1 -> %2 MHz")
                                  .arg(sweep.first().mhz(), 0, 'f', 4)
                                  .arg(sweep.last().mhz(), 0, 'f', 4));
}

void MainWindow::startScan(bool explore)
{
    Q_UNUSED(explore);

    SDR_LOG("ui") << "Start Scan clicked";
    if (!m_deviceManager->isDeviceOpen()) {
        QMessageBox::warning(this, QStringLiteral("Scan"), QStringLiteral("No SDR device open. Use Detect first."));
        return;
    }

    const QVector<Frequency> freqs = m_freqTable->model()->enabledItems();
    if (freqs.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Scan"),
                                  QStringLiteral("Add and enable at least one frequency first."));
        return;
    }

    stopScan();

    if (!m_deviceManager->reopenCurrentDevice()) {
        SDR_LOG("ui") << "startScan: failed to reopen device";
        statusBar()->showMessage(QStringLiteral("Failed to reopen SDR device."), 6000);
        return;
    }

    SDR_LOG("ui") << "Starting scan engine, enabled frequencies=" << freqs.size();
    m_engine = new ScanEngine(this);
    wireEngineSignals();
    m_engine->configure(m_deviceManager->device(), m_audioOutput, &m_stateHolder, freqs, false);
    m_engine->start();
    updateButtonsForState(true);
    statusBar()->showMessage(QStringLiteral("Scanning %1 frequency(ies)").arg(freqs.size()));
}

void MainWindow::stopScan()
{
    if (!m_engine) {
        SDR_LOG("ui") << "stopScan: no engine running, no-op";
        return;
    }
    SDR_LOG("ui") << "stopScan: requesting stop and waiting for scan thread to exit";
    m_engine->requestStop();
    m_engine->wait();
    SDR_LOG("ui") << "stopScan: scan thread exited";
    m_engine->deleteLater();
    m_engine = nullptr;
    updateButtonsForState(false);
    m_freqTable->setActiveFrequencyId(QUuid());
}

void MainWindow::wireEngineSignals()
{
    connect(m_engine, &ScanEngine::errorOccurred, this, [this](const QString &msg) {
        SDR_LOG("engine") << "errorOccurred:" << msg;
        statusBar()->showMessage(msg, 6000);
    });
    connect(m_engine, &QThread::finished, this, [this]() {
        SDR_LOG("engine") << "QThread::finished";
        updateButtonsForState(false);
    });
    connect(m_engine, &ScanEngine::activityLogged, this,
            [this](QDateTime startTime, qint64 freqHz, QString label, Modulation modulation, qint64 durationMs) {
                m_activityLog->addEntry(startTime, freqHz, label, modulation, durationMs);
            });

    m_engine->setHumFilterEnabled(m_humFilterEnabled);
    m_engine->setHumFilterHz(m_humFilterHz);
}

void MainWindow::updateButtonsForState(bool running)
{
    m_scanButton->setEnabled(!running);
    m_exploreButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
}

void MainWindow::loadFrequencies()
{
    QVector<Frequency> items;
    QString error;
    if (FrequencyListStore::loadJson(FrequencyListStore::defaultFilePath(), items, &error))
        m_freqTable->model()->setItems(items);
    else
        statusBar()->showMessage(QStringLiteral("Could not load saved frequencies: %1").arg(error), 6000);
}

void MainWindow::saveFrequencies()
{
    QString error;
    if (!FrequencyListStore::saveJson(FrequencyListStore::defaultFilePath(), m_freqTable->model()->items(), &error))
        statusBar()->showMessage(QStringLiteral("Could not save frequencies: %1").arg(error), 6000);
}
