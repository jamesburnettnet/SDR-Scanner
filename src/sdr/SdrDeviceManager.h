#pragma once

#include <QObject>
#include <QVector>
#include <memory>
#include "ISdrDevice.h"

// Enumerates and owns at most one open SDR device at a time, per the
// "single SDR, simple" design: the user picks a device from a dropdown
// populated by rescan()/Detect, and this manager owns opening/closing it.
// Runs on the GUI thread; ScanEngine is handed the already-open
// ISdrDevice* to stream from on its worker thread.
class SdrDeviceManager : public QObject {
    Q_OBJECT
public:
    explicit SdrDeviceManager(QObject *parent = nullptr);
    ~SdrDeviceManager() override;

    // Re-enumerates USB devices. Safe to call whether or not a device is
    // currently open (closes the current device first, since the user
    // may have unplugged it and plugged in another).
    void rescan();

    const QVector<SdrDeviceInfo> &availableDevices() const { return m_devices; }

    bool openDevice(int deviceIndex);
    void closeDevice();
    bool isDeviceOpen() const;

    ISdrDevice *device() const { return m_device.get(); }
    QString currentDeviceName() const { return m_currentDeviceName; }

signals:
    void devicesChanged();
    void deviceOpened(const QString &name);
    void deviceClosed();
    void errorOccurred(const QString &message);

private:
    QVector<SdrDeviceInfo> m_devices;
    std::unique_ptr<ISdrDevice> m_device;
    QString m_currentDeviceName;
};
