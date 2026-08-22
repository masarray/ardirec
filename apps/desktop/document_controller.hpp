// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <cstddef>
#include <utility>
#include <vector>

class DocumentController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY documentChanged)
    Q_PROPERTY(QString metadata READ metadata NOTIFY documentChanged)
    Q_PROPERTY(QStringList channels READ channels NOTIFY documentChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString selectedSignal READ selectedSignal NOTIFY waveformChanged)
    Q_PROPERTY(int selectedAnalogIndex READ selectedAnalogIndex NOTIFY waveformChanged)
    Q_PROPERTY(int analogCount READ analogCount NOTIFY documentChanged)
    Q_PROPERTY(qulonglong sampleCount READ sampleCount NOTIFY documentChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY documentChanged)
    Q_PROPERTY(double dataStartSeconds READ dataStartSeconds NOTIFY documentChanged)
    Q_PROPERTY(double dataEndSeconds READ dataEndSeconds NOTIFY documentChanged)
    Q_PROPERTY(double triggerOffsetSeconds READ triggerOffsetSeconds NOTIFY documentChanged)
    Q_PROPERTY(QString recorderId READ recorderId NOTIFY documentChanged)
    Q_PROPERTY(QString revisionText READ revisionText NOTIFY documentChanged)
    Q_PROPERTY(QString dataFormatText READ dataFormatText NOTIFY documentChanged)
    Q_PROPERTY(double nominalFrequency READ nominalFrequency NOTIFY documentChanged)
    Q_PROPERTY(QString startTimeText READ startTimeText NOTIFY documentChanged)
    Q_PROPERTY(QString triggerTimeText READ triggerTimeText NOTIFY documentChanged)
    Q_PROPERTY(QString recordHealth READ recordHealth NOTIFY documentChanged)
public:
    explicit DocumentController(QObject* parent = nullptr) : QObject(parent) {}

    QString title() const { return m_title; }
    QString metadata() const { return m_metadata; }
    QStringList channels() const { return m_channels; }
    QString error() const { return m_error; }
    QString selectedSignal() const { return m_selectedSignal; }
    int selectedAnalogIndex() const { return m_selectedAnalogIndex; }
    int analogCount() const { return m_analogCount; }
    qulonglong sampleCount() const { return static_cast<qulonglong>(m_timeSeconds.size()); }
    double durationSeconds() const;
    double dataStartSeconds() const;
    double dataEndSeconds() const;
    double triggerOffsetSeconds() const { return m_triggerOffsetSeconds; }
    QString recorderId() const { return m_recorderId; }
    QString revisionText() const { return m_revisionText; }
    QString dataFormatText() const { return m_dataFormatText; }
    double nominalFrequency() const { return m_nominalFrequency; }
    QString startTimeText() const { return m_startTimeText; }
    QString triggerTimeText() const { return m_triggerTimeText; }
    QString recordHealth() const { return m_recordHealth; }

    [[nodiscard]] const std::vector<double>& selectedSamples() const { return m_selectedSamples; }
    [[nodiscard]] const std::vector<double>& timeSeconds() const { return m_timeSeconds; }
    [[nodiscard]] const std::vector<double>& analogSamples(int index) const;
    [[nodiscard]] std::pair<std::size_t, std::size_t> visibleSampleRange(double zoomFactor,
                                                                        double panFraction) const;

    Q_INVOKABLE void openCfg(const QUrl& url);
    Q_INVOKABLE void selectChannel(int index);
    Q_INVOKABLE QString channelName(int index) const;
    Q_INVOKABLE QString channelUnit(int index) const;
    Q_INVOKABLE double channelPeak(int index) const;
    Q_INVOKABLE double sampleValue(int channelIndex,
                                   double absoluteTimeSeconds) const;
    Q_INVOKABLE QString sampleValueText(int channelIndex,
                                        double absoluteTimeSeconds) const;

signals:
    void documentChanged();
    void waveformChanged();
    void errorChanged();

private:
    void rebuildSelectedSamples();

    QString m_title{QStringLiteral("No record open")};
    QString m_metadata{QStringLiteral("Open a COMTRADE CFG to begin")};
    QStringList m_channels;
    QStringList m_channelNames;
    QStringList m_channelUnits;
    QString m_error;
    QString m_selectedSignal{QStringLiteral("No signal")};
    QString m_recorderId{QStringLiteral("—")};
    QString m_revisionText{QStringLiteral("—")};
    QString m_dataFormatText{QStringLiteral("—")};
    QString m_startTimeText{QStringLiteral("—")};
    QString m_triggerTimeText{QStringLiteral("—")};
    QString m_recordHealth{QStringLiteral("No record")};
    int m_selectedAnalogIndex{-1};
    int m_analogCount{0};
    double m_nominalFrequency{0.0};
    double m_triggerOffsetSeconds{0.0};
    std::vector<std::vector<double>> m_analogSamples;
    std::vector<double> m_channelPeaks;
    std::vector<double> m_selectedSamples;
    std::vector<double> m_timeSeconds;
};
