// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/channel_semantics.hpp"
#include "ardirec/comtrade/indexed_dat.hpp"
#include "ardirec/comtrade/parser.hpp"
#include "ardirec/comtrade/record.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

struct LoadedDocumentData;

class DocumentController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY documentChanged)
    Q_PROPERTY(QString metadata READ metadata NOTIFY documentChanged)
    Q_PROPERTY(QStringList channels READ channels NOTIFY documentChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString loadingStatus READ loadingStatus NOTIFY loadingChanged)
    Q_PROPERTY(qint64 lastLoadMilliseconds READ lastLoadMilliseconds NOTIFY loadingChanged)
    Q_PROPERTY(QString selectedSignal READ selectedSignal NOTIFY waveformChanged)
    Q_PROPERTY(int selectedAnalogIndex READ selectedAnalogIndex NOTIFY waveformChanged)
    Q_PROPERTY(int analogCount READ analogCount NOTIFY documentChanged)
    Q_PROPERTY(int digitalCount READ digitalCount NOTIFY documentChanged)
    Q_PROPERTY(int activeDigitalCount READ activeDigitalCount NOTIFY documentChanged)
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
    Q_PROPERTY(QStringList diagnostics READ diagnostics NOTIFY documentChanged)
    Q_PROPERTY(int diagnosticCount READ diagnosticCount NOTIFY documentChanged)
    Q_PROPERTY(QString valueRepresentation READ valueRepresentation NOTIFY representationChanged)
    Q_PROPERTY(bool transformerRatiosAvailable READ transformerRatiosAvailable NOTIFY documentChanged)
    Q_PROPERTY(QString transformerRatioSummary READ transformerRatioSummary NOTIFY documentChanged)
    Q_PROPERTY(QString distanceZonePath READ distanceZonePath NOTIFY documentChanged)
    Q_PROPERTY(QString headerSourceName READ headerSourceName NOTIFY documentChanged)
    Q_PROPERTY(QString headerText READ headerText NOTIFY documentChanged)
public:
    explicit DocumentController(QObject* parent = nullptr);
    ~DocumentController() override;

    QString title() const { return m_title; }
    QString metadata() const { return m_metadata; }
    QStringList channels() const { return m_channels; }
    QString error() const { return m_error; }
    bool loading() const { return m_loading; }
    QString loadingStatus() const { return m_loadingStatus; }
    qint64 lastLoadMilliseconds() const { return m_lastLoadMilliseconds; }
    QString selectedSignal() const { return m_selectedSignal; }
    int selectedAnalogIndex() const { return m_selectedAnalogIndex; }
    int analogCount() const { return m_analogCount; }
    int digitalCount() const { return m_digitalCount; }
    int activeDigitalCount() const { return m_activeDigitalCount; }
    qulonglong sampleCount() const { return static_cast<qulonglong>(timeSeconds().size()); }
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
    QStringList diagnostics() const { return m_diagnostics; }
    int diagnosticCount() const { return m_diagnostics.size(); }
    QString valueRepresentation() const { return m_valueRepresentation; }
    bool transformerRatiosAvailable() const { return m_transformerRatiosAvailable; }
    QString transformerRatioSummary() const { return m_transformerRatioSummary; }
    QString distanceZonePath() const { return m_distanceZonePath; }
    QString headerSourceName() const { return m_headerSourceName; }
    QString headerText() const { return m_headerText; }

    [[nodiscard]] const std::vector<double>& timeSeconds() const;
    [[nodiscard]] std::shared_ptr<const std::vector<double>> timeIndexSnapshot() const { return m_timeSeconds; }
    [[nodiscard]] std::shared_ptr<const ardirec::comtrade::IndexedDatFile> dataStoreSnapshot() const { return m_datStore; }
    [[nodiscard]] std::shared_ptr<const ardirec::comtrade::AnalogLodIndex> analogLodSnapshot() const {
        return m_datStore ? m_datStore->analogLodSnapshot() : nullptr;
    }
    [[nodiscard]] double recordedAnalogSampleAt(int channelIndex, std::size_t frameIndex) const noexcept;
    [[nodiscard]] bool recordedDigitalSampleAt(int channelIndex, std::size_t frameIndex) const noexcept;
    void copyRecordedAnalogRange(int channelIndex,
                                 std::size_t first,
                                 std::size_t end,
                                 std::vector<double>& destination) const;
    [[nodiscard]] std::pair<std::size_t, std::size_t> visibleSampleRange(double zoomFactor,
                                                                        double panFraction) const;

    Q_INVOKABLE void openCfg(const QUrl& url);
    Q_INVOKABLE void selectChannel(int index);
    Q_INVOKABLE void setValueRepresentation(const QString& representation);
    Q_INVOKABLE QString channelName(int index) const;
    Q_INVOKABLE QString channelUnit(int index) const;
    Q_INVOKABLE QString analogRole(int index) const;
    Q_INVOKABLE QString channelPhase(int index) const;
    Q_INVOKABLE double channelPeak(int index) const;
    Q_INVOKABLE double channelDisplayScale(int index) const;
    Q_INVOKABLE QString channelRatioText(int index) const;
    Q_INVOKABLE double sampleValue(int channelIndex,
                                   double absoluteTimeSeconds) const;
    Q_INVOKABLE QString sampleValueText(int channelIndex,
                                        double absoluteTimeSeconds) const;
    Q_INVOKABLE QString formatChannelValue(int channelIndex, double value) const;
    Q_INVOKABLE QString digitalName(int index) const;
    Q_INVOKABLE bool digitalIsActive(int index) const;
    Q_INVOKABLE bool digitalStateAt(int index, double absoluteTimeSeconds) const;
    Q_INVOKABLE QString digitalStateText(int index, double absoluteTimeSeconds) const;
    Q_INVOKABLE double snapToDigitalEdge(double absoluteTimeSeconds,
                                         double maxDistanceSeconds) const;

