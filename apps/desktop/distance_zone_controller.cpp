// SPDX-License-Identifier: GPL-3.0-or-later
#include "distance_zone_controller.hpp"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QVariantMap>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

QString canonicalId(QString value) {
    value = value.trimmed().toUpper();
    QString result;
    result.reserve(value.size());
    for (const QChar ch : value) {
        if (ch.isLetterOrNumber()) result.append(ch);
    }
    return result;
}

struct XrioNode {
    QString id;
    QString text;
    std::vector<XrioNode> children;
};

QString attributeValue(const QXmlStreamAttributes& attributes, const QString& wanted) {
    for (const auto& attribute : attributes) {
        if (attribute.name().compare(wanted, Qt::CaseInsensitive) == 0) return attribute.value().toString();
    }
    return {};
}

QString nodeValue(const XrioNode& node);

const XrioNode* directChild(const XrioNode& node, const QString& wanted) {
    const QString target = canonicalId(wanted);
    for (const auto& child : node.children) {
        if (canonicalId(child.id) == target) return &child;
    }
    return nullptr;
}

QString nodeValue(const XrioNode& node) {
    const QString direct = node.text.trimmed();
    if (!direct.isEmpty()) return direct;
    for (const QString& valueId : {QStringLiteral("VALUE"), QStringLiteral("CURRENTVALUE"), QStringLiteral("VAL")}) {
        if (const auto* child = directChild(node, valueId)) {
            const QString value = nodeValue(*child).trimmed();
            if (!value.isEmpty()) return value;
        }
    }
    return {};
}

XrioNode readNode(QXmlStreamReader& xml) {
    XrioNode node;
    const QString elementName = xml.name().toString();
    QString id = attributeValue(xml.attributes(), QStringLiteral("ID"));
    if (id.isEmpty()) id = attributeValue(xml.attributes(), QStringLiteral("Id"));
    if (id.isEmpty()) id = attributeValue(xml.attributes(), QStringLiteral("id"));
    if (id.isEmpty()) id = elementName;
    node.id = id;

    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            node.children.push_back(readNode(xml));
        } else if (token == QXmlStreamReader::Characters || token == QXmlStreamReader::EntityReference) {
            if (!xml.isWhitespace()) node.text += xml.text().toString();
        } else if (token == QXmlStreamReader::EndElement) {
            break;
        }
    }

    const QString generic = canonicalId(node.id);
    if (generic == QStringLiteral("BLOCK") || generic == QStringLiteral("PARAMETER")
        || generic == QStringLiteral("ROW") || generic == QStringLiteral("GROUP")) {
        if (const auto* idChild = directChild(node, QStringLiteral("ID"))) {
            const QString fixedId = nodeValue(*idChild).trimmed();
            if (!fixedId.isEmpty()) node.id = fixedId;
        } else {
            const QString name = attributeValue({}, QStringLiteral("NAME"));
            if (!name.isEmpty()) node.id = name;
        }
    }
    return node;
}

const XrioNode* findDescendant(const XrioNode& node, const QString& wanted) {
    const QString target = canonicalId(wanted);
    if (canonicalId(node.id) == target) return &node;
    for (const auto& child : node.children) {
        if (const auto* found = findDescendant(child, wanted)) return found;
    }
    return nullptr;
}

QString descendantValue(const XrioNode& node, const std::initializer_list<QString>& ids) {
    for (const auto& id : ids) {
        if (const auto* found = findDescendant(node, id)) {
            const QString value = nodeValue(*found).trimmed();
            if (!value.isEmpty()) return value;
        }
    }
    return {};
}

QString childValue(const XrioNode& node, const std::initializer_list<QString>& ids) {
    for (const auto& id : ids) {
        if (const auto* found = directChild(node, id)) {
            const QString value = nodeValue(*found).trimmed();
            if (!value.isEmpty()) return value;
        }
    }
    return {};
}

