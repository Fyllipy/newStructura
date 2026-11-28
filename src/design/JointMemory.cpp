#include "design/JointClassification.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace Structura::Design::JointClass {

namespace {

std::string fmt(double v, const char *spec = "%.5g")
{
    char buf[64];
    std::snprintf(buf, sizeof buf, spec, v);
    return buf;
}

std::string directionMemory(const char *title, const DirectionAnalysis &d, const JointAnalysis &a, const MemoryContext &ctx)
{
    std::ostringstream out;
    out << "\n" << title << "\n";
    if (!d.hasData) {
        out << "  Sem dados informados para este sentido: nada é calculado (não se presume simetria).\n";
        return out.str();
    }
    const StiffnessResult &s = d.stiffness;
    for (const std::string &m : s.messages) {
        out << "  Aviso: " << m << "\n";
    }
    if (!s.valid) {
        out << "  Si não pôde ser determinada.\n";
        return out.str();
    }
    out << "\n2. DETERMINAÇÃO DA RIGIDEZ Si (" << title << ")\n";
    if (s.momentRd > 0.0) {
        out << "  Mj,Rd = " << fmt(s.momentRd) << " kN·m\n";
        out << "  2/3 Mj,Rd = 2/3 × " << fmt(s.momentRd) << " = " << fmt(s.moment23) << " kN·m\n";
    }
    if (s.curveDriven) {
        out << "  θ(2/3 Mj,Rd) = " << fmt(s.theta23, "%.6g") << " rad (interpolada linearmente na curva informada, primeiro cruzamento do nível de 2/3 Mj,Rd)\n";
        out << "  Si = (2/3 × Mj,Rd) / θ(2/3 Mj,Rd) = " << fmt(s.moment23) << " / " << fmt(s.theta23, "%.6g") << " = " << fmt(s.stiffness) << " kN·m/rad\n";
    } else if (s.thetaDerived) {
        out << "  Si informada = " << fmt(s.stiffness) << " kN·m/rad  →  θ(2/3 Mj,Rd) = (2/3 Mj,Rd)/Si = " << fmt(s.theta23, "%.6g") << " rad (derivada)\n";
    } else if (s.theta23 > 0.0) {
        out << "  θ(2/3 Mj,Rd) informada = " << fmt(s.theta23, "%.6g") << " rad\n";
        out << "  Si = (2/3 × Mj,Rd) / θ(2/3 Mj,Rd) = " << fmt(s.moment23) << " / " << fmt(s.theta23, "%.6g") << " = " << fmt(s.stiffness) << " kN·m/rad\n";
    } else {
        out << "  Si informada = " << fmt(s.stiffness) << " kN·m/rad\n";
    }
    const Classification &c = d.classification;
    out << "\n3. LIMITES NORMATIVOS\n";
    const Limits &l = a.limits;
    if (l.referenceStiffness > 0.0) {
        out << "  E = " << fmt(ctx.youngModulus, "%.6g") << " kN/m²;  Iv = " << fmt(ctx.beamInertia, "%.6g") << " m⁴;  Lv = " << fmt(ctx.beamLength) << " m  (viga " << ctx.referenceMember << ")\n";
        out << "  E Iv / Lv = " << fmt(l.referenceStiffness) << " kN·m/rad\n";
        out << "  Smin = 0,5 × E Iv/Lv = " << fmt(l.sMin) << " kN·m/rad\n";
        out << "  Smax = 25 × E Iv/Lv = " << fmt(l.sMax) << " kN·m/rad\n";
        out << "  η = Si / (E Iv/Lv) = " << fmt(c.eta) << "\n";
    } else {
        out << "  Propriedades da viga insuficientes para calcular os limites.\n";
    }
    out << "\n4. CONDIÇÃO ADICIONAL Kv/Kp ≥ 0,1 (necessária apenas para classificar como rígida)\n";
    if (a.storey.status == StoreyStatus::Ok) {
        out << "  Kv = média de Iv/Lv das " << a.storey.beams << " viga(s) do pavimento = " << fmt(a.storey.kv, "%.6g") << " m³\n";
        out << "  Kp = média de Ip/Lp dos " << a.storey.columns << " pilar(es) do pavimento = " << fmt(a.storey.kp, "%.6g") << " m³\n";
        out << "  R = Kv/Kp = " << fmt(a.storey.ratio) << (a.storey.ratio >= Limits::kMinStoreyRatio ? "  ≥ 0,1: atendida\n" : "  < 0,1: NÃO atendida\n");
    } else {
        out << "  PENDENTE: " << a.storey.reason << "\n";
    }
    out << "\n5. RESULTADO (" << title << ")\n";
    out << "  Si = " << fmt(s.stiffness) << " kN·m/rad;  η = " << fmt(c.eta) << "\n";
    if (c.applicable && c.computed) {
        out << "  Classificação pela rigidez: " << className(c.byStiffness) << "\n";
        out << "  Classificação final: " << className(c.final) << (c.storeyPending ? " (rígida pendente da verificação Kv/Kp)" : "") << "\n";
    } else {
        out << "  Classificação: não aplicada.\n";
    }
    out << "  Justificativa: " << c.justification << "\n";
    return out.str();
}

} // namespace

