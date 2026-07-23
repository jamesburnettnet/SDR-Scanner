#pragma once

#include <QWidget>
#include <QSortFilterProxyModel>
#include <functional>
#include "../core/FrequencyListModel.h"

class QTableView;
class QPushButton;
class AddFrequencyDialog;

// Composite widget: the frequency table plus Add/Edit/Delete/Import/Export
// controls. Owns the FrequencyListModel. Sortable by clicking a column
// header (e.g. Frequency or Label).
class FrequencyTableView : public QWidget {
    Q_OBJECT
public:
    // Result callback for a calibration request: ok, measured noise floor
    // (dBFS), suggested squelch (dBFS), and an error message if !ok.
    using CalibrationResultFn = std::function<void(bool ok, double noiseFloorDb, double suggestedSquelchDb,
                                                     QString error)>;
    // Performs a one-shot squelch calibration at freqHz/modulation and
    // invokes onDone (on the GUI thread) when finished. Supplied by
    // whoever owns the live SDR device (MainWindow), since this widget
    // only knows about the frequency list, not the device.
    using CalibrationRequestFn = std::function<void(qint64 freqHz, Modulation modulation, CalibrationResultFn onDone)>;

    explicit FrequencyTableView(QWidget *parent = nullptr);

    FrequencyListModel *model() { return &m_model; }
    QList<int> selectedRows() const;

    void setCalibrationHandler(CalibrationRequestFn fn) { m_calibrationFn = std::move(fn); }

    // Highlights (and scrolls to) the row for this Frequency id, e.g.
    // during an active call; pass a null QUuid to clear the highlight.
    void setActiveFrequencyId(const QUuid &id);

signals:
    void listChanged();

private:
    void addFrequency();
    void editSelected();
    void deleteSelected();
    void importList();
    void exportList();
    void wireCalibration(AddFrequencyDialog &dlg);
    QList<int> visibleRows() const;
    void setEnabledForVisible(bool enabled);

    FrequencyListModel m_model;
    QSortFilterProxyModel m_proxy;
    QTableView *m_table;
    CalibrationRequestFn m_calibrationFn;
};