void collectDescendants(const XrioNode& node, const QString& wanted, std::vector<const XrioNode*>& output) {
    const QString target = canonicalId(wanted);
    for (const auto& child : node.children) {
        if (canonicalId(child.id) == target) output.push_back(&child);
        collectDescendants(child, wanted, output);
    }
}

void writeParameter(QTextStream& out, const QString& key, const QString& value) {
    if (!value.trimmed().isEmpty()) out << key << ' ' << value.trimmed() << '\n';
}

QString boolValue(const QString& value, const QString& fallback = QStringLiteral("NO")) {
    if (value.isEmpty()) return fallback;
    const QString normalized = value.trimmed().toUpper();
    return normalized == QStringLiteral("TRUE") || normalized == QStringLiteral("YES") || normalized == QStringLiteral("1")
               ? QStringLiteral("YES") : QStringLiteral("NO");
}

QString quoteRio(QString value) {
    value.replace('"', '\'');
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}

std::optional<ardirec::distance::RioDistanceModel> adaptStandardXrio(const QByteArray& data, QString& diagnostic) {
    QXmlStreamReader xml(data);
    while (!xml.atEnd() && xml.readNext() != QXmlStreamReader::StartElement) {}
    if (xml.atEnd() || xml.hasError()) {
        diagnostic = QStringLiteral("XRIO XML could not be parsed: %1").arg(xml.errorString());
        return std::nullopt;
    }

    XrioNode root = readNode(xml);
    if (xml.hasError()) {
        diagnostic = QStringLiteral("XRIO XML could not be parsed: %1").arg(xml.errorString());
        return std::nullopt;
    }

    const XrioNode* rio = findDescendant(root, QStringLiteral("RIO"));
    if (!rio) {
        diagnostic = QStringLiteral("XRIO has no standardized RIO section. Vendor Custom converter formulas are intentionally not evaluated.");
        return std::nullopt;
    }
    const XrioNode* distance = findDescendant(*rio, QStringLiteral("DISTANCE"));
    if (!distance) {
        diagnostic = QStringLiteral("XRIO RIO section has no DISTANCE block.");
        return std::nullopt;
    }

    const XrioNode* device = findDescendant(*rio, QStringLiteral("DEVICE"));
    const XrioNode* protectedObject = findDescendant(*distance, QStringLiteral("PROTECTEDOBJECT"));
    const XrioNode* protectionDevice = findDescendant(*distance, QStringLiteral("PROTECTIONDEVICE"));
    if (!protectedObject) protectedObject = distance;
    if (!protectionDevice) protectionDevice = distance;

    QString rioText;
    QTextStream out(&rioText);
    out << "BEGIN TESTOBJECT\nBEGIN DEVICE\n";
    if (device) {
        QString deviceName = descendantValue(*device, {QStringLiteral("DEVICE_MODEL"), QStringLiteral("DEVICEMODEL"), QStringLiteral("NAME")});
        if (!deviceName.isEmpty()) writeParameter(out, QStringLiteral("NAME"), quoteRio(deviceName));
        writeParameter(out, QStringLiteral("VNOM"), descendantValue(*device, {QStringLiteral("VNOM")}));
        writeParameter(out, QStringLiteral("VPRIM-LL"), descendantValue(*device, {QStringLiteral("VPRIM_LL"), QStringLiteral("VPRIMLL"), QStringLiteral("VPRIM")}));
        writeParameter(out, QStringLiteral("INOM"), descendantValue(*device, {QStringLiteral("INOM")}));
        writeParameter(out, QStringLiteral("IPRIM"), descendantValue(*device, {QStringLiteral("IPRIM")}));
    }
    out << "END DEVICE\nBEGIN DISTANCE\n";

    writeParameter(out, QStringLiteral("LINEANGLE"), descendantValue(*protectedObject, {QStringLiteral("LINEANGLE")}));
    writeParameter(out, QStringLiteral("IMPPRIM"), boolValue(descendantValue(*protectionDevice, {QStringLiteral("IMPPRIM")})));

    const QString grfMode = descendantValue(*protectedObject, {QStringLiteral("GRF_MODE"), QStringLiteral("GRFMODE")}).trimmed().toUpper();
    if (grfMode == QStringLiteral("Z0Z1")) {
        const QString mag = descendantValue(*protectedObject, {QStringLiteral("Z0Z1_MAG"), QStringLiteral("Z0Z1MAG")});
        const QString angle = descendantValue(*protectedObject, {QStringLiteral("Z0Z1_ANGLE"), QStringLiteral("Z0Z1ANGLE")});
        if (!mag.isEmpty() && !angle.isEmpty()) out << "Z0Z1 " << mag << ", " << angle << '\n';
    } else if (grfMode == QStringLiteral("ZNR_ZNX") || grfMode == QStringLiteral("ZNRZNX")) {
        const QString reRl = descendantValue(*protectedObject, {QStringLiteral("ZN_R"), QStringLiteral("ZNR")});
        const QString xeXl = descendantValue(*protectedObject, {QStringLiteral("ZN_X"), QStringLiteral("ZNX")});
        if (!reRl.isEmpty() && !xeXl.isEmpty()) out << "RERL_XEXL " << reRl << ", " << xeXl << '\n';
    } else {
        const QString mag = descendantValue(*protectedObject, {QStringLiteral("KL_MAG"), QStringLiteral("KLMAG")});
        const QString angle = descendantValue(*protectedObject, {QStringLiteral("KL_ANGLE"), QStringLiteral("KLANGLE")});
        if (!mag.isEmpty() && !angle.isEmpty()) out << "KL " << mag << ", " << angle << '\n';
    }

    const XrioNode* zones = findDescendant(*protectionDevice, QStringLiteral("ZONES"));
    if (!zones) zones = findDescendant(*distance, QStringLiteral("ZONES"));
    std::vector<const XrioNode*> zoneNodes;
    if (zones) collectDescendants(*zones, QStringLiteral("ZONE"), zoneNodes);

    for (const XrioNode* zone : zoneNodes) {
        out << "BEGIN ZONE\n";
        writeParameter(out, QStringLiteral("INDEX"), childValue(*zone, {QStringLiteral("INDEX")}));
        writeParameter(out, QStringLiteral("LABEL"), quoteRio(childValue(*zone, {QStringLiteral("LABEL")})));
        writeParameter(out, QStringLiteral("TYPE"), childValue(*zone, {QStringLiteral("TYPE")}));
        writeParameter(out, QStringLiteral("FAULTLOOP"), childValue(*zone, {QStringLiteral("FAULTLOOP")}));
        writeParameter(out, QStringLiteral("TRIPTIME"), childValue(*zone, {QStringLiteral("TRIPTIME")}));
        writeParameter(out, QStringLiteral("ACTIVE"), boolValue(childValue(*zone, {QStringLiteral("ACTIVE")}), QStringLiteral("YES")));

        if (const auto* mho = findDescendant(*zone, QStringLiteral("MHOSHAPE"))) {
            out << "BEGIN MHOSHAPE\n";
            writeParameter(out, QStringLiteral("ANGLE"), childValue(*mho, {QStringLiteral("ANGLE")}));
            writeParameter(out, QStringLiteral("REACH"), childValue(*mho, {QStringLiteral("REACH")}));
            writeParameter(out, QStringLiteral("OFFSET"), childValue(*mho, {QStringLiteral("OFFSET")}));
            writeParameter(out, QStringLiteral("INVERT"), boolValue(childValue(*mho, {QStringLiteral("INVERT")})));
            out << "END MHOSHAPE\n";
        } else if (const auto* generic = findDescendant(*zone, QStringLiteral("GENERICSHAPE"))) {
            out << "BEGIN SHAPE\n";
            std::vector<const XrioNode*> lines;
            std::vector<const XrioNode*> arcs;
            collectDescendants(*generic, QStringLiteral("LINE"), lines);
            collectDescendants(*generic, QStringLiteral("ARC"), arcs);
            for (const auto* line : lines) {
                const QString refType = childValue(*line, {QStringLiteral("REFPOINT_TYPE"), QStringLiteral("REFPOINTTYPE")}).trimmed().toUpper();
                const QString angle = childValue(*line, {QStringLiteral("ANGLE")});
                const QString inside = childValue(*line, {QStringLiteral("INSIDE")});
                if (refType == QStringLiteral("CARTESIAN")) {
                    out << "LINE " << childValue(*line, {QStringLiteral("REFPOINT_R"), QStringLiteral("REFPOINTR")}) << ", "
                        << childValue(*line, {QStringLiteral("REFPOINT_X"), QStringLiteral("REFPOINTX")}) << ", "
                        << angle << ", " << (inside.isEmpty() ? QStringLiteral("LEFT") : inside) << '\n';
                } else {
                    out << "LINEP " << childValue(*line, {QStringLiteral("REFPOINT_MAG"), QStringLiteral("REFPOINTMAG")}) << ", "
                        << childValue(*line, {QStringLiteral("REFPOINT_ANGLE"), QStringLiteral("REFPOINTANGLE")}) << ", "
                        << angle << ", " << (inside.isEmpty() ? QStringLiteral("LEFT") : inside) << '\n';
                }
            }
            for (const auto* arc : arcs) {
                const QString refType = childValue(*arc, {QStringLiteral("REFPOINT_TYPE"), QStringLiteral("REFPOINTTYPE")}).trimmed().toUpper();
                const QString radius = childValue(*arc, {QStringLiteral("RADIUS")});
                const QString start = childValue(*arc, {QStringLiteral("STARTANGLE")});
                const QString end = childValue(*arc, {QStringLiteral("ENDANGLE")});
                const QString direction = childValue(*arc, {QStringLiteral("DIR")});
                const QString inside = childValue(*arc, {QStringLiteral("INSIDE")});
                if (refType == QStringLiteral("CARTESIAN")) {
                    out << "ARC " << childValue(*arc, {QStringLiteral("REFPOINT_R"), QStringLiteral("REFPOINTR")}) << ", "
                        << childValue(*arc, {QStringLiteral("REFPOINT_X"), QStringLiteral("REFPOINTX")}) << ", ";
                } else {
                    out << "ARCP " << childValue(*arc, {QStringLiteral("REFPOINT_MAG"), QStringLiteral("REFPOINTMAG")}) << ", "
                        << childValue(*arc, {QStringLiteral("REFPOINT_ANGLE"), QStringLiteral("REFPOINTANGLE")}) << ", ";
                }
                out << radius << ", " << start << ", " << end << ", "
                    << (direction.isEmpty() ? QStringLiteral("CCW") : direction) << ", "
                    << (inside.isEmpty() ? QStringLiteral("LEFT") : inside) << '\n';
            }
            writeParameter(out, QStringLiteral("AUTOCLOSE"), boolValue(childValue(*generic, {QStringLiteral("AUTOCLOSE")}), QStringLiteral("YES")));
            writeParameter(out, QStringLiteral("INVERT"), boolValue(childValue(*generic, {QStringLiteral("INVERT")})));
            out << "END SHAPE\n";
        } else if (const auto* lens = findDescendant(*zone, QStringLiteral("LENSTOMATOSHAPE"))) {
            out << "BEGIN LENSTOMATOSHAPE\n";
            writeParameter(out, QStringLiteral("ANGLE"), childValue(*lens, {QStringLiteral("ANGLE")}));
            writeParameter(out, QStringLiteral("REACH"), childValue(*lens, {QStringLiteral("REACH")}));
            writeParameter(out, QStringLiteral("OFFSET"), childValue(*lens, {QStringLiteral("OFFSET")}));
            writeParameter(out, QStringLiteral("INVERT"), boolValue(childValue(*lens, {QStringLiteral("INVERT")})));
            out << "END LENSTOMATOSHAPE\n";
        }
        out << "END ZONE\n";
    }

    out << "END DISTANCE\nEND TESTOBJECT\n";
    out.flush();
    auto model = ardirec::distance::parse_rio(rioText.toStdString());
    if (!model.valid) {
        diagnostic = QStringLiteral("XRIO standardized DISTANCE section was found but no supported zones could be constructed.");
        return std::nullopt;
    }
    diagnostic = QStringLiteral("XRIO standardized RIO Distance section imported. Vendor Custom formulas/scripts are not evaluated in P2.");
    return model;
}

