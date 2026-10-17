#include "DynamicsService.h"

#include "ModalService.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

namespace Structura::App::Services {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::unordered_map<int, std::size_t> indexById(const SolverBridge::ModelData &model)
{
    std::unordered_map<int, std::size_t> index;
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        index[model.nodes[i].id] = i;
    }
    return index;
}

double shapeAt(const std::vector<double> &shape, std::size_t node, int dof)
{
    const std::size_t i = node * 6 + static_cast<std::size_t>(dof);
    return i < shape.size() ? shape[i] : 0.0;
}

/// Soma sum_k sum_l rho_kl q_k q_l (CQC) ou sum_k q_k^2 (SRSS); resultado = raiz.
double combineModal(const std::vector<double> &q, const std::vector<std::vector<double>> &rho, bool cqc)
{
    double sum = 0.0;
    for (std::size_t k = 0; k < q.size(); ++k) {
        sum += q[k] * q[k];
        if (cqc) {
            for (std::size_t l = 0; l < q.size(); ++l) {
                if (l != k) {
                    sum += rho[k][l] * q[k] * q[l];
                }
            }
        }
    }
    return std::sqrt(std::max(0.0, sum));
}

} // namespace

std::vector<double> DynamicsService::nodalMasses(const SolverBridge::ModelData &model, bool includeMaterialMass)
{
    const auto index = indexById(model);
    std::vector<double> mass(model.nodes.size(), 0.0);
    for (const auto &[id, value] : ModalService::massesFromLoads(model)) {
        const auto it = index.find(id);
        if (it != index.end()) {
            mass[it->second] += value;
        }
    }
    if (includeMaterialMass) {
        std::unordered_map<int, double> density;
        for (const auto &m : model.materials) {
            density[m.id] = m.density;
        }
        std::unordered_map<int, double> area;
        for (const auto &s : model.sections) {
            area[s.id] = s.area;
        }
        for (const auto &member : model.members) {
            const auto a = index.find(member.nodeI), b = index.find(member.nodeJ);
            if (a == index.end() || b == index.end()) {
                continue;
            }
            const auto &na = model.nodes[a->second], &nb = model.nodes[b->second];
            const double length = std::sqrt((na.x - nb.x) * (na.x - nb.x) + (na.y - nb.y) * (na.y - nb.y) + (na.z - nb.z) * (na.z - nb.z));
            const double half = 0.5 * density[member.materialId] * area[member.sectionId] * length;
            mass[a->second] += half;
            mass[b->second] += half;
        }
    }
    return mass;
}

double DynamicsService::totalMass(const Basis &basis)
{
    double total = 0.0;
    for (const double m : basis.mass) {
        total += m;
    }
    return total;
}

std::vector<DynamicsService::ModeInfo> DynamicsService::modes(const Basis &basis)
{
    std::vector<ModeInfo> out;
    if (!basis.modal) {
        return out;
    }
    const double total = std::max(1e-12, totalMass(basis));
    const std::size_t count = std::min(basis.modal->modalOmega.size(), basis.modal->modalShapes.size());
    for (std::size_t k = 0; k < count; ++k) {
        const auto &shape = basis.modal->modalShapes[k];
        ModeInfo info;
        info.index = static_cast<int>(k);
        info.omega = basis.modal->modalOmega[k];
        info.frequency = info.omega / (2.0 * kPi);
        info.period = info.omega > 0.0 ? 2.0 * kPi / info.omega : 0.0;
        double general = 0.0;
        std::array<double, 3> lead {{0, 0, 0}};
        for (std::size_t j = 0; j < basis.mass.size(); ++j) {
            for (int d = 0; d < 3; ++d) {
                const double phi = shapeAt(shape, j, d);
                general += basis.mass[j] * phi * phi;
                lead[static_cast<std::size_t>(d)] += basis.mass[j] * phi;
            }
        }
        info.generalizedMass = general;
        for (std::size_t d = 0; d < 3; ++d) {
            info.gamma[d] = general > 1e-300 ? lead[d] / general : 0.0;
            info.effectiveMass[d] = general > 1e-300 ? lead[d] * lead[d] / general : 0.0;
            info.effectiveRatio[d] = info.effectiveMass[d] / total;
        }
        out.push_back(info);
    }
    return out;
}

// ── Espectro ─────────────────────────────────────────────────────────────

