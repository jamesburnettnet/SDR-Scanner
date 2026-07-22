#include "ScanGroup.h"
#include <algorithm>

namespace ScanGrouping {

QVector<ScanGroup> buildGroups(const QVector<Frequency> &frequencies)
{
    QVector<Frequency> sorted = frequencies;
    std::sort(sorted.begin(), sorted.end(), [](const Frequency &a, const Frequency &b) {
        return a.hz < b.hz;
    });

    QVector<ScanGroup> groups;
    int i = 0;
    while (i < sorted.size()) {
        qint64 minHz = sorted[i].hz;
        qint64 maxHz = sorted[i].hz;
        int j = i;
        // Greedily extend the group while every member stays within the
        // usable span of the eventual center point.
        while (j + 1 < sorted.size()) {
            qint64 candidateMax = sorted[j + 1].hz;
            qint64 span = candidateMax - minHz;
            if (span > 2 * kUsableHalfSpanHz)
                break;
            maxHz = candidateMax;
            ++j;
        }

        ScanGroup group;
        group.centerHz = (minHz + maxHz) / 2;
        for (int k = i; k <= j; ++k) {
            GroupChannel ch;
            ch.freq = sorted[k];
            ch.offsetHz = sorted[k].hz - group.centerHz;
            group.channels.append(ch);
        }
        groups.append(group);
        i = j + 1;
    }

    return groups;
}

} // namespace ScanGrouping