QString diagnosticsText(const ardirec::distance::RioDistanceModel& model) {
    QStringList items;
    for (const auto& diagnostic : model.diagnostics) items.push_back(QString::fromStdString(diagnostic));
    return items.join(QStringLiteral("  •  "));
}
} // namespace

DistanceZoneController::DistanceZoneController(QObject* parent)
    : QObject(parent) {}

QString DistanceZoneController::zoneNativeRepresentation() const {
    return m_model.impedances_primary ? QStringLiteral("PRIMARY") : QStringLiteral("SECONDARY");
}

double DistanceZoneController::groundingFactorMagnitude() const {
    return std::abs(m_groundingFactor);
}

double DistanceZoneController::groundingFactorAngle() const {
    if (!m_groundingFactorValid) return 0.0;
    return std::atan2(m_groundingFactor.imag(), m_groundingFactor.real()) * 180.0 / kPi;
}

void DistanceZoneController::setGroundingFactorMagnitude(double value) {
    if (!std::isfinite(value) || value < 0.0) return;
    const double angle = groundingFactorAngle() * kPi / 180.0;
    m_groundingFactor = std::polar(value, angle);
    m_groundingFactorValid = true;
    m_groundingFactorSource = QStringLiteral("manual");
    emit groundingFactorChanged();
}