bool bilinearEquivalent(const DirectionAnalysis &d, double &si, double &my, double &hardening)
{
    if (!d.hasData || !d.stiffness.valid || d.stiffness.momentRd <= 0.0 || d.stiffness.stiffness <= 0.0) {
        return false;
    }
    double thetaRd = 0.0;
    if (!thetaAtMoment(d.behavior.curve, d.stiffness.momentRd, thetaRd) || thetaRd <= 0.0) {
        return false;
    }
    const double theta23 = d.stiffness.moment23 / d.stiffness.stiffness;
    if (thetaRd <= theta23) {
        return false;
    }
    si = d.stiffness.stiffness;
    my = d.stiffness.moment23;
    hardening = ((d.stiffness.momentRd - d.stiffness.moment23) / (thetaRd - theta23)) / si;
    return true;
}

std::string originNature(DataOrigin origin)
{
    switch (origin) {
    case DataOrigin::ComponentMethod: return "CALCULADO pelo método dos componentes a partir da ligação cadastrada (dimensionada) no modelo";
    case DataOrigin::ExperimentalCurve: return "EXPERIMENTAL, informado pelo usuário (não calculado pelo programa)";
    case DataOrigin::NumericalCurve: return "NUMÉRICO, informado pelo usuário (não calculado pelo programa)";
    case DataOrigin::ImportedCurve: return "IMPORTADO de arquivo (não calculado pelo programa)";
    case DataOrigin::UserStiffness: return "INFORMADO pelo usuário (Si, Mj,Rd: procedimento independente)";
    case DataOrigin::ModelSpring: return "MODELAGEM do solver (mola/liberação de extremidade): só a rigidez, sem resistência";
    case DataOrigin::PreliminaryModel: return "PRELIMINAR: calculado com uma configuração de referência e hipóteses declaradas; NÃO é uma ligação dimensionada";
    case DataOrigin::None: break;
    }
    return "sem dados";
}

const DirectionAnalysis *JointAnalysis::primary() const
{
    if (positive.hasData && positive.stiffness.valid) {
        return &positive;
    }
    if (negative.hasData && negative.stiffness.valid) {
        return &negative;
    }
    return nullptr;
}

JointAnalysis analyzeJoint(const JointBehavior &behavior, const Limits &limits, const StoreyStiffness &storey, JointKind kind)
{
    JointAnalysis a;
    a.behavior = behavior;
    a.limits = limits;
    a.storey = storey;
    a.kind = kind;
    auto fill = [&](const DirectionBehavior &in, DirectionAnalysis &out) {
        out.behavior = in;
        out.hasData = in.hasData();
        if (!out.hasData) {
            return;
        }
        out.stiffness = computeStiffness(in);
        if (out.stiffness.valid) {
            out.classification = classify(out.stiffness.stiffness, limits, storey, kind);
        }
    };
    fill(behavior.positive, a.positive);
    fill(behavior.negative, a.negative);
    return a;
}

