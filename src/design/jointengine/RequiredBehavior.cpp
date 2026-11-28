#include "design/jointengine/RequiredBehavior.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace Structura::Design::JointEngine {

namespace JS = Structura::Design::JointClass;

namespace {

std::string fmt(const char *format, double a = 0.0, double b = 0.0, double c = 0.0)
{
    char buffer[768];
    std::snprintf(buffer, sizeof buffer, format, a, b, c);
    return buffer;
}

std::string num(double v, const char *f = "%.4g") { return fmt(f, v); }

} // namespace

std::string nodeMemberRoleName(NodeMemberRole role)
{
    switch (role) {
    case NodeMemberRole::Beam: return "viga";
    case NodeMemberRole::Column: return "pilar";
    case NodeMemberRole::Diagonal: return "diagonal";
    case NodeMemberRole::Unclassified: return "não classificada";
    }
    return "?";
}

std::string rotationalConditionName(RotationalCondition c)
{
    switch (c) {
    case RotationalCondition::ContinuousImplicit: return "contínua (padrão do modelo, sem declaração)";
    case RotationalCondition::Released: return "liberada à rotação (declarada)";
    case RotationalCondition::Spring: return "mola rotacional (declarada)";
    case RotationalCondition::Unknown: return "desconhecida";
    }
    return "?";
}

std::string requiredKindName(RequiredKind k)
{
    switch (k) {
    case RequiredKind::Rigid: return "Rígido";
    case RequiredKind::SemiRigid: return "Semirrígido";
    case RequiredKind::Pinned: return "Rotulado";
    case RequiredKind::Undetermined: return "Indeterminado";
    }
    return "?";
}

std::string requirementBasisName(RequirementBasis b)
{
    switch (b) {
    case RequirementBasis::ModelImplicitContinuity: return "continuidade implícita do modelo (não declarada)";
    case RequirementBasis::ModelRelease: return "liberação de rotação declarada no modelo";
    case RequirementBasis::ModelSpring: return "mola rotacional declarada no modelo";
    case RequirementBasis::NotApplicable: return "critério não aplicável";
    case RequirementBasis::InsufficientData: return "dados insuficientes";
    }
    return "?";
}

std::string statusLabelPt(ValidityStatus status)
{
    switch (status) {
    case ValidityStatus::Valid: return "determinado";
    case ValidityStatus::ValidWithCaveats: return "determinado, com ressalvas";
    case ValidityStatus::InsufficientData: return "dados insuficientes";
    case ValidityStatus::OutOfDomain: return "critério não aplicável";
    case ValidityStatus::Unsupported: return "sem critério implementado";
    default: return statusName(status);
    }
}

std::string systemConsistencyName(SystemConsistency c)
{
    switch (c) {
    case SystemConsistency::Undetermined: return "indeterminada";
    case SystemConsistency::Consistent: return "compatível";
    case SystemConsistency::NeedsAttention: return "REQUER ATENÇÃO";
    }
    return "?";
}

std::string rigidAdmissibilityName(RigidAdmissibility a)
{
    switch (a) {
    case RigidAdmissibility::Admissible: return "admitida (Kv/Kp ≥ 0,1)";
    case RigidAdmissibility::NotAdmissible: return "NÃO admitida (Kv/Kp < 0,1)";
    case RigidAdmissibility::Unknown: return "indeterminada";
    }
    return "?";
}

