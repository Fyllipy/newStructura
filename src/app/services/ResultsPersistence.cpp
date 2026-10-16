#include "ResultsPersistence.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QHash>

namespace Structura::App::Services {

namespace {

void put(QByteArray &bytes, double value)
{
    bytes += QByteArray::number(value, 'g', 12);
    bytes += ',';
}

void put(QByteArray &bytes, int value)
{
    bytes += QByteArray::number(value);
    bytes += ',';
}

} // namespace

QString ResultsPersistence::fingerprint(const std::vector<const SolverBridge::ModelData *> &models, bool secondOrder, double stiffnessReduction)
{
    QByteArray bytes;
    put(bytes, secondOrder ? 1 : 0);
    put(bytes, stiffnessReduction);
    for (const SolverBridge::ModelData *model : models) {
        bytes += "|M";
        for (const auto &m : model->materials) {
            put(bytes, m.id);
            put(bytes, m.e);
            put(bytes, m.g);
        }
        bytes += "|S";
        for (const auto &s : model->sections) {
            put(bytes, s.id);
            put(bytes, s.area);
            put(bytes, s.iz);
            put(bytes, s.iy);
            put(bytes, s.j);
        }
        bytes += "|N";
        for (const auto &n : model->nodes) {
            put(bytes, n.id);
            put(bytes, n.x);
            put(bytes, n.y);
            put(bytes, n.z);
            for (const int r : n.restraints) {
                put(bytes, r);
            }
        }
        bytes += "|B";
        for (const auto &m : model->members) {
            put(bytes, m.id);
            put(bytes, m.nodeI);
            put(bytes, m.nodeJ);
            put(bytes, m.materialId);
            put(bytes, m.sectionId);
            put(bytes, m.sectionRotationDeg);
            put(bytes, m.offsetY1);
            put(bytes, m.offsetZ1);
            put(bytes, m.offsetY2);
            put(bytes, m.offsetZ2);
            for (const int r : m.releases) {
                put(bytes, r);
            }
            put(bytes, m.tensionOnly ? 1 : 0);
        }
        bytes += "|L";
        for (const auto &l : model->nodalLoads) {
            put(bytes, l.nodeId);
            for (const double v : l.values) {
                put(bytes, v);
            }
        }
        for (const auto &l : model->memberLoads) {
            put(bytes, l.memberId);
            put(bytes, static_cast<int>(l.coordSystem));
            put(bytes, l.qx);
            put(bytes, l.qy);
            put(bytes, l.qz);
        }
        for (const auto &l : model->memberMoments) {
            put(bytes, l.memberId);
            put(bytes, static_cast<int>(l.coordSystem));
            put(bytes, l.mx);
            put(bytes, l.my);
            put(bytes, l.mz);
        }
    }
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
}

ProjectData::StoredRun ResultsPersistence::toStored(const QString &name, const SolverBridge::Result &result)
{
    ProjectData::StoredRun run;
    run.name = name;
    for (const auto &n : result.nodeResults) {
        ProjectData::StoredNodeResult s;
        s.nodeId = n.nodeId;
        s.displacements = n.displacements;
        s.reactions = n.reactions;
        run.nodes.append(s);
    }
    for (const auto &m : result.memberResults) {
        ProjectData::StoredMemberResult s;
        s.memberId = m.memberId;
        s.forces = m.localForces;
        s.auxK = m.auxK;
        run.members.append(s);
    }
    return run;
}

SolverBridge::Result ResultsPersistence::fromStored(const ProjectData::StoredRun &run, const SolverBridge::ModelData &model, bool *ok)
{
    SolverBridge::Result result;
    QHash<int, const ProjectData::StoredNodeResult *> nodes;
    for (const auto &n : run.nodes) {
        nodes.insert(n.nodeId, &n);
    }
    QHash<int, const ProjectData::StoredMemberResult *> members;
    for (const auto &m : run.members) {
        members.insert(m.memberId, &m);
    }
    bool good = true;
    for (const auto &node : model.nodes) {
        SolverBridge::NodeResult r;
        r.nodeId = node.id;
        const auto it = nodes.constFind(node.id);
        if (it == nodes.constEnd()) {
            good = false;
        } else {
            r.displacements = (*it)->displacements;
            r.reactions = (*it)->reactions;
        }
        result.nodeResults.push_back(r);
    }
    for (const auto &member : model.members) {
        SolverBridge::MemberResult r;
        r.memberId = member.id;
        const auto it = members.constFind(member.id);
        if (it == members.constEnd()) {
            good = false;
        } else {
            r.localForces = (*it)->forces;
            r.auxK = (*it)->auxK;
        }
        result.memberResults.push_back(r);
    }
    if (ok) {
        *ok = good;
    }
    return result;
}

} // namespace Structura::App::Services
