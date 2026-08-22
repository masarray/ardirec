// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <cstddef>
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
    QString recordHealth() const { return m_recordHealth; }

    [[nodiscard]] const std::vector<double>& selectedSamples() const { return m_selectedSamples; }
    [[nodiscard]] const std::vector<double>& timeSeconds() const { return m_timeSeconds; }

    Q_INVOKABLE void openCfg(const QUrl& url);
    Q_INVOKABLE void selectChannel(int index);

signals:
    void documentChanged();
    void waveformChanged();
    void errorChanged();

private:
    void rebuildSelectedSamples();

    QString m_title{QStringLiteral("No record open")};
    QString m_metadata{QStringLiteral("Open a COMTRADE CFG to begin")};
    QStringList m_channels;
    QString m_error;
    QString m_selectedSignal{QStringLiteral("No signal")};
    QString m_recordHealth{QStringLiteral("No record")};
    int m_selectedAnalogIndex{-1};
    int m_analogCount{0};
    std::vector<std::vector<double>> m_analogSamples;
    std::vector<double> m_selectedSamples;
    std::vector<double> m_timeSeconds;
};
