// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFile>
#include <QString>
#include <QStringList>

#include <cmath>

namespace ardirec::desktop {

struct ClassicalGroundingFactors final {
    bool valid{false};
    double re_over_rl{0.0};
    double xe_over_xl{0.0};
};

inline ClassicalGroundingFactors read_classical_grounding_factors(const QString& path) {
    ClassicalGroundingFactors result;
    if (path.trimmed().isEmpty()) return result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;

    bool haveRe = false;
    bool haveXe = false;
    double re = 0.0;
    double xe = 0.0;

    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty()) continue;
        const QString upper = line.toUpper();

        if (upper.startsWith(QStringLiteral("RERL_XEXL"))) {
            QString values = line.mid(QStringLiteral("RERL_XEXL").size()).trimmed();
            const QStringList parts = values.split(',', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                bool okRe = false;
                bool okXe = false;
                const double parsedRe = parts.at(0).trimmed().toDouble(&okRe);
                const double parsedXe = parts.at(1).trimmed().toDouble(&okXe);
                if (okRe && okXe && std::isfinite(parsedRe) && std::isfinite(parsedXe)) {
                    return {true, parsedRe, parsedXe};
                }
            }
            continue;
        }

        if (upper.startsWith(QStringLiteral("RE/RL"))) {
            bool ok = false;
            const double value = line.mid(QStringLiteral("RE/RL").size()).trimmed().toDouble(&ok);
            if (ok && std::isfinite(value)) {
                re = value;
                haveRe = true;
            }
        } else if (upper.startsWith(QStringLiteral("XE/XL"))) {
            bool ok = false;
            const double value = line.mid(QStringLiteral("XE/XL").size()).trimmed().toDouble(&ok);
            if (ok && std::isfinite(value)) {
                xe = value;
                haveXe = true;
            }
        }
    }

    if (haveRe && haveXe) return {true, re, xe};
    return result;
}

} // namespace ardirec::desktop