RequiredBehaviorAssessment assessRequiredBehavior(const NodeFacts &facts)
{
    RequiredBehaviorAssessment out;
    out.nodeId = facts.nodeId;
    out.standard = facts.standard;
    out.edition = facts.edition;
    out.structuralSystem = facts.structuralSystem;
    out.bracing = facts.bracing;
    out.members = facts.members;

    const bool standardSupported = facts.standard == "NBR 8800" && facts.edition == "2024";

    // ------------------------------------------------------------------------------------------------- lacunas e notas do nó
    const char *systemNames[] = {"não declarado", "pórtico de vigas e pilares", "galpão / um pavimento", "treliça", "outro"};
    const bool systemDeclared = facts.structuralSystem >= 1 && facts.structuralSystem <= 4;
    const bool bracingDeclared = facts.bracing == 1 || facts.bracing == 2;
    if (!systemDeclared || !bracingDeclared) {
        out.nodeGaps.push_back(std::string("Sistema estrutural e/ou condição de contraventamento NÃO declarados nas propriedades do projeto (sistema: ") + (systemDeclared ? systemNames[facts.structuralSystem] : "não declarado") +
                               "; contraventamento: " + (bracingDeclared ? (facts.bracing == 1 ? "contraventada" : "não contraventada") : "não declarada") +
                               "): os critérios que dependem deles ficam insuficientes; nenhum valor padrão foi assumido.");
    } else {
        out.nodeNotes.push_back(std::string("Declarado nas propriedades do projeto: sistema = ") + systemNames[facts.structuralSystem] + "; contraventamento = " +
                                (facts.bracing == 1 ? "contraventada (estabilidade lateral por contraventamento/núcleos)" : "não contraventada (estabilidade lateral pelos pórticos)") + ".");
    }
    if (!standardSupported) {
        out.nodeGaps.push_back("Os critérios implementados são os da NBR 8800:2024; norma/edição selecionada: " + facts.standard + " " + facts.edition + " — sem critério implementado.");
    }
    if (facts.standardOrigin.empty()) {
        out.nodeGaps.push_back("Origem da norma selecionada não informada.");
    }
    bool restrained = false;
    for (const int r : facts.restraints) {
        restrained = restrained || r != 0;
    }
    if (restrained) {
        std::string dofs;
        const char *names[] = {"ux", "uy", "uz", "rx", "ry", "rz"};
        for (std::size_t i = 0; i < 6; ++i) {
            if (facts.restraints[i] != 0) {
                dofs += std::string(dofs.empty() ? "" : ", ") + names[i];
            }
        }
        out.nodeNotes.push_back("O nó é apoiado (graus de liberdade restritos: " + dofs + "): o apoio vincula o nó; a rotação relativa viga–pilar continua a ser a da ligação.");
    }
    int beams = 0, columns = 0, diagonals = 0, unclassified = 0;
    for (const NodeMemberFacts &m : facts.members) {
        beams += m.role == NodeMemberRole::Beam;
        columns += m.role == NodeMemberRole::Column;
        diagonals += m.role == NodeMemberRole::Diagonal;
        unclassified += m.role == NodeMemberRole::Unclassified;
        if (m.hasEccentricity) {
            out.nodeNotes.push_back("Barra " + std::to_string(m.memberId) + " tem excentricidade de eixo ou zona rígida neste nó: o critério de rigidez NÃO considera excentricidades.");
        }
    }
    out.nodeNotes.push_back("Barras no nó: " + std::to_string(facts.members.size()) + " (vigas " + std::to_string(beams) + ", pilares " + std::to_string(columns) + ", diagonais " + std::to_string(diagonals) +
                            (unclassified ? ", não classificadas " + std::to_string(unclassified) : std::string()) + ").");
    out.nodeNotes.push_back("Papéis pelo eixo da barra em relação à horizontal (hipótese): ≤ 10° viga; ≥ 80° pilar; intermediário diagonal. O modelo não declara o papel estrutural das barras.");
    out.nodeNotes.push_back("Esforços solicitantes não são usados: nenhum critério da NBR 8800:2024, 6.1.2 depende deles (a rigidez requerida não é função da solicitação).");
    for (const std::string &n : facts.modelNotes) {
        out.nodeNotes.push_back(n);
    }

    // ----------------------------------------------------------------------------------------------------------- itens (um por par)
    int determined = 0, insufficient = 0, notApplicable = 0;
    for (const PairFacts &p : facts.pairs) {
        RequiredBehaviorItem item;
        item.beamId = p.beamId;
        item.otherId = p.otherId;
        item.kind = p.kind;
        item.notes = p.notes;
        item.criteria = {"NBR 8800:2024, 6.1.2.1: rotulada se Si ≤ 0,5 EIv/Lv; rígida se Si ≥ 25 EIv/Lv; intermediária = semirrígida (Si = rigidez a 2/3 do momento resistente de cálculo da ligação)",
                         "NBR 8800:2024, 6.1.2.2: o limite Si ≥ 25 EIv/Lv só pode ser usado se, em cada andar, Kv/Kp ≥ 0,1; senão a ligação deve ser considerada semirrígida",
                         "NBR 8800:2024, 6.1.2.3: ligações usuais tradicionalmente rotuladas ou rígidas podem ser simuladas com esses vínculos, a critério do responsável técnico",
                         "NBR 8800:2024, 6.1.3 e 6.1.4: ligações flexíveis devem permitir a rotação de vigas simplesmente apoiadas sem colapso; ligações rígidas/semirrígidas exigem considerar os efeitos combinados dos esforços da rigidez"};
        item.assumptions = {"Idealização do modelo lida da extremidade da viga neste nó (liberação, mola ou continuidade), em torno do eixo normal ao plano de análise.",
                            "Os limites são metas para a ligação a projetar, calculadas com E, Iv e Lv da viga do modelo; NÃO classificam nenhuma ligação existente."};
        if (!standardSupported) {
            item.status = ValidityStatus::Unsupported;
            item.basis = RequirementBasis::InsufficientData;
            item.gaps.push_back("Norma/edição sem critério implementado: " + facts.standard + " " + facts.edition + ".");
            item.statement = "Sem critério implementado para a norma selecionada.";
            ++insufficient;
            out.items.push_back(item);
            continue;
        }
        if (p.kind != JS::JointKind::BeamColumn) {
            item.status = ValidityStatus::OutOfDomain;
            item.basis = RequirementBasis::NotApplicable;
            item.required = RequiredKind::Undetermined;
            item.statement = "O critério de rigidez de 6.1.2 aplica-se somente a ligações viga–pilar; o par " + std::to_string(p.beamId) + "–" + std::to_string(p.otherId) + " é do tipo \"" +
                             JS::kindName(p.kind) + "\".";
            ++notApplicable;
            out.items.push_back(item);
            continue;
        }
        item.limitsAvailable = p.limitsAvailable;
        item.limits = p.limits;
        item.storey = p.storey;
        // metas de rigidez
        if (p.limitsAvailable && p.limits.referenceStiffness > 0.0) {
            item.referenceKnm = p.limits.referenceStiffness;
            item.pinnedMaxKnm = p.limits.sMin;
            item.rigidMinKnm = p.limits.sMax;
        } else {
            item.gaps.push_back("E, Iv ou Lv da viga " + std::to_string(p.beamId) + " ausentes ou nulos: as metas de rigidez de 6.1.2.1 não podem ser calculadas.");
        }
        if (p.storey.status == JS::StoreyStatus::Ok) {
            item.kvKp = p.storey.ratio;
            item.rigidAdmissibility = p.storey.ratio >= JS::Limits::kMinStoreyRatio ? RigidAdmissibility::Admissible : RigidAdmissibility::NotAdmissible;
        } else {
            item.rigidAdmissibility = RigidAdmissibility::Unknown;
            item.gaps.push_back("Kv/Kp do andar indeterminado (" + (p.storey.reason.empty() ? std::string("sem dados") : p.storey.reason) + "): a admissibilidade da meta rígida (6.1.2.2) não pode ser avaliada.");
        }
        // idealização do modelo na extremidade da viga
        switch (p.beamCondition) {
        case RotationalCondition::Released:
            item.required = RequiredKind::Pinned;
            item.basis = RequirementBasis::ModelRelease;
            break;
        case RotationalCondition::Spring:
            item.required = RequiredKind::SemiRigid;
            item.basis = RequirementBasis::ModelSpring;
            if (!std::isnan(p.beamSpringKnm)) {
                item.springKnm = p.beamSpringKnm;
            }
            break;
        case RotationalCondition::ContinuousImplicit:
            item.required = RequiredKind::Rigid;
            item.basis = RequirementBasis::ModelImplicitContinuity;
            break;
        case RotationalCondition::Unknown:
            item.required = RequiredKind::Undetermined;
            item.basis = RequirementBasis::InsufficientData;
            item.gaps.push_back("Condição de rotação da extremidade da viga " + std::to_string(p.beamId) + " não determinada no modelo.");
            break;
        }
        // orientação
        const bool targets = !std::isnan(item.referenceKnm);
        const std::string beamTxt = "viga " + std::to_string(p.beamId) + " (pilar/barra " + std::to_string(p.otherId) + ")";
        if (item.required == RequiredKind::Pinned) {
            item.statement = "Comportamento requerido: ROTULADO (liberação de rotação declarada no modelo na " + beamTxt + ").";
            if (targets) {
                item.statement += " A ligação a projetar deverá ter Si ≤ " + num(item.pinnedMaxKnm, "%.5g") + " kN·m/rad (0,5 EIv/Lv, 6.1.2.1) e permitir a rotação da viga simplesmente apoiada sem colapso (6.1.3).";
            }
        } else if (item.required == RequiredKind::SemiRigid) {
            item.statement = "Comportamento requerido: SEMIRRÍGIDO (mola rotacional declarada no modelo na " + beamTxt + ", k = " + num(p.beamSpringKnm, "%.5g") + " kN·m/rad).";
            if (targets) {
                item.statement += " Faixa semirrígida de 6.1.2.1: " + num(item.pinnedMaxKnm, "%.5g") + " < Si < " + num(item.rigidMinKnm, "%.5g") + " kN·m/rad; a ligação deverá ter rigidez inicial compatível com k (que a análise elástica toma constante).";
                if (!std::isnan(p.beamSpringKnm) && p.beamSpringKnm <= item.pinnedMaxKnm) {
                    item.notes.push_back("k do modelo está na faixa de ligação rotulada (≤ 0,5 EIv/Lv): pela norma tal ligação seria considerada rotulada.");
                }
                if (!std::isnan(p.beamSpringKnm) && p.beamSpringKnm >= item.rigidMinKnm) {
                    item.notes.push_back("k do modelo está na faixa de ligação rígida (≥ 25 EIv/Lv)" +
                                         std::string(item.rigidAdmissibility == RigidAdmissibility::Admissible ? "." : " (a meta rígida só é admitida com Kv/Kp ≥ 0,1)."));
                }
            }
        } else if (item.required == RequiredKind::Rigid) {
            item.statement = "Comportamento requerido pela análise: RÍGIDO — mas por CONTINUIDADE IMPLÍCITA do modelo na " + beamTxt + ": o projetista não declarou a idealização.";
            if (targets) {
                item.statement += " Para sustentar a continuidade, a ligação deverá ter Si ≥ " + num(item.rigidMinKnm, "%.5g") + " kN·m/rad (25 EIv/Lv, 6.1.2.1)";
                if (item.rigidAdmissibility == RigidAdmissibility::Admissible) {
                    item.statement += "; meta admitida (Kv/Kp = " + num(item.kvKp) + " ≥ 0,1, 6.1.2.2).";
                } else if (item.rigidAdmissibility == RigidAdmissibility::NotAdmissible) {
                    item.statement += "; PORÉM Kv/Kp = " + num(item.kvKp) + " < 0,1: pela 6.1.2.2 a ligação, mesmo com Si ≥ 25 EIv/Lv, deve ser considerada SEMIRRÍGIDA — a idealização rígida não se sustenta por este critério (alternativas: justificar pela 6.1.2.3, a critério do responsável técnico, ou modelar a mola).";
                } else {
                    item.statement += "; admissibilidade (Kv/Kp ≥ 0,1) indeterminada.";
                }
            }
            item.gaps.push_back("DECISÃO DE PROJETO pendente: o modelo não declara se a ligação deve ser rotulada, semirrígida ou rígida; as três alternativas e suas metas estão acima — o programa não escolhe entre elas.");
        } else {
            item.statement = "Comportamento requerido indeterminado: dados insuficientes.";
        }
        // compatibilidade qualitativa com a estabilidade lateral DECLARADA (nenhum limiar numérico; sem declaração, indeterminada)
        const bool framed = facts.structuralSystem == 1 || facts.structuralSystem == 2;
        if (item.required == RequiredKind::Undetermined) {
            item.systemConsistency = SystemConsistency::Undetermined;
        } else if (!bracingDeclared) {
            item.systemConsistency = SystemConsistency::Undetermined;
            item.gaps.push_back("Condição de contraventamento não declarada nas propriedades do projeto: a compatibilidade do comportamento requerido com a estabilidade lateral não pôde ser avaliada.");
        } else if (systemDeclared && !framed) {
            item.systemConsistency = SystemConsistency::Undetermined;
            item.gaps.push_back(std::string("Sistema declarado (") + systemNames[facts.structuralSystem] + ") não é porticado: a orientação por contraventamento aplica-se a pórticos de vigas e pilares.");
        } else if (item.required == RequiredKind::Pinned && facts.bracing == 2) {
            item.systemConsistency = SystemConsistency::NeedsAttention;
            item.systemNote = "Ligação viga–pilar rotulada em estrutura NÃO contraventada: a estabilidade lateral depende da ação de pórtico; rotular esta ligação retira rigidez lateral. Só é compatível se a estabilidade for garantida por outras ligações rígidas/semirrígidas, por apoios do pórtico ou por outro sistema (verifique no modelo e nas verificações de estabilidade).";
        } else if (item.required == RequiredKind::Pinned) {
            item.systemConsistency = SystemConsistency::Consistent;
            item.systemNote = "Estrutura contraventada: ligação rotulada compatível com a estabilidade lateral declarada (a estabilidade não depende desta ligação).";
        } else if (facts.bracing == 1) {
            item.systemConsistency = SystemConsistency::Consistent;
            item.systemNote = "Estrutura contraventada: a continuidade/semirrigidez é admissível, mas não é necessária para a estabilidade lateral; é decisão de economia e de deformações.";
        } else {
            item.systemConsistency = SystemConsistency::Consistent;
            item.systemNote = "Estrutura não contraventada: a ação de pórtico exige ligações que transmitam momento; as verificações de estabilidade e de segunda ordem não são feitas aqui.";
        }
        if (item.required == RequiredKind::Undetermined || !targets) {
            item.status = ValidityStatus::InsufficientData;
            ++insufficient;
        } else {
            // Valid só sem lacunas e com o sistema declarado compatível; a continuidade implícita sempre mantém a lacuna de decisão de projeto
            item.status = item.gaps.empty() && item.systemConsistency == SystemConsistency::Consistent ? ValidityStatus::Valid : ValidityStatus::ValidWithCaveats;
            ++determined;
        }
        out.items.push_back(item);
    }
    if (out.items.empty()) {
        out.nodeGaps.push_back("Nenhum par de barras avaliável no nó.");
    }
    out.status = determined > 0 ? ValidityStatus::ValidWithCaveats : (insufficient > 0 ? ValidityStatus::InsufficientData : (notApplicable > 0 ? ValidityStatus::OutOfDomain : ValidityStatus::InsufficientData));
    return out;
}

