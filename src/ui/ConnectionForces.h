#pragma once

#include "ConnectionDialog.h"
#include "DesignRunData.h"
#include "app/services/ConnectionService.h"

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

/// Esforcos de uma ligacao do modelo (com as diagonais, no caso da ligacao tubular).
inline std::optional<Structura::App::Services::ConnectionService::Forces> connectionForces(const DesignRunData &run, const Structura::App::ProjectData::NodeConnection &connection)
{
    auto forces = connectionForces(run, connection.memberId, connection.atEnd);
    if (!forces || connection.type != Structura::App::ProjectData::NodeConnection::kTubular) {
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
    const bool end1 = connection.values.value(QStringLiteral("br1End"), 0.0) != 0.0;
    const bool end2 = connection.values.value(QStringLiteral("br2End"), 0.0) != 0.0;
    for (auto &fc : forces->cases) {
        if (br1 > 0) {
            fc.braceAxial.append(std::abs(axialOf(br1, end1, fc.name)));
        }
        if (br2 > 0) {
            fc.braceAxial.append(std::abs(axialOf(br2, end2, fc.name)));
        }
    }
    return forces;
}

} // namespace Structura::UI