double DynamicsService::Spectrum::sa(double period) const
{
    if (points.empty()) {
        return 0.0;
    }
    if (period <= points.front().first) {
        return points.front().second;
    }
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (period <= points[i].first) {
            const double t0 = points[i - 1].first, t1 = points[i].first;
            const double u = t1 > t0 ? (period - t0) / (t1 - t0) : 0.0;
            return points[i - 1].second + u * (points[i].second - points[i - 1].second);
        }
    }
    return points.back().second;
}

DynamicsService::Spectrum DynamicsService::Spectrum::parametric(double plateau, double sa1, double tl, double t0Factor)
{
    Spectrum s;
    if (plateau <= 0.0 || sa1 <= 0.0) {
        return s;
    }
    const double ts = sa1 / plateau;
    const double t0 = t0Factor * ts;
    s.points.push_back({0.0, 0.4 * plateau});
    s.points.push_back({t0, plateau});
    s.points.push_back({ts, plateau});
    tl = std::max(tl, ts);
    for (double t = ts + 0.05; t <= 10.0 + 1e-9; t += 0.05) {
        s.points.push_back({t, t <= tl ? sa1 / t : sa1 * tl / (t * t)});
    }
    return s;
}

bool DynamicsService::Spectrum::parse(const QString &text, Spectrum &out)
{
    out.points.clear();
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        QString line = raw.trimmed();
        line.replace(QLatin1Char(','), QLatin1Char('.'));
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("[\\s;]+")), Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            continue;
        }
        bool okT = false, okS = false;
        const double t = parts[0].toDouble(&okT);
        const double sa = parts[1].toDouble(&okS);
        if (okT && okS && t >= 0.0 && sa >= 0.0) {
            out.points.push_back({t, sa});
        }
    }
    std::sort(out.points.begin(), out.points.end());
    return out.points.size() >= 2;
}

double DynamicsService::cqcCorrelation(double omegaA, double omegaB, double damping)
{
    if (omegaA <= 0.0 || omegaB <= 0.0) {
        return 0.0;
    }
    const double r = omegaB / omegaA;
    const double z = damping;
    const double num = 8.0 * z * z * (1.0 + r) * std::pow(r, 1.5);
    const double den = (1.0 - r * r) * (1.0 - r * r) + 4.0 * z * z * r * (1.0 + r) * (1.0 + r);
    return den > 0.0 ? num / den : 1.0;
}