std::string referencesText()
{
    return "ABNT NBR 8800:2024 — Projeto de estruturas de aço e de estruturas mistas de aço e concreto de edificações, item 6.1.2 (rigidez das ligações entre viga e pilar),\n"
           "texto conferido no exemplar da norma do projeto (6.1.2.1 a 6.1.2.3).\n"
           "  6.1.2.1: rotulada Si <= 0,5 E Iv/Lv; rígida Si >= 25 E Iv/Lv (ver 6.1.2.2); semirrígida entre os limites. Si é a rigidez correspondente a 2/3 do\n"
           "  momento resistente de cálculo (simplificadamente, rigidez inicial). Pode ser determinada, na ausência de Norma Brasileira aplicável, conforme a\n"
           "  EN 1993-1-8, por resultados experimentais ou por análises numéricas; na análise elástica a ligação pode ser semirrígida com Si constante.\n"
           "  6.1.2.2: o limite de 25 E Iv/Lv só vale se, em cada andar, Kv/Kp >= 0,1 (Kv = média de Iv/Lv das vigas no topo do andar; Kp = média de Ip/Lp dos pilares\n"
           "  do andar); caso contrário a ligação é semirrígida.\n"
           "EN 1993-1-8:2005, 6.3 (método dos componentes: rigidez inicial Sj,ini), 6.3.1 (curva momento-rotação) e Tabelas 6.8 e 6.11 — implementado para mesas soldadas\n"
           "  e chapa de extremidade parafusada (ver docs/METODO_DOS_COMPONENTES.md). Mj,Rd vem das verificações de resistência da NBR 8800 feitas pelo programa.\n"
           "\nHipóteses e limitações:\n"
           " • A curva momento-rotação vem do método dos componentes (ligação definida no projeto e tipologia suportada), de uma curva importada/experimental/numérica,\n"
           "   ou de dados informados (Si, Mj,Rd, rotação a 2/3). Nenhuma curva é inventada; sem dados, o gráfico mostra dados insuficientes.\n"
           " • Rotação relativa da ligação (e não rotação absoluta do nó); momento-rotação (e não momento-curvatura).\n"
           " • O critério de 6.1.2 é aplicado somente a ligações viga-pilar. Cada par de barras do nó é uma ligação distinta; rigidezes não são somadas.\n"
           " • Curvas positiva e negativa são tratadas separadamente (não se presume simetria).\n"
           " • Lv e Lp são os comprimentos das barras do modelo; a norma define Lv de centro a centro de pilares e Lp como a altura do andar.\n"
           " • A capacidade de rotação não é calculada: a curva do método dos componentes termina em Mj,Rd.\n";
}

