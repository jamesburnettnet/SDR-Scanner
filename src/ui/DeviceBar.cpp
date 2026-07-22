#include "DeviceBar.h"
#include "../sdr/SdrDeviceManager.h"
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>

DeviceBar::DeviceBar(SdrDeviceManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    auto *deviceLabel = new QLabel(QStringLiteral("SDR Device:"), this);

    m_combo = new QComboBox(this);
    m_combo->setMinimumWidth(280);
    m_detectButton = new QPushButton(QStringLiteral("Detect"), this);
    m_connectButton = new QPushButton(QStringLiteral("Connect"), this);
    m_statusLabel = new QLabel(QStringLiteral("No device"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #b04040; font-weight: bold;"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(deviceLabel);
    layout->addWidget(m_combo, 1);
    layout->addWidget(m_detectButton);
    layout->addWidget(m_connectButton);
    layout->addWidget(m_statusLabel);

    // Hidden together with the combo/detect button once connected -- see
    // updateForState(). deviceLabel is owned by `this` via Qt's parent-child
    // widget ownership, so capturing the raw pointer here is safe for the
    // lifetime of this DeviceBar.
    auto setDeviceLabelVisible = [deviceLabel](bool visible) { deviceLabel->setVisible(visible); };

    connect(m_detectButton, &QPushButton::clicked, this, &DeviceBar::detectRequested);
    connect(m_connectButton, &QPushButton::clicked, this, [this]() {
        if (m_manager->isDeviceOpen()) {
            emit disconnectRequested();
            return;
        }
        const int idx = m_combo->currentData().toInt();
        if (idx >= 0)
            emit connectRequested(idx);
    });
    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { updateForState(); });
    connect(m_manager, &SdrDeviceManager::devicesChanged, this, &DeviceBar::refreshCombo);
    connect(m_manager, &SdrDeviceManager::deviceOpened, this, [this, setDeviceLabelVisible]() {
        setDeviceLabelVisible(false);
        updateForState();
    });
    connect(m_manager, &SdrDeviceManager::deviceClosed, this, [this, setDeviceLabelVisible]() {
        setDeviceLabelVisible(true);
        updateForState();
    });

    refreshCombo();
}

void DeviceBar::refreshCombo()
{
    m_combo->clear();
    for (const auto &info : m_manager->availableDevices()) {
        QString text = info.name.isEmpty() ? QStringLiteral("RTL-SDR #%1").arg(info.index) : info.name;
        if (!info.serial.isEmpty())
            text += QStringLiteral("  [%1]").arg(info.serial);
        m_combo->addItem(text, info.index);
    }
    if (m_combo->count() == 0)
        m_combo->addItem(QStringLiteral("(no devices found -- click Detect)"), -1);
    updateForState();
}

void DeviceBar::updateForState()
{
    const bool connected = m_manager->isDeviceOpen();

    m_combo->setVisible(!connected);
    m_detectButton->setVisible(!connected);

    if (connected) {
        m_connectButton->setText(QStringLiteral("Disconnect"));
        m_statusLabel->setText(QStringLiteral("Connected: %1").arg(m_manager->currentDeviceName()));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #2f9e44; font-weight: bold;"));
    } else {
        m_connectButton->setText(QStringLiteral("Connect"));
        m_connectButton->setEnabled(m_combo->currentData().toInt() >= 0);
        m_statusLabel->setText(QStringLiteral("No device"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #b04040; font-weight: bold;"));
    }
}
