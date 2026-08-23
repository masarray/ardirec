// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/distance/distance.hpp"
#include "ardirec/power/harmonics.hpp"
#include "document_controller.hpp"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <complex>
#include <utility>

class AnalysisController final : public QObject {
    Q_OBJECT
public:
    explicit AnalysisController(DocumentController* document, QObject* parent = nullptr);

    Q_INVOKABLE QString channelPhase(int channelIndex) const;
    Q_INVOKABLE QString phaseColor(int channelIndex) const;
    Q_INVOKABLE QString phaseColorForName(const QString& phase) const;
    Q_INVOKABLE int phaseChannel(const QString& role, const QString& phase) const;

    Q_INVOKABLE double rmsValue(int channelIndex, double absoluteTimeSeconds) const;
    Q_INVOKABLE QString rmsValueText(int channelIndex, double absoluteTimeSeconds) const;
    Q_INVOKABLE QVariantMap phasorAt(int channelIndex, double absoluteTimeSeconds) const;
    Q_INVOKABLE QVariantMap harmonicSpectrumAt(int channelIndex,
                                               double absoluteTimeSeconds,
                                               int maximumOrder = 15) const;
    Q_INVOKABLE QVariantList harmonicsAt(int channelIndex,
                                         double absoluteTimeSeconds,
                                         int maximumOrder = 15) const;
    Q_INVOKABLE QVariantMap impedanceAt(int voltageChannelIndex,
                                        int currentChannelIndex,
                                        double absoluteTimeSeconds) const;
    Q_INVOKABLE bool distanceLoopAvailable(const QString& loopId) const;
    Q_INVOKABLE QVariantMap distanceLoopAt(const QString& loopId,
                                           double absoluteTimeSeconds,
                                           double groundingFactorMagnitude,
                                           double groundingFactorAngleDegrees) const;
    Q_INVOKABLE QVariantList distanceLocus(const QString& loopId,
                                           double viewStartSeconds,
                                           double visibleDurationSeconds,
                                           int steps,
                                           double groundingFactorMagnitude,
                                           double groundingFactorAngleDegrees) const;

private:
    [[nodiscard]] std::pair<std::size_t, std::size_t> oneCycleWindow(double absoluteTimeSeconds) const;
    [[nodiscard]] std::complex<double> phasorComplex(int channelIndex,
                                                     double absoluteTimeSeconds,
                                                     int harmonicOrder = 1) const;
    [[nodiscard]] ardirec::power::HarmonicSpectrum harmonicSpectrum(int channelIndex,
                                                                    double absoluteTimeSeconds,
                                                                    int maximumOrder) const;
    [[nodiscard]] bool distancePhasors(ardirec::distance::FaultLoop loop,
                                       double absoluteTimeSeconds,
                                       ardirec::distance::ThreePhasePhasors& phasors) const;
    [[nodiscard]] double unitScaleToSi(int channelIndex) const;
    [[nodiscard]] QString formatEngineeringValue(double value, int channelIndex) const;

    QPointer<DocumentController> m_document;
};