std::string buildMemory(const JointAnalysis &a, const MemoryContext &ctx)
{
    std::ostringstream out;
    out << "MEMÓRIA DE CÁLCULO — CLASSIFICAÇÃO DA LIGAÇÃO (ABNT NBR 8800:2024, 6.1.2)\n";
    out << "=====================================================================\n\n";
    out << "1. DADOS DE ENTRADA\n";
    out << "  Nó: " << ctx.nodeId << ";  barras selecionadas: " << ctx.memberA << " e " << ctx.memberB << "\n";
    out << "  Configuração: " << kindName(a.kind) << "\n";
    if (!ctx.planeNote.empty()) {
        out << "  Plano de análise: " << ctx.planeNote << "\n";
    }
    out << "  Origem dos dados da ligação: " << originName(a.behavior.origin) << "\n";
    out << "  Natureza: " << originNature(a.behavior.origin) << "\n";
    if (!a.behavior.description.empty()) {
        out << "  Descrição: " << a.behavior.description << "\n";
    }
    auto points = [&](const char *name, const DirectionBehavior &d) {
        if (d.curve.empty()) {
            return;
        }
        out << "  Curva (" << name << "): " << d.curve.size() << " ponto(s) θ[rad] ; M[kN·m]\n";
        for (const CurvePoint &p : d.curve) {
            out << "     " << fmt(p.theta, "%.6g") << " ; " << fmt(p.moment) << "\n";
        }
    };
    points("momento positivo", a.behavior.positive);
    points("momento negativo", a.behavior.negative);
    if (!a.positive.hasData && !a.negative.hasData) {
        out << "\n  NENHUM DADO DE COMPORTAMENTO DA LIGAÇÃO: a rigidez e a classificação não podem ser determinadas.\n";
        out << "  Informe a curva momento-rotação com Mj,Rd, ou Mj,Rd e a rotação a 2/3 de Mj,Rd, ou Si.\n";
    }
    out << directionMemory("Momento positivo", a.positive, a, ctx);
    out << directionMemory("Momento negativo", a.negative, a, ctx);
    if (!ctx.componentLines.empty()) {
        out << "\n6. MÉTODO DOS COMPONENTES (origem da curva)\n";
        for (const std::string &l : ctx.componentLines) {
            out << "  " << l << "\n";
        }
    }
    out << "\n7. HIPÓTESES E OBSERVAÇÕES\n";
    for (const std::string &n : ctx.notes) {
        out << "  - " << n << "\n";
    }
    out << "\n8. REFERÊNCIAS\n" << referencesText();
    return out.str();
}

std::string parseCurveText(const std::string &text, double thetaUnit, double momentUnit, std::vector<CurvePoint> &positive, std::vector<CurvePoint> &negative)
{
    positive.clear();
    negative.clear();
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        std::string cleaned;
        for (char ch : line) {
            if (ch == ';' || ch == '\t') {
                cleaned += ' ';
            } else if (ch == ',') {
                // virgula: decimal se nao houver ponto na linha e ha so um numero por campo; separador se houver ponto
                cleaned += (line.find('.') != std::string::npos) ? ' ' : '.';
            } else if (ch != '\r') {
                cleaned += ch;
            }
        }
        const auto first = cleaned.find_first_not_of(' ');
        if (first == std::string::npos || cleaned[first] == '#') {
            continue;
        }
        std::istringstream ls(cleaned);
        std::string a, b, extra;
        ls >> a >> b >> extra;
        if (a.empty() || b.empty() || !extra.empty()) {
            return "Linha " + std::to_string(lineNo) + ": esperado \"rotação momento\".";
        }
        char *e1 = nullptr, *e2 = nullptr;
        const double theta = std::strtod(a.c_str(), &e1) * thetaUnit;
        const double moment = std::strtod(b.c_str(), &e2) * momentUnit;
        if (*e1 != '\0' || *e2 != '\0' || !std::isfinite(theta) || !std::isfinite(moment)) {
            return "Linha " + std::to_string(lineNo) + ": número inválido.";
        }
        if (theta == 0.0 && moment == 0.0) {
            continue;
        }
        if (theta < 0.0 && moment > 0.0) {
            return "Linha " + std::to_string(lineNo) + ": rotação e momento com sinais opostos.";
        }
        if (theta > 0.0 && moment < 0.0) {
            return "Linha " + std::to_string(lineNo) + ": rotação e momento com sinais opostos.";
        }
        (theta < 0.0 || moment < 0.0 ? negative : positive).push_back({std::abs(theta), std::abs(moment)});
    }
    auto byTheta = [](const CurvePoint &x, const CurvePoint &y) { return x.theta < y.theta; };
    std::sort(positive.begin(), positive.end(), byTheta);
    std::sort(negative.begin(), negative.end(), byTheta);
    return {};
}

} // namespace Structura::Design::JointClass
