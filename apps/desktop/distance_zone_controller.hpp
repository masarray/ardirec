// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/distance/rio.hpp"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <complex>

class DistanceZoneController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY modelChanged)
    Q_PROPERTY(QString status READ status NOTIFY modelChanged)
    Q_PROPERTY(QString compatibilityWarning READ compatibilityWarning NOTIFY modelChanged)
    Q_PROPERTY(bool hasZones READ hasZones NOTIFY modelChanged)
    Q_PROPERTY(int zoneCount READ zoneCount NOTIFY modelChanged)
    Q_PROPERTY(QString zoneNativeRepresentation READ zoneNativeRepresentation NOTIFY modelChanged)
    Q_PROPERTY(bool zoneBaseConversionAvailable READ zoneBaseConversionAvailable NOTIFY modelChanged)
    Q_PROPERTY(double groundingFactorMagnitude READ groundingFactorMagnitude WRITE setGroundingFactorMagnitude NOTIFY groundingFactorChanged)
    Q_PROPERTY(double groundingFactorAngle READ groundingFactorAngle WRITE setGroundingFactorAngle NOTIFY groundingFactorChanged)
    Q_PROPERTY(bool groundingFactorValid READ groundingFactorValid NOTIFY groundingFactorChanged)
    Q_PROPERTY(QString groundingFactorSource READ groundingFactorSource NOTIFY groundingFactorChanged)

public:
    explicit DistanceZoneController(QObject* parent = nullptr);

    [[nodiscard]] QString sourceName() const { return m_sourceName; }
    [[nodiscard]] QString status() const { return m_status; }
    [[nodiscard]] QString compatibilityWarning() const { return m_compatibilityWarning; }
    [[nodiscard]] bool hasZones() const { return m_model.valid && !m_model.zones.empty(); }
    [[nodiscard]] int zoneCount() const { return static_cast<int>(m_model.zones.size()); }
    [[nodiscard]] QString zoneNativeRepresentation() const;
    [[nodiscard]] bool zoneBaseConversionAvailable() const { return m_model.impedance_base_conversion_valid(); }

    [[nodiscard]] double groundingFactorMagnitude() const;
    [[nodiscard]] double groundingFactorAngle() const;
    [[nodiscard]] bool groundingFactorValid() const { return m_groundingFactorValid; }
    [[nodiscard]] QString groundingFactorSource() const { return m_groundingFactorSource; }
    [[nodiscard]] std::complex<double> groundingFactor() const { return m_groundingFactor; }

    void setGroundingFactorMagnitude(double value);
    void setGroundingFactorAngle(double value);

    Q_INVOKABLE bool openFile(const QUrl& fileUrl);
    Q_INVOKABLE void clearZones();
    Q_INVOKABLE QVariantList zonesForLoop(const QString& loopId, const QString& valueRepresentation) const;

signals:
    void modelChanged();
    void groundingFactorChanged();

private:
    bool loadXrio(const QByteArray& data);
    void acceptModel(ardirec::distance::RioDistanceModel model,
                     const QString& sourceName,
                     const QString& adapterWarning = {});

    ardirec::distance::RioDistanceModel m_model;
    QString m_sourceName;
    QString m_status{QStringLiteral("No distance zones loaded")};
    QString m_compatibilityWarning;
    std::complex<double> m_groundingFactor{};
    bool m_groundingFactorValid{};
    QString m_groundingFactorSource{QStringLiteral("manual")};
};