signals:
    void documentChanged();
    void waveformChanged();
    void representationChanged();
    void errorChanged();
    void loadingChanged();

private:
    void applyLoadedDocument(const std::shared_ptr<LoadedDocumentData>& loaded,
                             quint64 generation);
    void rebuildTransformerSummary();
    [[nodiscard]] std::size_t nearestSampleIndex(double absoluteTimeSeconds) const;

    QString m_title{QStringLiteral("No record open")};
    QString m_metadata{QStringLiteral("Open a COMTRADE CFG to begin")};
    QStringList m_channels;
    QStringList m_channelNames;
    QStringList m_channelUnits;
    QStringList m_statusNames;
    QStringList m_diagnostics;
    QString m_error;
    QString m_loadingStatus;
    QString m_selectedSignal{QStringLiteral("No signal")};
    QString m_recorderId{QStringLiteral("—")};
    QString m_revisionText{QStringLiteral("—")};
    QString m_dataFormatText{QStringLiteral("—")};
    QString m_startTimeText{QStringLiteral("—")};
    QString m_triggerTimeText{QStringLiteral("—")};
    QString m_recordHealth{QStringLiteral("No record")};
    QString m_valueRepresentation{QStringLiteral("secondary")};
    QString m_transformerRatioSummary{QStringLiteral("No CT/PT ratio metadata")};
    QString m_distanceZonePath;
    QString m_headerSourceName;
    QString m_headerText;
    int m_selectedAnalogIndex{-1};
    int m_analogCount{0};
    int m_digitalCount{0};
    int m_activeDigitalCount{0};
    double m_nominalFrequency{0.0};
    double m_triggerOffsetSeconds{0.0};
    bool m_transformerRatiosAvailable{false};
    bool m_loading{false};
    qint64 m_lastLoadMilliseconds{0};
    QElapsedTimer m_loadTimer;
    quint64 m_loadGeneration{0};
    std::shared_ptr<std::atomic_bool> m_activeLoadCancel;
    std::shared_ptr<const ardirec::comtrade::IndexedDatFile> m_datStore;
    std::shared_ptr<const std::vector<double>> m_timeSeconds{std::make_shared<const std::vector<double>>()};
    std::vector<ardirec::comtrade::AnalogChannel> m_channelConfigs;
    std::vector<ardirec::comtrade::AnalogRole> m_analogRoles;
    std::vector<ardirec::comtrade::PhaseRole> m_phaseRoles;
    std::vector<std::uint8_t> m_statusActive;
    std::vector<int> m_statusNormalState;
    std::vector<double> m_digitalEdgeTimes;
    std::vector<double> m_channelPeaks;
};