DynamicsService::SpectrumResult DynamicsService::responseSpectrum(const Basis &basis, const SpectrumOptions &options)
{
    SpectrumResult result;
    if (!basis.model || !basis.modal) {
        result.warning = QStringLiteral("Calcule os modos de vibração antes.");
        return result;
    }
    const auto info = modes(basis);
    std::size_t usable = info.size();
    if (options.modeCount > 0) {
        usable = std::min<std::size_t>(usable, static_cast<std::size_t>(options.modeCount));
    }
    if (usable == 0 || options.spectrum.points.empty()) {
        result.warning = QStringLiteral("Sem modos ou sem espectro definido.");
        return result;
    }
    const auto &model = *basis.model;
    const auto index = indexById(model);
    const std::size_t nodeCount = model.nodes.size();

    std::vector<std::vector<double>> rho(usable, std::vector<double>(usable, 0.0));
    for (std::size_t k = 0; k < usable; ++k) {
        for (std::size_t l = 0; l < usable; ++l) {
            rho[k][l] = k == l ? 1.0 : cqcCorrelation(info[k].omega, info[l].omega, options.damping);
        }
    }
    const double scale = options.importance / std::max(1e-9, options.reduction);

    // por direcao: deslocamentos, reacoes e esforcos combinados
    struct PerDirection {
        std::vector<std::array<double, 6>> disp, react;
        std::vector<std::array<double, 12>> member;
    };
    std::vector<PerDirection> perDirection;
    std::map<int, std::size_t> memberRow;
    std::vector<int> memberIds;
    for (const auto &m : model.members) {
        memberRow[m.id] = memberIds.size();
        memberIds.push_back(m.id);
    }

    SolverBridge solver;
    int totalSolves = 0, solved = 0;
    for (std::size_t d = 0; d < 3; ++d) {
        totalSolves += options.directions[d] ? static_cast<int>(usable) : 0;
    }
    for (std::size_t d = 0; d < 3; ++d) {
        if (!options.directions[d]) {
            continue;
        }
        // por modo
        std::vector<std::vector<std::array<double, 6>>> modeDisp(usable), modeReact(usable);
        std::vector<std::vector<std::array<double, 12>>> modeMember(usable);
        std::vector<double> baseShearModal(usable, 0.0);
        for (std::size_t k = 0; k < usable; ++k) {
            const double sa = options.spectrum.sa(info[k].period) * kGravity * scale;
            auto copy = model;
            copy.nodalLoads.clear();
            copy.memberLoads.clear();
            copy.memberMoments.clear();
            copy.extraMasses.clear();
            copy.modalModes = 0;
            copy.bucklingModes = 0;
            copy.secondOrder = false;
            const auto &shape = basis.modal->modalShapes[k];
            for (std::size_t j = 0; j < nodeCount; ++j) {
                SolverBridge::NodalLoadData load;
                load.nodeId = model.nodes[j].id;
                bool any = false;
                for (int i = 0; i < 3; ++i) {
                    const double force = basis.mass[j] * info[k].gamma[d] * shapeAt(shape, j, i) * sa;
                    load.values[static_cast<std::size_t>(i)] = force;
                    any = any || std::abs(force) > 0.0;
                }
                if (any) {
                    copy.nodalLoads.push_back(load);
                }
            }
            baseShearModal[k] = info[k].effectiveMass[d] * sa;
            ModalLine line;
            line.direction = static_cast<int>(d);
            line.mode = static_cast<int>(k);
            line.period = info[k].period;
            line.sa = sa;
            line.gamma = info[k].gamma[d];
            line.baseShear = baseShearModal[k];
            result.lines.push_back(line);

            SolverBridge::Result solvedResult;
            try {
                solvedResult = solver.solve(copy);
                if (options.progress) {
                    options.progress(++solved, totalSolves);
                }
            } catch (const std::exception &error) {
                result.warning = QStringLiteral("Falha ao resolver o modo %1: %2").arg(k + 1).arg(QString::fromUtf8(error.what()));
                return result;
            }
            modeDisp[k].assign(nodeCount, {});
            modeReact[k].assign(nodeCount, {});
            for (const auto &nr : solvedResult.nodeResults) {
                const auto it = index.find(nr.nodeId);
                if (it != index.end()) {
                    modeDisp[k][it->second] = nr.displacements;
                    modeReact[k][it->second] = nr.reactions;
                }
            }
            modeMember[k].assign(memberIds.size(), {});
            for (const auto &mr : solvedResult.memberResults) {
                const auto it = memberRow.find(mr.memberId);
                if (it != memberRow.end()) {
                    modeMember[k][it->second] = mr.localForces;
                }
            }
        }
        PerDirection pd;
        pd.disp.assign(nodeCount, {});
        pd.react.assign(nodeCount, {});
        pd.member.assign(memberIds.size(), {});
        std::vector<double> q(usable);
        for (std::size_t j = 0; j < nodeCount; ++j) {
            for (std::size_t c = 0; c < 6; ++c) {
                for (std::size_t k = 0; k < usable; ++k) {
                    q[k] = modeDisp[k][j][c];
                }
                pd.disp[j][c] = combineModal(q, rho, options.cqc);
                for (std::size_t k = 0; k < usable; ++k) {
                    q[k] = modeReact[k][j][c];
                }
                pd.react[j][c] = combineModal(q, rho, options.cqc);
            }
        }
        for (std::size_t m = 0; m < memberIds.size(); ++m) {
            for (std::size_t c = 0; c < 12; ++c) {
                for (std::size_t k = 0; k < usable; ++k) {
                    q[k] = modeMember[k][m][c];
                }
                pd.member[m][c] = combineModal(q, rho, options.cqc);
            }
        }
        result.baseShear[d] = combineModal(baseShearModal, rho, options.cqc);
        double covered = 0.0;
        for (std::size_t k = 0; k < usable; ++k) {
            covered += info[k].effectiveRatio[d];
        }
        result.massCovered[d] = covered;
        perDirection.push_back(std::move(pd));
    }
    if (perDirection.empty()) {
        result.warning = QStringLiteral("Nenhuma direção selecionada.");
        return result;
    }

    // combinacao entre direcoes
    auto mix = [&](const std::vector<double> &values) {
        if (options.thirtyPercent) {
            double best = 0.0;
            for (std::size_t a = 0; a < values.size(); ++a) {
                double sum = std::abs(values[a]);
                for (std::size_t b = 0; b < values.size(); ++b) {
                    if (b != a) {
                        sum += 0.3 * std::abs(values[b]);
                    }
                }
                best = std::max(best, sum);
            }
            return best;
        }
        double sum = 0.0;
        for (const double v : values) {
            sum += v * v;
        }
        return std::sqrt(sum);
    };
    std::vector<double> values(perDirection.size());
    result.displacements.assign(nodeCount, {});
    result.reactions.assign(nodeCount, {});
    for (std::size_t j = 0; j < nodeCount; ++j) {
        for (std::size_t c = 0; c < 6; ++c) {
            for (std::size_t p = 0; p < perDirection.size(); ++p) {
                values[p] = perDirection[p].disp[j][c];
            }
            result.displacements[j][c] = mix(values);
            for (std::size_t p = 0; p < perDirection.size(); ++p) {
                values[p] = perDirection[p].react[j][c];
            }
            result.reactions[j][c] = mix(values);
        }
    }
    for (std::size_t m = 0; m < memberIds.size(); ++m) {
        SolverBridge::MemberResult row;
        row.memberId = memberIds[m];
        for (std::size_t c = 0; c < 12; ++c) {
            for (std::size_t p = 0; p < perDirection.size(); ++p) {
                values[p] = perDirection[p].member[m][c];
            }
            row.localForces[c] = mix(values);
        }
        result.members.push_back(row);
    }
    if (result.massCovered[0] < 0.9 && options.directions[0] && usable < info.size()) {
        result.warning = QStringLiteral("A massa efetiva dos modos usados é inferior a 90% (use mais modos).");
    }
    for (std::size_t d = 0; d < 3; ++d) {
        if (options.directions[d] && result.massCovered[d] < 0.9) {
            result.warning = QStringLiteral("Massa efetiva acumulada abaixo de 90% em alguma direção: aumente o número de modos.");
        }
    }
    return result;
}

