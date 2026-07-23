#include "SdrDeviceManager.h"
#include "RtlSdrDevice.h"
#include "../core/DebugLog.h"

SdrDeviceManager::SdrDeviceManager(QObject *parent)
    : QObject(parent)
{
}

SdrDeviceManager::~SdrDeviceManager()
{
    closeDevice();
}

void SdrDeviceManager::rescan()
{
    SDR_LOG("sdr") << "rescan() begin";
    closeDevice();

    m_devices.clear();
    const int count = RtlSdrDevice::deviceCount();
    for (int i = 0; i < count; ++i) {
        SdrDeviceInfo info;
        info.index = i;
        info.name = RtlSdrDevice::deviceName(i);
        info.serial = RtlSdrDevice::deviceSerial(i);
        m_devices.append(info);
    }
    SDR_LOG("sdr") << "rescan() found" << count << "device(s)";
    emit devicesChanged();
}

bool SdrDeviceManager::openDevice(int deviceIndex)
{
    SDR_LOG("sdr") << "openDevice(" << deviceIndex << ")";
    closeDevice();

    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        SDR_LOG("sdr") << "openDevice: invalid index";
        emit errorOccurred(QStringLiteral("Invalid device index"));
        return false;
    }

    auto dev = std::make_unique<RtlSdrDevice>();
    if (!dev->open(deviceIndex)) {
        SDR_LOG("sdr") << "openDevice: open() failed:" << dev->lastError();
        emit errorOccurred(dev->lastError());
        return false;
    }

    m_device = std::move(dev);
    m_currentDeviceName = m_devices[deviceIndex].name;
    m_currentDeviceIndex = deviceIndex;
    SDR_LOG("sdr") << "openDevice: opened" << m_currentDeviceName;
    emit deviceOpened(m_currentDeviceName);
    return true;
}

void SdrDeviceManager::closeDevice()
{
    if (m_device) {
        SDR_LOG("sdr") << "closeDevice(): closing" << m_currentDeviceName;
        m_device->close();
        m_device.reset();
        m_currentDeviceName.clear();
        emit deviceClosed();
    }
}

bool SdrDeviceManager::isDeviceOpen() const
{
    return m_device && m_device->isOpen();
}

bool SdrDeviceManager::reopenCurrentDevice()
{
    // The vendored Windows librtlsdr backend's async-cancel path is flaky
    // (see the comments in ScanEngine::requestStop()/onSamples()): a stop
    // that takes many retries to land can leave the DLL's internal USB
    // transfer state corrupted in a way that doesn't crash immediately but
    // poisons the *next* rtlsdr_read_async() on the same handle. Closing
    // and reopening the device forces the OS/WinUSB driver stack to rebuild
    // that state from scratch instead of inheriting whatever a rocky
    // cancellation left behind.
    if (m_currentDeviceIndex < 0) {
        emit errorOccurred(QStringLiteral("No device previously opened"));
        return false;
    }
    SDR_LOG("sdr") << "reopenCurrentDevice(): reopening index" << m_currentDeviceIndex;
    return openDevice(m_currentDeviceIndex);
}
