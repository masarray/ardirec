// SPDX-License-Identifier: GPL-3.0-or-later
#include "distance_zone_controller.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) throw std::runtime_error(message);
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        const QByteArray xrio = R"XRIO(<?xml version="1.0" encoding="UTF-8"?>
<XRIO>
  <Block ID="RIO">
    <Block ID="DEVICE">
      <Parameter ID="DEVICE_MODEL"><Value>P2 XRIO TEST</Value></Parameter>
      <Parameter ID="VNOM"><Value>110</Value></Parameter>
      <Parameter ID="VPRIM_LL"><Value>110000</Value></Parameter>
      <Parameter ID="INOM"><Value>1</Value></Parameter>
      <Parameter ID="IPRIM"><Value>500</Value></Parameter>
    </Block>
    <Block ID="DISTANCE">
      <Block ID="PROTECTEDOBJECT">
        <Parameter ID="LINEANGLE"><Value>75</Value></Parameter>
        <Parameter ID="GRF_MODE"><Value>Z0Z1</Value></Parameter>
        <Parameter ID="Z0Z1_MAG"><Value>4</Value></Parameter>
        <Parameter ID="Z0Z1_ANGLE"><Value>0</Value></Parameter>
      </Block>
      <Block ID="PROTECTIONDEVICE">
        <Parameter ID="IMPPRIM"><Value>false</Value></Parameter>
        <Block ID="ZONES">
          <Block ID="ZONE">
            <Parameter ID="INDEX"><Value>1</Value></Parameter>
            <Parameter ID="LABEL"><Value>Z1 LL</Value></Parameter>
            <Parameter ID="TYPE"><Value>TRIPPING</Value></Parameter>
            <Parameter ID="FAULTLOOP"><Value>LL</Value></Parameter>
            <Parameter ID="TRIPTIME"><Value>0.03</Value></Parameter>
            <Parameter ID="ACTIVE"><Value>true</Value></Parameter>
            <Block ID="MHOSHAPE">
              <Parameter ID="ANGLE"><Value>75</Value></Parameter>
              <Parameter ID="REACH"><Value>4</Value></Parameter>
              <Parameter ID="OFFSET"><Value>0</Value></Parameter>
              <Parameter ID="INVERT"><Value>false</Value></Parameter>
            </Block>
          </Block>
        </Block>
      </Block>
    </Block>
  </Block>
</XRIO>)XRIO";

        QTemporaryDir directory;
        require(directory.isValid(), "temporary directory");
        const QString path = directory.filePath(QStringLiteral("relay.xrio"));
        QFile file(path);
        require(file.open(QIODevice::WriteOnly), "open temporary XRIO");
        require(file.write(xrio) == xrio.size(), "write temporary XRIO");
        file.close();

        DistanceZoneController zones;
        require(zones.openFile(QUrl::fromLocalFile(path)), "standardized XRIO imports");
        require(zones.hasZones(), "XRIO exposes zones");
        require(zones.zoneCount() == 1, "XRIO zone count");
        require(zones.zoneNativeRepresentation() == QStringLiteral("SECONDARY"), "XRIO IMPPRIM false");
        require(zones.zoneBaseConversionAvailable(), "XRIO CT/VT conversion metadata");
        require(zones.groundingFactorValid(), "XRIO grounding factor imported");
        require_near(zones.groundingFactorMagnitude(), 1.0, 1.0e-12, "XRIO Z0/Z1 -> kL magnitude");
        require_near(zones.groundingFactorAngle(), 0.0, 1.0e-12, "XRIO Z0/Z1 -> kL angle");

        const QVariantList secondary = zones.zonesForLoop(QStringLiteral("L1-L2"), QStringLiteral("secondary"));
        require(secondary.size() == 1, "XRIO LL zone matches L1-L2");
        const QVariantMap secondaryZone = secondary.front().toMap();
        require(secondaryZone.value(QStringLiteral("kind")).toString() == QStringLiteral("circle"), "XRIO mho becomes circle");
        require_near(secondaryZone.value(QStringLiteral("radius")).toDouble(), 2.0, 1.0e-12, "XRIO mho secondary radius");

        const QVariantList primary = zones.zonesForLoop(QStringLiteral("L1-L2"), QStringLiteral("primary"));
        require(primary.size() == 1, "XRIO primary zone available");
        require_near(primary.front().toMap().value(QStringLiteral("radius")).toDouble(), 4.0, 1.0e-12,
                     "XRIO zone scales with CT/VT base");
        require(zones.compatibilityWarning().contains(QStringLiteral("Custom formulas")),
                "XRIO import explicitly reports Custom formula limitation");

        std::cout << "ardirec XRIO tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec XRIO tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
