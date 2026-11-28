#include "design/JointClassification.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::JointClass {

std::string originName(DataOrigin origin)
{
    switch (origin) {
    case DataOrigin::ImportedCurve: return "curva momento-rotação importada";
    case DataOrigin::ExperimentalCurve: return "curva experimental";
    case DataOrigin::NumericalCurve: return "curva de análise numérica";
    case DataOrigin::UserStiffness: return "rigidez e momento resistente informados (procedimento independente)";
    case DataOrigin::ComponentMethod: return "método dos componentes (EN 1993-1-8)";
    case DataOrigin::ModelSpring: return "mola rotacional de extremidade do modelo";
    case DataOrigin::PreliminaryModel: return "modelo preliminar (configuração de referência, NÃO dimensionada)";
    case DataOrigin::None: break;
    }
    return "nenhum dado de ligação informado";
}

bool thetaAtMoment(const std::vector<CurvePoint> &curve, double moment, double &theta)
{
    if (moment <= 0.0) {
        theta = 0.0;
        return true;
    }
    CurvePoint previous {0.0, 0.0};
    for (const CurvePoint &point : curve) {
        if (point.theta <= previous.theta && !(point.theta == 0.0 && previous.theta == 0.0)) {
            continue;
        }
        if (previous.moment <= moment && moment <= point.moment && point.moment > previous.moment) {
            const double f = (moment - previous.moment) / (point.moment - previous.moment);
            theta = previous.theta + f * (point.theta - previous.theta);
            return true;
        }
        previous = point;
    }
    return false;
}

StiffnessResult computeStiffness(const DirectionBehavior &d)
{
    StiffnessResult r;
    const bool hasCurve = !d.curve.empty();
    if (!d.hasData()) {
        r.messages.push_back("Nenhum dado de ligação informado: a curva momento-rotação e a rigidez não podem ser determinadas.");
        return r;
    }
    if (hasCurve) {
        double last = 0.0;
        for (const CurvePoint &p : d.curve) {
            if (!(p.theta > last) || p.moment < 0.0 || !std::isfinite(p.theta) || !std::isfinite(p.moment)) {
                r.messages.push_back("Curva inválida: as rotações devem crescer estritamente a partir de zero e os momentos devem ser não negativos (valores em módulo).");
                return r;
            }
            last = p.theta;
        }
    }
    if (d.resistantMoment && *d.resistantMoment <= 0.0) {
        r.messages.push_back("Mj,Rd deve ser positivo.");
        return r;
    }
    if (d.resistantMoment) {
        r.momentRd = *d.resistantMoment;
        r.moment23 = 2.0 / 3.0 * r.momentRd;
    }
    if (hasCurve) {
        if (!d.resistantMoment) {
            r.messages.push_back("Mj,Rd não informado: sem ele não se define o nível de 2/3 do momento resistente (o máximo da curva não é adotado em seu lugar).");
            if (!d.stiffness) {
                return r;
            }
        } else {
            double theta = 0.0;
            if (!thetaAtMoment(d.curve, r.moment23, theta) || theta <= 0.0) {
                r.messages.push_back("A curva não alcança 2/3 de Mj,Rd: Si não pode ser lida da curva (verifique Mj,Rd ou estenda a curva).");
                return r;
            }
            r.theta23 = theta;
            r.stiffness = r.moment23 / theta;
            r.curveDriven = true;
            r.valid = true;
            if (d.stiffness && std::abs(*d.stiffness - r.stiffness) > 0.01 * r.stiffness) {
                r.messages.push_back("Si informada difere da calculada pela curva em mais de 1%: prevalece a obtida da curva.");
            }
            return r;
        }
    }
    if (d.stiffness) {
        if (*d.stiffness < 0.0) {
            r.messages.push_back("Si deve ser não negativa.");
            return r;
        }
        r.stiffness = *d.stiffness;
        r.valid = true;
        if (d.resistantMoment && r.stiffness > 0.0) {
            r.theta23 = r.moment23 / r.stiffness;
            r.thetaDerived = true;
        } else if (!d.resistantMoment) {
            r.messages.push_back("Mj,Rd não informado: a curva e o nível de 2/3 não podem ser desenhados; a classificação usa apenas Si.");
        }
        return r;
    }
    if (d.resistantMoment && d.theta23) {
        if (*d.theta23 <= 0.0) {
            r.messages.push_back("A rotação a 2/3 de Mj,Rd deve ser positiva.");
            return r;
        }
        r.theta23 = *d.theta23;
        r.stiffness = r.moment23 / r.theta23;
        r.valid = true;
        return r;
    }
    r.messages.push_back("Dados insuficientes: informe a curva com Mj,Rd, ou Mj,Rd com a rotação a 2/3 de Mj,Rd, ou Si.");
    return r;
}

std::string kindName(JointKind kind)
{
    switch (kind) {
    case JointKind::BeamColumn: return "viga-pilar";
    case JointKind::BeamBeam: return "viga-viga";
    case JointKind::ColumnColumn: return "pilar-pilar";
    case JointKind::DiagonalColumn: return "diagonal-pilar";
    case JointKind::DiagonalBeam: return "diagonal-viga";
    case JointKind::Other: break;
    }
    return "outra configuração";
}

std::string roleName(BarRole role)
{
    switch (role) {
    case BarRole::Beam: return "viga";
    case BarRole::Column: return "pilar";
    case BarRole::Diagonal: return "diagonal";
    }
    return "barra";
}

