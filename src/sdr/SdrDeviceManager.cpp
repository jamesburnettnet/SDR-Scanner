#include "SdrDeviceManager.h"
#include "RtlSdrDevice.h"

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
    emit devicesChanged();
}

bool SdrDeviceManager::openDevice(int deviceIndex)
{
    closeDevice();

    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        emit errorOccurred(QStringLiteral("Invalid device index"));
        return false;
    }

    auto dev = std::make_unique<RtlSdrDevice>();
    if (!dev->open(deviceIndex)) {
        emit errorOccurred(dev->lastError());
        return false;
    }

    m_device = std::move(dev);
    m_currentDeviceName = m_devices[deviceIndex].name;
    emit deviceOpened(m_currentDeviceName);
    return true;
}

void SdrDeviceManager::closeDevice()
{
    if (m_device) {
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
