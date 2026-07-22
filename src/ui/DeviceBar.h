#pragma once

#include <QWidget>

class QComboBox;
class QPushButton;
class QLabel;
class SdrDeviceManager;

// Device selection row. Auto-detects on app start; from then on:
//  - Not connected: shows the device combo, a "Detect" button (re-enumerate
//    USB, e.g. after plugging in a different SDR), and a "Connect" button.
//  - Connected: shows just "Disconnect" and the connected device's name --
//    Detect/combo are hidden since only one device is used at a time.
// Detect reappears as soon as you disconnect, so swapping dongles is
// Disconnect -> Detect -> pick from combo -> Connect.
class DeviceBar : public QWidget {
    Q_OBJECT
public:
    explicit DeviceBar(SdrDeviceManager *manager, QWidget *parent = nullptr);

signals:
    void connectRequested(int index);
    void disconnectRequested();
    void detectRequested();

private:
    void refreshCombo();
    void updateForState();

    SdrDeviceManager *m_manager;
    QComboBox *m_combo;
    QPushButton *m_detectButton;
    QPushButton *m_connectButton;
    QLabel *m_statusLabel;
};
