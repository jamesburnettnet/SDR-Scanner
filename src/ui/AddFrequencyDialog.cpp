#include "AddFrequencyDialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QCloseEvent>

AddFrequencyDialog::AddFrequencyDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Frequency"));

    m_mhzSpin = new QDoubleSpinBox(this);
    m_mhzSpin->setRange(0.01, 6000.0);
    m_mhzSpin->setDecimals(5);
    m_mhzSpin->setSuffix(QStringLiteral(" MHz"));
    m_mhzSpin->setValue(462.5625);

    m_labelEdit = new QLineEdit(this);
    m_labelEdit->setPlaceholderText(QStringLiteral("e.g. County Fire Dispatch"));

    m_modulationCombo = new QComboBox(this);
    m_modulationCombo->addItems({QStringLiteral("NFM"), QStringLiteral("FM"), QStringLiteral("AM")});

    m_autoSquelchCheck = new QCheckBox(QStringLiteral("Auto squelch"), this);
    m_autoSquelchCheck->setChecked(true);
    m_autoSquelchCheck->setToolTip(QStringLiteral(
        "Adapts to the noise floor live while scanning.\n"
        "For a fixed value instead, uncheck this and use Auto Tune."));

    m_squelchSpin = new QDoubleSpinBox(this);
    m_squelchSpin->setRange(-100.0, 0.0);
    m_squelchSpin->setDecimals(1);
    m_squelchSpin->setSuffix(QStringLiteral(" dBFS"));
    m_squelchSpin->setValue(-50.0);
    m_squelchSpin->setEnabled(false);

    m_autoTuneButton = new QPushButton(QStringLiteral("Auto Tune"), this);
    m_autoTuneButton->setToolTip(QStringLiteral(
        "Briefly tunes the SDR to this frequency, measures the noise floor,\n"
        "and fills in a fixed squelch value from it (switches off Auto squelch)."));

    connect(m_autoSquelchCheck, &QCheckBox::toggled, m_squelchSpin, [this](bool checked) {
        m_squelchSpin->setEnabled(!checked);
    });
    connect(m_autoTuneButton, &QPushButton::clicked, this, [this]() {
        Frequency tmp;
        tmp.setMhz(m_mhzSpin->value());
        emit autoTuneRequested(tmp.hz, modulationFromString(m_modulationCombo->currentText()));
    });

    auto *squelchRow = new QHBoxLayout;
    squelchRow->addWidget(m_squelchSpin, 1);
    squelchRow->addWidget(m_autoTuneButton);

    m_calibrationLabel = new QLabel(this);
    m_calibrationLabel->setStyleSheet(QStringLiteral("color: #666;"));

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Frequency:"), m_mhzSpin);
    form->addRow(QStringLiteral("Label:"), m_labelEdit);
    form->addRow(QStringLiteral("Mode:"), m_modulationCombo);
    form->addRow(QString(), m_autoSquelchCheck);
    form->addRow(QStringLiteral("Squelch:"), squelchRow);
    form->addRow(QString(), m_calibrationLabel);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_buttons);
}

void AddFrequencyDialog::setFrequency(const Frequency &f)
{
    m_id = f.id;
    m_mhzSpin->setValue(f.mhz());
    m_labelEdit->setText(f.label);
    m_modulationCombo->setCurrentText(modulationToString(f.modulation));
    m_autoSquelchCheck->setChecked(f.autoSquelch);
    m_squelchSpin->setValue(f.squelchDb);
    m_squelchSpin->setEnabled(!f.autoSquelch);
}

Frequency AddFrequencyDialog::frequency() const
{
    Frequency f;
    if (!m_id.isNull())
        f.id = m_id;
    f.setMhz(m_mhzSpin->value());
    f.label = m_labelEdit->text().trimmed();
    f.modulation = modulationFromString(m_modulationCombo->currentText());
    f.autoSquelch = m_autoSquelchCheck->isChecked();
    f.squelchDb = m_squelchSpin->value();
    f.enabled = true;
    return f;
}

void AddFrequencyDialog::setCalibrating(bool active)
{
    m_calibrating = active;
    m_autoTuneButton->setEnabled(!active);
    m_buttons->setEnabled(!active);
    m_calibrationLabel->setText(active ? QStringLiteral("Measuring noise floor...") : QString());
}

void AddFrequencyDialog::applyCalibrationResult(double noiseFloorDb, double suggestedSquelchDb)
{
    setCalibrating(false);
    m_autoSquelchCheck->setChecked(false);
    m_squelchSpin->setEnabled(true);
    m_squelchSpin->setValue(suggestedSquelchDb);
    m_calibrationLabel->setText(QStringLiteral("Measured noise floor: %1 dBFS")
                                     .arg(noiseFloorDb, 0, 'f', 1));
}

void AddFrequencyDialog::showCalibrationError(const QString &message)
{
    setCalibrating(false);
    m_calibrationLabel->setText(message);
}

void AddFrequencyDialog::closeEvent(QCloseEvent *event)
{
    if (m_calibrating) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void AddFrequencyDialog::reject()
{
    if (m_calibrating)
        return;
    QDialog::reject();
}