void DistanceZoneController::setGroundingFactorAngle(double value) {
    if (!std::isfinite(value)) return;
    const double magnitude = groundingFactorMagnitude();
    m_groundingFactor = std::polar(magnitude, value * kPi / 180.0);
    m_groundingFactorValid = true;
    m_groundingFactorSource = QStringLiteral("manual");
    emit groundingFactorChanged();
}

bool DistanceZoneController::openFile(const QUrl& fileUrl) {
    const QString localPath = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_status = QStringLiteral("Cannot open distance-zone file: %1").arg(file.errorString());
        m_sourceName = QFileInfo(localPath).fileName();
        emit modelChanged();
        return false;
    }

    const QByteArray data = file.readAll();
    m_sourceName = QFileInfo(localPath).fileName();
    const QByteArray trimmed = data.trimmed();
    const bool looksXml = trimmed.startsWith('<') || QFileInfo(localPath).suffix().compare(QStringLiteral("xrio"), Qt::CaseInsensitive) == 0;
    if (looksXml) return loadXrio(data);

    acceptModel(ardirec::distance::parse_rio(std::string_view(data.constData(), static_cast<std::size_t>(data.size()))),
                m_sourceName);
    return hasZones();
}

bool DistanceZoneController::loadXrio(const QByteArray& data) {
    QString adapterDiagnostic;
    const auto adapted = adaptStandardXrio(data, adapterDiagnostic);
    if (!adapted) {
        m_model = {};
        m_status = QStringLiteral("XRIO import failed");
        m_compatibilityWarning = adapterDiagnostic;
        emit modelChanged();
        return false;
    }
    acceptModel(*adapted, m_sourceName, adapterDiagnostic);
    return hasZones();
}