// ── Vibracao de pisos ────────────────────────────────────────────────────

std::vector<DynamicsService::FloorOccupancy> DynamicsService::floorOccupancies()
{
    return {
        {QStringLiteral("Escritórios"), 0.050, 8.0, 0.010},
        {QStringLiteral("Escolas"), 0.050, 8.0, 0.010},
        {QStringLiteral("Circulação de público (shopping, aeroportos, rodoviárias)"), 0.150, 24.0, 0.000},
        {QStringLiteral("Centros comerciais e lojas em geral"), 0.050, 8.0, 0.010},
        {QStringLiteral("Garagens, passarelas e escadas internas"), 0.150, 24.0, 0.005},
        {QStringLiteral("Passarelas e escadas externas"), 0.200, 32.0, 0.005},
        {QStringLiteral("Residências e hotéis"), 0.050, 8.0, 0.010},
        {QStringLiteral("Hospitais: atendimento, internação (limite mais restritivo)"), 0.025, 4.0, 0.010},
        {QStringLiteral("Hospitais: cirurgia e laboratórios (limite mais restritivo)"), 0.006, 1.0, 0.010},
    };
}

int DynamicsService::firstVerticalMode(const Basis &basis)
{
    const std::size_t count = basis.modal ? std::min(basis.modal->modalOmega.size(), basis.modal->modalShapes.size()) : 0;
    int best = 0;
    double bestShare = -1.0;
    for (std::size_t k = 0; k < count; ++k) {
        double vertical = 0.0, all = 0.0;
        for (std::size_t j = 0; j < basis.mass.size(); ++j) {
            for (int d = 0; d < 3; ++d) {
                const double phi = shapeAt(basis.modal->modalShapes[k], j, d);
                all += basis.mass[j] * phi * phi;
                if (d == 2) {
                    vertical += basis.mass[j] * phi * phi;
                }
            }
        }
        const double share = all > 1e-300 ? vertical / all : 0.0;
        if (share >= 0.5) {
            return static_cast<int>(k);
        }
        if (share > bestShare) {
            bestShare = share;
            best = static_cast<int>(k);
        }
    }
    return best;
}

