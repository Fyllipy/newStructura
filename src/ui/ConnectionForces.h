#pragma once

#include "app/services/ConnectionCatalog.h"
#include "ConnectionDialog.h"
#include "DesignRunData.h"
#include "app/services/ConnectionService.h"
#include "app/services/NodeEquilibrium.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace Structura::UI {

/// Envoltoria dos esforcos de extremidade de uma barra (kN, kN.m) a partir de uma analise de dimensionamento.
inline EndForceEnvelope envelopeAt(const DesignRunData &run, int memberId, bool atEnd)
{
    EndForceEnvelope env;
    for (const auto &effects : run.effects) {
        if (effects.memberId != memberId) {
            continue;
        }
        for (const auto &combo : effects.combos) {
            if (combo.stations.empty()) {
                continue;
            }
            const auto &e = (atEnd ? combo.stations.back() : combo.stations.front()).e;
            const double kN = 1.0 / 1000.0, kNm = 1.0 / 1e6;
            env.n = std::max(env.n, e.N * kN);
            env.nc = std::max(env.nc, -e.N * kN);
            env.vy = std::max(env.vy, std::abs(e.Vy) * kN);
            env.vz = std::max(env.vz, std::abs(e.Vz) * kN);
            env.mx = std::max(env.mx, std::abs(e.Mx) * kNm);
            env.my = std::max(env.my, std::abs(e.My) * kNm);
            env.t = std::max(env.t, std::abs(e.T) * kNm);
            env.ok = true;
        }
    }
    return env;
}

/// Esforcos de calculo de uma ligacao: cortante resultante, axial dominante, maiores tracao/compressao e momento (N, N.mm),
/// alem dos esforcos por combinacao na extremidade da barra.
inline std::optional<Structura::App::Services::ConnectionService::Forces> connectionForces(const DesignRunData &run, int memberId, bool atEnd)
{
    const auto env = envelopeAt(run, memberId, atEnd);
    if (!env.ok) {
        return std::nullopt;
    }
    using Structura::App::Services::ConnectionService;
    ConnectionService::Forces forces;
    forces.shear = std::hypot(env.vy, env.vz) * 1000.0;
    forces.axial = (env.n >= env.nc ? env.n : -env.nc) * 1000.0;
    forces.tension = std::max(0.0, env.n) * 1000.0;
    forces.compression = std::max(0.0, env.nc) * 1000.0;
    forces.moment = std::max(env.mx, env.my) * 1e6;
    for (const auto &effects : run.effects) {
        if (effects.memberId != memberId) {
            continue;
        }
        for (const auto &combo : effects.combos) {
            if (combo.stations.empty()) {
                continue;
            }
            const auto &e = (atEnd ? combo.stations.back() : combo.stations.front()).e;
            ConnectionService::ForceCase fc;
            fc.name = QString::fromStdString(combo.name);
            fc.shear = std::hypot(e.Vy, e.Vz);
            fc.axial = e.N;
            fc.moment = std::max(std::abs(e.Mx), std::abs(e.My));
            forces.cases.append(fc);
        }
    }
    return forces;
}

/// Cortante do painel de alma do pilar e esforco normal do pilar por combinacao (equilibrio do no): usa as barras do no gravadas na ligacao
/// (`nmCount`, `nm<i>`, `ne<i>`) e o pilar de apoio (`supMember`). So para as ligacoes rigidas viga-pilar.
inline void attachPanelData(Structura::App::Services::ConnectionService::Forces &forces, const DesignRunData &run,
                            const Structura::App::ProjectData::NodeConnection &connection)
{
    using Structura::App::ProjectData::NodeConnection;
    namespace Ne = Structura::App::Services::NodeEquilibrium;
    const auto *entry = Structura::App::Services::ConnectionCatalog::find(connection.type);
    const bool rigid = Structura::App::Services::ConnectionCatalog::baseType(connection.type) == NodeConnection::kEndPlate
        || (entry && entry->family == Structura::App::Services::ConnectionCatalog::Family::Moment);
    const int count = static_cast<int>(connection.values.value(QStringLiteral("nmCount"), 0.0) + 0.5);
    const int host = static_cast<int>(connection.values.value(QStringLiteral("supMember"), 0.0) + 0.5);
    if (!rigid || count < 2 || host <= 0) {
        return;
    }
    const auto &v = connection.values;
    const double depth = v.value(QStringLiteral("beamD"), 0.0);
    const double flange = v.contains(QStringLiteral("tfl")) ? v.value(QStringLiteral("tfl"), 0.0) : v.value(QStringLiteral("beamTf"), 0.0);
    if (depth <= 0.0 || flange <= 0.0 || depth <= flange) {
        return;
    }
    const double lever = depth - flange;
    for (auto &fc : forces.cases) {
        std::vector<Ne::MemberEnd> members;
        int beamIndex = -1, hostIndex = -1;
        bool complete = true;
        for (int i = 0; i < count && complete; ++i) {
            const int id = static_cast<int>(v.value(QStringLiteral("nm%1").arg(i), 0.0) + 0.5);
            const bool atEnd = v.value(QStringLiteral("ne%1").arg(i), 0.0) != 0.0;
            const Structura::App::Services::DesignService::EndRecord *record = nullptr;
            for (const auto &effects : run.effects) {
                if (effects.memberId != id) {
                    continue;
                }
                for (const auto &end : effects.ends) {
                    if (end.combo == fc.name.toStdString()) {
                        record = &end;
                        break;
                    }
                }
            }
            if (!record) {
                complete = false;
                break;
            }
            Ne::MemberEnd m;
            m.atEnd = atEnd;
            m.x = record->x;
            m.y = record->y;
            m.z = record->z;
            m.f = record->f;
            if (id == connection.memberId && atEnd == connection.atEnd) {
                beamIndex = static_cast<int>(members.size());
            }
            if (id == host && hostIndex < 0) {
                hostIndex = static_cast<int>(members.size());
            }
            members.push_back(m);
        }
        if (!complete) {
            continue;
        }
        const auto panel = Ne::panelShear(members, beamIndex, hostIndex, lever);
        if (panel.valid) {
            fc.panelShear = panel.panelShear;
            fc.columnAxial = panel.columnAxial;
        }
    }
}