BarRole roleOfDirection(const std::array<double, 3> &d)
{
    const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-12) {
        return BarRole::Diagonal;
    }
    const double elevation = std::asin(std::clamp(std::abs(d[2]) / len, 0.0, 1.0)) * 180.0 / 3.14159265358979323846;
    if (elevation <= 10.0) {
        return BarRole::Beam;
    }
    if (elevation >= 80.0) {
        return BarRole::Column;
    }
    return BarRole::Diagonal;
}

JointKind kindOf(BarRole a, BarRole b)
{
    if (a > b) {
        std::swap(a, b);
    }
    if (a == BarRole::Beam && b == BarRole::Beam) {
        return JointKind::BeamBeam;
    }
    if (a == BarRole::Beam && b == BarRole::Column) {
        return JointKind::BeamColumn;
    }
    if (a == BarRole::Column && b == BarRole::Column) {
        return JointKind::ColumnColumn;
    }
    if (a == BarRole::Beam && b == BarRole::Diagonal) {
        return JointKind::DiagonalBeam;
    }
    if (a == BarRole::Column && b == BarRole::Diagonal) {
        return JointKind::DiagonalColumn;
    }
    return JointKind::Other;
}

StoreyStiffness storeyStiffness(const std::vector<StoreyBar> &beams, const std::vector<StoreyBar> &columns)
{
    StoreyStiffness s;
    auto mean = [](const std::vector<StoreyBar> &bars, int &count) {
        double sum = 0.0;
        count = 0;
        for (const StoreyBar &b : bars) {
            if (b.length > 0.0 && b.inertia > 0.0) {
                sum += b.inertia / b.length;
                ++count;
            }
        }
        return count > 0 ? sum / count : 0.0;
    };
    s.kv = mean(beams, s.beams);
    s.kp = mean(columns, s.columns);
    if (s.beams == 0) {
        s.reason = "Não foram encontradas vigas do pavimento (no plano e na cota do nó) com propriedades válidas.";
        return s;
    }
    if (s.columns == 0) {
        s.reason = "Não foram encontrados pilares do pavimento (chegando ao nível do nó) com propriedades válidas.";
        return s;
    }
    s.ratio = s.kv / s.kp;
    s.status = StoreyStatus::Ok;
    return s;
}

std::string className(StiffnessClass c)
{
    switch (c) {
    case StiffnessClass::Pinned: return "ROTULADA";
    case StiffnessClass::SemiRigid: return "SEMIRRÍGIDA";
    case StiffnessClass::Rigid: return "RÍGIDA";
    case StiffnessClass::NotClassified: break;
    }
    return "NÃO CLASSIFICADA";
}

Limits limitsFor(double e, double iv, double lv)
{
    Limits l;
    if (e > 0.0 && iv > 0.0 && lv > 0.0) {
        l.referenceStiffness = e * iv / lv;
        l.sMin = Limits::kPinnedFactor * l.referenceStiffness;
        l.sMax = Limits::kRigidFactor * l.referenceStiffness;
    }
    return l;
}

Classification classify(double stiffness, const Limits &limits, const StoreyStiffness &storey, JointKind kind)
{
    Classification c;
    c.stiffness = stiffness;
    c.applicable = kind == JointKind::BeamColumn;
    c.computed = true;
    if (limits.referenceStiffness > 0.0) {
        c.eta = stiffness / limits.referenceStiffness;
    }
    if (!c.applicable) {
        c.justification = "O critério de 6.1.2 foi formulado para ligações viga-pilar; a configuração " + kindName(kind)
                          + " não é classificada automaticamente. Si e a razão η são apresentados apenas como informação.";
        return c;
    }
    if (limits.referenceStiffness <= 0.0) {
        c.computed = false;
        c.justification = "Propriedades da viga (E, Iv, Lv) insuficientes para os limites.";
        return c;
    }
    if (stiffness <= limits.sMin) {
        c.byStiffness = c.final = StiffnessClass::Pinned;
        c.justification = "Si ≤ 0,5 E Iv/Lv: ligação rotulada.";
    } else if (stiffness >= limits.sMax) {
        c.byStiffness = StiffnessClass::Rigid;
        c.reachedRigidLimit = true;
        c.storeyConditionNeeded = true;
        if (storey.status == StoreyStatus::Pending) {
            c.storeyPending = true;
            c.final = StiffnessClass::SemiRigid;
            c.justification = "Si ≥ 25 E Iv/Lv (limite de rigidez alcançado), mas a condição Kv/Kp ≥ 0,1 não pôde ser verificada (" + storey.reason
                              + "). A classificação como rígida fica pendente; adota-se semirrígida até a verificação.";
        } else if (storey.ratio >= Limits::kMinStoreyRatio) {
            c.storeyConditionMet = true;
            c.final = StiffnessClass::Rigid;
            c.justification = "Si ≥ 25 E Iv/Lv e Kv/Kp ≥ 0,1: ligação rígida.";
        } else {
            c.final = StiffnessClass::SemiRigid;
            c.justification = "Si ≥ 25 E Iv/Lv (limite de rigidez alcançado), mas Kv/Kp < 0,1: a ligação é classificada como semirrígida.";
        }
    } else {
        c.byStiffness = c.final = StiffnessClass::SemiRigid;
        c.justification = "0,5 E Iv/Lv < Si < 25 E Iv/Lv: ligação semirrígida.";
    }
    return c;
}

} // namespace Structura::Design::JointClass
