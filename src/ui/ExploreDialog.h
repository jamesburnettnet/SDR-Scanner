#pragma once

#include <QDialog>
#include <QVector>
#include "../core/Frequency.h"

class QDoubleSpinBox;
class QComboBox;
class QCheckBox;

// Explore mode setup: sweep between two frequencies at a custom step,
// generating a synthetic frequency list that reuses the same grouped
// scan engine (and thus the same 2.4MHz-window speed benefit) as a
// regular saved-list scan.
class ExploreDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExploreDialog(QWidget *parent = nullptr);

    // Builds one Frequency per step from startMHz to endMHz (inclusive).
    QVector<Frequency> buildSweep() const;

private:
    QDoubleSpinBox *m_startSpin;
    QDoubleSpinBox *m_endSpin;
    QDoubleSpinBox *m_stepSpin;
    QComboBox *m_modulationCombo;
    QCheckBox *m_autoSquelchCheck;
    QDoubleSpinBox *m_squelchSpin;
};
