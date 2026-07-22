#pragma once

#include <QDialog>
#include "../core/Frequency.h"

class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QDialogButtonBox;

// Add/edit form for a single scanned frequency: MHz, label, modulation
// (FM/NFM/AM), squelch (auto or manual dB), and a free-form group tag.
//
// Also hosts the "Auto Tune" button: a one-shot noise-floor measurement
// (performed elsewhere, via the calibration handler wired up by whoever
// owns this dialog) that fills in a fixed manual squelch value, as an
// alternative to live auto-squelch drifting while scanning.
class AddFrequencyDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddFrequencyDialog(QWidget *parent = nullptr);

    void setExistingGroups(const QStringList &groups);
    void setFrequency(const Frequency &f);
    Frequency frequency() const;

    // Disables editing/closing while a calibration measurement is running
    // (a few hundred ms), so the dialog can't be destroyed out from under
    // an in-flight asynchronous measurement.
    void setCalibrating(bool active);
    bool isCalibrating() const { return m_calibrating; }

    // Called by the calibration handler once a measurement completes.
    void applyCalibrationResult(double noiseFloorDb, double suggestedSquelchDb);
    void showCalibrationError(const QString &message);

signals:
    void autoTuneRequested(qint64 freqHz, Modulation modulation);

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private:
    QDoubleSpinBox *m_mhzSpin;
    QLineEdit *m_labelEdit;
    QComboBox *m_modulationCombo;
    QCheckBox *m_autoSquelchCheck;
    QDoubleSpinBox *m_squelchSpin;
    QComboBox *m_groupCombo;
    QPushButton *m_autoTuneButton;
    QLabel *m_calibrationLabel;
    QDialogButtonBox *m_buttons;
    QUuid m_id;
    bool m_calibrating = false;
};