void DistanceZoneController::acceptModel(ardirec::distance::RioDistanceModel model,
                                         const QString& sourceName,
                                         const QString& adapterWarning) {
    m_model = std::move(model);
    m_sourceName = sourceName;
    QStringList warnings;
    if (!adapterWarning.isEmpty()) warnings.push_back(adapterWarning);
    const QString parsedWarnings = diagnosticsText(m_model);
    if (!parsedWarnings.isEmpty()) warnings.push_back(parsedWarnings);
    m_compatibilityWarning = warnings.join(QStringLiteral("  •  "));

    if (m_model.grounding_factor_valid) {
        m_groundingFactor = m_model.grounding_factor;
        m_groundingFactorValid = true;
        m_groundingFactorSource = QString::fromStdString(m_model.grounding_factor_source);
    }

    if (m_model.valid) {
        const QString device = m_model.device_name.empty() ? QStringLiteral("distance model") : QString::fromStdString(m_model.device_name);
        m_status = QStringLiteral("%1 · %2 zone%3 · native %4")
                       .arg(device)
                       .arg(m_model.zones.size())
                       .arg(m_model.zones.size() == 1 ? QString() : QStringLiteral("s"))
                       .arg(zoneNativeRepresentation());
    } else {
        m_status = QStringLiteral("No supported distance zones found");
    }
    emit modelChanged();
    emit groundingFactorChanged();
}

