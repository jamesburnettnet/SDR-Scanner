#include "ExploreDialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <algorithm>

ExploreDialog::ExploreDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Explore Frequency Range"));

    m_startSpin = new QDoubleSpinBox(this);
    m_startSpin->setRange(0.01, 6000.0);
    m_startSpin->setDecimals(4);
    m_startSpin->setSuffix(QStringLiteral(" MHz"));
    m_startSpin->setValue(462.0);

    m_endSpin = new QDoubleSpinBox(this);
    m_endSpin->setRange(0.01, 6000.0);
    m_endSpin->setDecimals(4);
    m_endSpin->setSuffix(QStringLiteral(" MHz"));
    m_endSpin->setValue(463.0);

    m_stepSpin = new QDoubleSpinBox(this);
    m_stepSpin->setRange(0.0001, 10.0);
    m_stepSpin->setDecimals(4);
    m_stepSpin->setSuffix(QStringLiteral(" MHz"));
    m_stepSpin->setValue(0.0125); // 12.5kHz, typical NFM channel spacing
    m_stepSpin->setSingleStep(0.0025);

    m_modulationCombo = new QComboBox(this);
    m_modulationCombo->addItems({QStringLiteral("NFM"), QStringLiteral("FM"), QStringLiteral("AM")});

    m_autoSquelchCheck = new QCheckBox(QStringLiteral("Auto squelch"), this);
    m_autoSquelchCheck->setChecked(true);

    m_squelchSpin = new QDoubleSpinBox(this);
    m_squelchSpin->setRange(-100.0, 0.0);
    m_squelchSpin->setDecimals(1);
    m_squelchSpin->setSuffix(QStringLiteral(" dBFS"));
    m_squelchSpin->setValue(-50.0);
    m_squelchSpin->setEnabled(false);
    connect(m_autoSquelchCheck, &QCheckBox::toggled, m_squelchSpin, [this](bool checked) {
        m_squelchSpin->setEnabled(!checked);
    });

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Start:"), m_startSpin);
    form->addRow(QStringLiteral("End:"), m_endSpin);
    form->addRow(QStringLiteral("Step:"), m_stepSpin);
    form->addRow(QStringLiteral("Mode:"), m_modulationCombo);
    form->addRow(QString(), m_autoSquelchCheck);
    form->addRow(QStringLiteral("Squelch:"), m_squelchSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral(
        "Sweeps from Start to End in Step increments. Frequencies within 2.4MHz\n"
        "of each other are still captured together for fast simultaneous checking."), this));
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QVector<Frequency> ExploreDialog::buildSweep() const
{
    QVector<Frequency> out;
    const double start = m_startSpin->value();
    const double end = m_endSpin->value();
    const double step = std::max(0.0001, m_stepSpin->value());
    if (end < start)
        return out;

    const Modulation mod = modulationFromString(m_modulationCombo->currentText());
    const bool autoSq = m_autoSquelchCheck->isChecked();
    const double squelchDb = m_squelchSpin->value();

    const int steps = static_cast<int>((end - start) / step + 0.5);
    for (int i = 0; i <= steps; ++i) {
        Frequency f;
        f.setMhz(start + i * step);
        f.label = QStringLiteral("Explore");
        f.modulation = mod;
        f.autoSquelch = autoSq;
        f.squelchDb = squelchDb;
        f.enabled = true;
        out.append(f);
    }
    return out;
}
