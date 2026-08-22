// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class DocumentController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY documentChanged)
    Q_PROPERTY(QString metadata READ metadata NOTIFY documentChanged)
    Q_PROPERTY(QStringList channels READ channels NOTIFY documentChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit DocumentController(QObject* parent = nullptr) : QObject(parent) {}

    QString title() const { return m_title; }
    QString metadata() const { return m_metadata; }
    QStringList channels() const { return m_channels; }
    QString error() const { return m_error; }

    Q_INVOKABLE void openCfg(const QUrl& url);

signals:
    void documentChanged();
    void errorChanged();

private:
    QString m_title{QStringLiteral("No record open")};
    QString m_metadata{QStringLiteral("Open a COMTRADE CFG to begin")};
    QStringList m_channels;
    QString m_error;
};