std::string RequiredBehaviorAssessment::memory() const
{
    std::ostringstream s;
    s << "COMPORTAMENTO ESTRUTURAL REQUERIDO DO NÓ " << nodeId << " (anterior à definição da ligação)\n";
    s << "Norma: " << standard << ":" << edition << "    Estado: " << statusLabelPt(status) << "\n";
    s << "Este resultado NÃO classifica uma ligação real: a classificação normativa da rigidez só é possível depois de definida a ligação e calculada a sua curva M–θ.\n\n";
    s << "BARRAS DO NÓ\n";
    for (const NodeMemberFacts &m : members) {
        s << "  barra " << m.memberId << (m.label.empty() ? "" : " [" + m.label + "]") << ": " << nodeMemberRoleName(m.role) << "; L = " << (std::isnan(m.lengthM) ? "n/d" : num(m.lengthM, "%.4g") + " m")
          << "; I(plano) = " << (std::isnan(m.inertiaM4) ? "n/d" : num(m.inertiaM4, "%.5g") + " m4") << "; E = " << (std::isnan(m.youngKnM2) ? "n/d" : num(m.youngKnM2, "%.5g") + " kN/m2")
          << "; rotação na extremidade: " << rotationalConditionName(m.condition)
          << (m.condition == RotationalCondition::Spring && !std::isnan(m.springKnm) ? " k = " + num(m.springKnm, "%.5g") + " kN·m/rad" : std::string()) << "\n";
    }
    s << "\nRESULTADO POR LIGAÇÃO (par de barras)\n";
    for (const RequiredBehaviorItem &i : items) {
        s << "  Par " << i.beamId << "–" << i.otherId << " (" << JS::kindName(i.kind) << "): " << requiredKindName(i.required) << " — " << requirementBasisName(i.basis) << " [" << statusLabelPt(i.status) << "]\n";
        s << "    " << i.statement << "\n";
        if (!std::isnan(i.referenceKnm)) {
            s << "    EIv/Lv = " << num(i.referenceKnm, "%.5g") << " kN·m/rad; rotulada: Si ≤ " << num(i.pinnedMaxKnm, "%.5g") << "; rígida: Si ≥ " << num(i.rigidMinKnm, "%.5g")
              << "; Kv/Kp = " << (std::isnan(i.kvKp) ? std::string("indeterminado") : num(i.kvKp)) << " → meta rígida " << rigidAdmissibilityName(i.rigidAdmissibility) << "\n";
        }
        s << "    Compatibilidade com o sistema/contraventamento declarados: " << systemConsistencyName(i.systemConsistency) << (i.systemNote.empty() ? "" : " — " + i.systemNote) << "\n";
        for (const std::string &g : i.gaps) {
            s << "    LACUNA: " << g << "\n";
        }
        for (const std::string &n : i.notes) {
            s << "    Nota: " << n << "\n";
        }
    }
    s << "\nCRITÉRIOS APLICADOS\n";
    if (!items.empty()) {
        for (const std::string &c : items.front().criteria) {
            s << "  - " << c << "\n";
        }
    }
    s << "\nHIPÓTESES\n";
    if (!items.empty()) {
        for (const std::string &a : items.front().assumptions) {
            s << "  - " << a << "\n";
        }
    }
    s << "\nLACUNAS E LIMITAÇÕES DO NÓ\n";
    for (const std::string &g : nodeGaps) {
        s << "  - " << g << "\n";
    }
    for (const std::string &n : nodeNotes) {
        s << "  - " << n << "\n";
    }
    return s.str();
}

} // namespace Structura::Design::JointEngine