DynamicsService::FloorResult DynamicsService::floorVibration(const Basis &basis, const FloorInput &input)
{
    FloorResult r;
    const auto info = modes(basis);
    if (info.empty() || input.mode < 0 || input.mode >= static_cast<int>(info.size())) {
        r.message = QStringLiteral("Calcule os modos de vibração e escolha um modo.");
        return r;
    }
    const auto &mode = info[static_cast<std::size_t>(input.mode)];
    const auto &shape = basis.modal->modalShapes[static_cast<std::size_t>(input.mode)];
    double maxZ = 0.0;
    std::size_t control = 0;
    double vertical = 0.0, all = 0.0;
    for (std::size_t j = 0; j < basis.mass.size(); ++j) {
        const double z = shapeAt(shape, j, 2);
        if (std::abs(z) > std::abs(maxZ)) {
            maxZ = z;
            control = j;
        }
        vertical += basis.mass[j] * z * z;
        for (int d = 0; d < 3; ++d) {
            const double phi = shapeAt(shape, j, d);
            all += basis.mass[j] * phi * phi;
        }
    }
    if (std::abs(maxZ) < 1e-12 || all < 1e-300) {
        r.message = QStringLiteral("O modo escolhido não tem deslocamento vertical.");
        return r;
    }
    r.verticalShare = vertical / all;
    r.frequency = mode.frequency;
    r.modalMass = vertical / (maxZ * maxZ) * 1000.0;   // t -> kg, forma normalizada com 1 no ponto de maior deslocamento
    r.controlNode = basis.model->nodes[control].id;
    r.zeta = r.frequency <= 5.0 ? 0.15 : 0.10;
    r.weighting = r.frequency < 4.0 ? 0.5 * std::sqrt(r.frequency) : (r.frequency <= 8.0 ? 1.0 : 8.0 / r.frequency);
    const double beta = std::max(1e-6, input.damping);
    r.peak = 0.55 * input.personWeight * std::exp(-0.35 * r.frequency) / (2.0 * r.modalMass * beta);
    r.rms = r.zeta * input.personWeight / (2.0 * std::sqrt(2.0) * r.modalMass * beta) * r.weighting;
    r.responseFactor = r.rms / 0.005;
    const auto occupancies = floorOccupancies();
    const auto &occ = occupancies[static_cast<std::size_t>(std::clamp(input.occupancy, 0, static_cast<int>(occupancies.size()) - 1))];
    r.limitPeak = occ.peakLimit;
    r.limitFactor = occ.responseFactor;
    r.frequencyOk = r.frequency >= input.fMin;
    r.accelerationOk = input.useRms ? r.responseFactor <= r.limitFactor : r.peak <= r.limitPeak;
    r.ok = true;
    if (r.verticalShare < 0.5) {
        r.message = QStringLiteral("Atenção: o modo escolhido não é predominantemente vertical (%1% da energia).").arg(r.verticalShare * 100.0, 0, 'f', 0);
    } else if (r.frequency < 3.0 || r.frequency > 9.0) {
        r.message = QStringLiteral("Frequência fora de 3 a 9 Hz: o procedimento simplificado do Anexo I (I.2.1) não se aplica integralmente; "
                                   "a norma pede avaliação precisa (I.3) quando f < 3 Hz.");
    }
    return r;
}

// ── Resposta no tempo ────────────────────────────────────────────────────