/// Esforcos de uma ligacao do modelo (com as diagonais, no caso da ligacao tubular).
inline std::optional<Structura::App::Services::ConnectionService::Forces> connectionForces(const DesignRunData &run, const Structura::App::ProjectData::NodeConnection &connection)
{
    auto forces = connectionForces(run, connection.memberId, connection.atEnd);
    if (forces) {
        attachPanelData(*forces, run, connection);
    }
    if (!forces || connection.values.value(QStringLiteral("br1"), 0.0) <= 0.0) {
        return forces;
    }
    auto axialOf = [&run](int memberId, bool atEnd, const QString &combo) -> double {
        for (const auto &effects : run.effects) {
            if (effects.memberId != memberId) {
                continue;
            }
            for (const auto &c : effects.combos) {
                if (QString::fromStdString(c.name) == combo && !c.stations.empty()) {
                    return (atEnd ? c.stations.back() : c.stations.front()).e.N;
                }
            }
        }
        return 0.0;
    };
    const int br1 = static_cast<int>(connection.values.value(QStringLiteral("br1"), 0.0) + 0.5);
    const int br2 = static_cast<int>(connection.values.value(QStringLiteral("br2"), 0.0) + 0.5);
    const int br3 = static_cast<int>(connection.values.value(QStringLiteral("br3"), 0.0) + 0.5);
    const bool end1 = connection.values.value(QStringLiteral("br1End"), 0.0) != 0.0;
    const bool end2 = connection.values.value(QStringLiteral("br2End"), 0.0) != 0.0;
    const bool end3 = connection.values.value(QStringLiteral("br3End"), 0.0) != 0.0;
    for (auto &fc : forces->cases) {
        const std::array<std::pair<int, bool>, 3> braces {{{br1, end1}, {br2, end2}, {br3, end3}}};
        for (const auto &[id, atEnd] : braces) {
            if (id > 0) {
                const double n = axialOf(id, atEnd, fc.name);
                fc.braceAxial.append(std::abs(n));
                fc.braceSigned.append(n);
            }
        }
    }
    return forces;
}

/// Idem, acrescentando a solicitacao das demais ligacoes que chegam a mesma barra de apoio (`supMember`) no mesmo no:
/// a verificacao local do apoio deve considerar o esforco de todas elas.
inline std::optional<Structura::App::Services::ConnectionService::Forces> connectionForces(const DesignRunData &run,
                                                                                         const Structura::App::ProjectData::NodeConnection &connection,
                                                                                         const QVector<Structura::App::ProjectData::NodeConnection> &all)
{
    using Structura::App::ProjectData::NodeConnection;
    using Structura::App::Services::ConnectionService;
    auto forces = connectionForces(run, connection);
    const int host = static_cast<int>(connection.values.value(QStringLiteral("supMember"), 0.0) + 0.5);
    const int baseType = Structura::App::Services::ConnectionCatalog::baseType(connection.type);
    const auto *catalogEntry = Structura::App::Services::ConnectionCatalog::find(connection.type);
    const bool usesSupport = baseType == NodeConnection::kShearTab || baseType == NodeConnection::kDoubleAngle || baseType == NodeConnection::kGusset || (catalogEntry && catalogEntry->usesSupport && catalogEntry->legacyType < 0);
    if (!forces || host <= 0 || !usesSupport) {
        return forces;
    }
    for (const auto &other : all) {
        const bool same = other.memberId == connection.memberId && other.atEnd == connection.atEnd && other.nodeId == connection.nodeId && other.type == connection.type;
        const int otherBase = Structura::App::Services::ConnectionCatalog::baseType(other.type);
        const auto *otherEntry = Structura::App::Services::ConnectionCatalog::find(other.type);
        const bool otherUses = otherBase == NodeConnection::kShearTab || otherBase == NodeConnection::kDoubleAngle || otherBase == NodeConnection::kGusset || (otherEntry && otherEntry->usesSupport && otherEntry->legacyType < 0);
        if (same || !otherUses || other.nodeId != connection.nodeId
            || static_cast<int>(other.values.value(QStringLiteral("supMember"), 0.0) + 0.5) != host) {
            continue;
        }
        const auto otherForces = connectionForces(run, other);
        if (!otherForces) {
            continue;
        }
        const auto demand = ConnectionService::supportDemand(other, *otherForces);
        forces->hostTension += demand.first;
        forces->hostCompression += demand.second;
        ++forces->hostConnections;
    }
    return forces;
}

} // namespace Structura::UI