void DistanceZoneController::clearZones() {
    m_model = {};
    m_sourceName.clear();
    m_status = QStringLiteral("No distance zones loaded");
    m_compatibilityWarning.clear();
    m_groundingFactor = {};
    m_groundingFactorValid = false;
    m_groundingFactorSource = QStringLiteral("manual");
    emit modelChanged();
    emit groundingFactorChanged();
}

QVariantList DistanceZoneController::zonesForLoop(const QString& loopId, const QString& valueRepresentation) const {
    QVariantList result;
    if (!m_model.valid) return result;
    const auto loop = ardirec::distance::fault_loop_from_id(loopId.toStdString());
    const bool targetPrimary = valueRepresentation.compare(QStringLiteral("primary"), Qt::CaseInsensitive) == 0;
    const double scale = m_model.zone_scale(targetPrimary);

    for (const auto& zone : m_model.zones) {
        if (!ardirec::distance::zone_matches_loop(zone, loop)) continue;
        if (zone.shape.kind == ardirec::distance::ZoneShapeKind::Unsupported) continue;

        QVariantMap item;
        item.insert(QStringLiteral("index"), zone.index);
        item.insert(QStringLiteral("label"), QString::fromStdString(zone.label));
        item.insert(QStringLiteral("type"), QString::fromStdString(zone.type));
        item.insert(QStringLiteral("faultLoop"), QString::fromStdString(zone.fault_loop));
        item.insert(QStringLiteral("tripTime"), zone.trip_time_seconds);
        item.insert(QStringLiteral("scaleTrusted"), m_model.impedances_primary == targetPrimary || m_model.impedance_base_conversion_valid());

        if (zone.shape.kind == ardirec::distance::ZoneShapeKind::Circle) {
            item.insert(QStringLiteral("kind"), QStringLiteral("circle"));
            item.insert(QStringLiteral("centerR"), zone.shape.center_r * scale);
            item.insert(QStringLiteral("centerX"), zone.shape.center_x * scale);
            item.insert(QStringLiteral("radius"), zone.shape.radius * std::abs(scale));
        } else {
            item.insert(QStringLiteral("kind"), QStringLiteral("polygon"));
            QVariantList points;
            for (const auto& point : zone.shape.points) {
                points.push_back(QVariantMap{{QStringLiteral("r"), point.r * scale},
                                             {QStringLiteral("x"), point.x * scale}});
            }
            item.insert(QStringLiteral("points"), points);
        }
        result.push_back(item);
    }
    return result;
}
