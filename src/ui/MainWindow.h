#pragma once

#include <QMainWindow>
#include <memory>
#include "../core/ScannerState.h"

class SdrDeviceManager;
class AudioOutput;
class ScanEngine;
class DeviceBar;
class LcdPanel;
class SignalStripChart;
class ActivityLogWidget;
class FrequencyTableView;
class QPushButton;
class QSlider;
class QCheckBox;
class QComboBox;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void startScan(bool explore);
    void stopScan();
    void onExploreClicked();
    void onDeviceConnectRequested(int index);
    void onDeviceDisconnectRequested();
    void loadFrequencies();
    void saveFrequencies();
    void updateButtonsForState(bool running);
    void wireEngineSignals();

    SdrDeviceManager *m_deviceManager;
    AudioOutput *m_audioOutput;
    ScannerStateHolder m_stateHolder;
    ScanEngine *m_engine = nullptr;

    DeviceBar *m_deviceBar;
    LcdPanel *m_lcdPanel;
    SignalStripChart *m_signalChart;
    ActivityLogWidget *m_activityLog;
    FrequencyTableView *m_freqTable;

    QPushButton *m_scanButton;
    QPushButton *m_exploreButton;
    QPushButton *m_stopButton;
    QCheckBox *m_muteCheck;
    QSlider *m_volumeSlider;

    // Mains-hum notch filter controls; applied live to m_engine (if
    // running) and to whatever engine gets created next.
    QCheckBox *m_humFilterCheck;
    QComboBox *m_humFilterCombo;
    bool m_humFilterEnabled = true;
    int m_humFilterHz = 60;

    // Lightweight poll of m_stateHolder just to keep the frequency table's
    // active-row highlight in sync -- doesn't need LCD/chart-grade frame
    // rate, so it's a separate, slower timer.
    QTimer *m_highlightTimer;
};
