// SPDX-License-Identifier: GPL-3.0-or-later
#include "digital_item.hpp"
#include "document_controller.hpp"
#include "waveform_item.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <qqml.h>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ardirec"));
    QGuiApplication::setOrganizationName(QStringLiteral("ardirec"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.2.0-alpha.5"));

    qmlRegisterType<WaveformItem>("Ardirec.Render", 1, 0, "WaveformItem");
    qmlRegisterType<DigitalItem>("Ardirec.Render", 1, 0, "DigitalItem");

    DocumentController document;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("documentController"), &document);
    engine.loadFromModule("Ardirec", "Main");
    if (engine.rootObjects().isEmpty()) return -1;
    return app.exec();
}