DynamicsService::TimeResult DynamicsService::timeHistory(const Basis &basis, const TimeInput &input)
{
    TimeResult r;
    const auto info = modes(basis);
    if (info.empty()) {
        r.message = QStringLiteral("Calcule os modos de vibração antes.");
        return r;
    }
    const auto index = indexById(*basis.model);
    const auto load = index.find(input.loadNode), resp = index.find(input.responseNode);
    if (load == index.end() || resp == index.end()) {
        r.message = QStringLiteral("Nó de carga ou de resposta inexistente.");
        return r;
    }
    if (input.duration <= 0.0) {
        r.message = QStringLiteral("Duração inválida.");
        return r;
    }
    std::size_t usable = info.size();
    if (input.modeCount > 0) {
        usable = std::min<std::size_t>(usable, static_cast<std::size_t>(input.modeCount));
    }
    // formas normalizadas pela massa (modal massa unitaria) nos graus de liberdade de carga e resposta
    std::vector<double> loadShape(usable), respShape(usable), omega(usable);
    double highest = 0.0;
    for (std::size_t k = 0; k < usable; ++k) {
        const double scale = info[k].generalizedMass > 1e-300 ? 1.0 / std::sqrt(info[k].generalizedMass) : 0.0;
        const auto &shape = basis.modal->modalShapes[k];
        loadShape[k] = shapeAt(shape, load->second, input.loadDirection) * scale;
        respShape[k] = shapeAt(shape, resp->second, input.responseDirection) * scale;
        omega[k] = info[k].omega;
        highest = std::max(highest, omega[k]);
    }
    const double fMax = highest / (2.0 * kPi);
    double dt = std::min(0.005, 1.0 / std::max(1.0, 25.0 * fMax));
    dt = std::max(dt, input.duration / 400000.0);
    const std::size_t steps = static_cast<std::size_t>(std::ceil(input.duration / dt)) + 1;

    auto force = [&](double t) {
        switch (input.kind) {
        case TimeInput::Kind::Harmonic:
            return input.amplitude * std::sin(2.0 * kPi * input.frequency * t);
        case TimeInput::Kind::HalfSine:
            return (input.pulse > 0.0 && t <= input.pulse) ? input.amplitude * std::sin(kPi * t / input.pulse) : 0.0;
        case TimeInput::Kind::Table: {
            const auto &tab = input.table;
            if (tab.size() < 2 || t < tab.front().first || t > tab.back().first) {
                return 0.0;
            }
            for (std::size_t i = 1; i < tab.size(); ++i) {
                if (t <= tab[i].first) {
                    const double span = tab[i].first - tab[i - 1].first;
                    const double u = span > 0.0 ? (t - tab[i - 1].first) / span : 0.0;
                    return tab[i - 1].second + u * (tab[i].second - tab[i - 1].second);
                }
            }
            return 0.0;
        }
        }
        return 0.0;
    };

    constexpr double kBeta = 0.25, kGamma = 0.5;
    std::vector<double> q(usable, 0.0), v(usable, 0.0), a(usable, 0.0);
    r.t.reserve(steps);
    r.force.reserve(steps);
    r.displacement.reserve(steps);
    r.velocity.reserve(steps);
    r.acceleration.reserve(steps);
    auto record = [&](double t, double f) {
        double u = 0.0, vel = 0.0, acc = 0.0;
        for (std::size_t k = 0; k < usable; ++k) {
            u += respShape[k] * q[k];
            vel += respShape[k] * v[k];
            acc += respShape[k] * a[k];
        }
        r.t.push_back(t);
        r.force.push_back(f);
        r.displacement.push_back(u);
        r.velocity.push_back(vel);
        r.acceleration.push_back(acc);
    };
    const double f0 = force(0.0);
    for (std::size_t k = 0; k < usable; ++k) {
        a[k] = loadShape[k] * f0;
    }
    record(0.0, f0);
    for (std::size_t n = 1; n < steps; ++n) {
        const double t = static_cast<double>(n) * dt;
        const double f = force(t);
        for (std::size_t k = 0; k < usable; ++k) {
            const double w = omega[k], c = 2.0 * input.damping * w;
            const double keff = w * w + kGamma / (kBeta * dt) * c + 1.0 / (kBeta * dt * dt);
            const double p = loadShape[k] * f
                + (q[k] / (kBeta * dt * dt) + v[k] / (kBeta * dt) + (1.0 / (2.0 * kBeta) - 1.0) * a[k])
                + c * (kGamma / (kBeta * dt) * q[k] + (kGamma / kBeta - 1.0) * v[k] + dt * (kGamma / (2.0 * kBeta) - 1.0) * a[k]);
            const double qn = p / keff;
            const double an = (qn - q[k]) / (kBeta * dt * dt) - v[k] / (kBeta * dt) - (1.0 / (2.0 * kBeta) - 1.0) * a[k];
            const double vn = v[k] + dt * ((1.0 - kGamma) * a[k] + kGamma * an);
            q[k] = qn;
            v[k] = vn;
            a[k] = an;
        }
        record(t, f);
    }
    double sumSq = 0.0;
    for (std::size_t i = 0; i < r.t.size(); ++i) {
        r.peakDisplacement = std::max(r.peakDisplacement, std::abs(r.displacement[i]));
        r.peakAcceleration = std::max(r.peakAcceleration, std::abs(r.acceleration[i]));
        sumSq += r.acceleration[i] * r.acceleration[i];
    }
    r.rmsAcceleration = std::sqrt(sumSq / static_cast<double>(r.t.size()));
    for (std::size_t k = 0; k < usable; ++k) {
        r.staticDisplacement += respShape[k] * loadShape[k] * input.amplitude / (omega[k] * omega[k]);
    }
    r.ok = true;
    return r;
}

} // namespace Structura::App::Services
