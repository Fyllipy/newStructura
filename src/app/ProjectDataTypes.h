#pragma once

#include <QString>
#include <QUuid>

namespace Structura::App::ProjectData {

struct MaterialInfo {
    QUuid uuid;
    int externalId {0};
    QString name;
    double youngModulus {0.0};
    double shearModulus {0.0};
};

struct SectionInfo {
    QUuid uuid;
    int externalId {0};
    QString name;
    double area {0.0};
    double iz {0.0};
    double iy {0.0};
    double j {0.0};
};

struct NodeSupport {
    int nodeId {0};
    bool restraints[6] {false, false, false, false, false, false};
};

struct NodalLoad {
    int nodeId {0};
    double fx {0.0};
    double fy {0.0};
    double fz {0.0};
    double mx {0.0};
    double my {0.0};
    double mz {0.0};
};

struct MemberLoad {
    int memberId {0};
    QString system;
    double qx {0.0};
    double qy {0.0};
    double qz {0.0};
};

struct NodalLoadPreset {
    double fx {0.0};
    double fy {0.0};
    double fz {0.0};
    double mx {0.0};
    double my {0.0};
    double mz {0.0};
};

struct DistributedLoadPreset {
    QString system {QStringLiteral("GLOBAL")};
    double qx {0.0};
    double qy {0.0};
    double qz {0.0};
};

} // namespace Structura::App::ProjectData
