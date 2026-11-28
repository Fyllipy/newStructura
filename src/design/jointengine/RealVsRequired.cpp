#include "design/jointengine/RealVsRequired.h"

#include "design/jointengine/NbrClassifier.h"

#include <cmath>
#include <cstdio>
#include <sstream>

namespace Structura::Design::JointEngine {

namespace JS = Structura::Design::JointClass;

std::string comparisonOutcomeName(ComparisonOutcome outcome)
{
    switch (outcome) {
    case ComparisonOutcome::Meets: return "Atende";
    case ComparisonOutcome::DoesNotMeet: return "Não atende";
    case ComparisonOutcome::Indeterminate: return "Indeterminado";
    case ComparisonOutcome::NotApplicable: return "Não aplicável";
    }
    return "Indeterminado";
}

namespace {

std::string num(double v, const char *fmt = "%.5g")
{
    if (!std::isfinite(v)) {
        return "n/d";
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, fmt, v);
    return buf;
}

std::string requiredText(RequiredKind k)
{
    switch (k) {
    case RequiredKind::Pinned: return "rotulado";
    case RequiredKind::SemiRigid: return "semirrígido";
    case RequiredKind::Rigid: return "rígido";
    case RequiredKind::Undetermined: return "indeterminado";
    }
    return "indeterminado";
}

std::string classText(JS::StiffnessClass c)
{
    switch (c) {
    case JS::StiffnessClass::Pinned: return "rotulada";
    case JS::StiffnessClass::SemiRigid: return "semirrígida";
    case JS::StiffnessClass::Rigid: return "rígida";
    case JS::StiffnessClass::NotClassified: return "não classificada";
    }
    return "não classificada";
}

void finish(ConnectionRequirementComparison &c)
{
    switch (c.outcome) {
    case ComparisonOutcome::NotApplicable: c.status = ValidityStatus::OutOfDomain; break;
    case ComparisonOutcome::Indeterminate: c.status = ValidityStatus::InsufficientData; break;
    default: c.status = c.caveats.empty() && c.gaps.empty() ? ValidityStatus::Valid : ValidityStatus::ValidWithCaveats; break;
    }
}

} // namespace

ConnectionRequirementComparison compareRealWithRequired(const RequiredBehaviorItem &item, const MechanicalResponse *response, const std::string &unavailableReason, const std::string &directionLabel)
{
    ConnectionRequirementComparison out;
    out.directionLabel = directionLabel;
    out.beamId = item.beamId;
    out.otherId = item.otherId;
    out.required = item.required;
    out.basis = item.basis;
    out.real.standard = "NBR 8800";
    out.real.edition = "2024";
    out.real.criteria = {"NBR 8800:2024, 6.1.2.1 (limites de rigidez: Si ≤ 0,5 EIv/Lv rotulada; Si ≥ 25 EIv/Lv rígida; intermediária semirrígida; Si = rigidez a 2/3 de Mj,Rd)",
                         "NBR 8800:2024, 6.1.2.2 (a classe rígida só vale com Kv/Kp ≥ 0,1 no andar; senão a ligação é considerada semirrígida)"};
    out.hypotheses = {"A classe da ligação real é a de `JointClassification` (mesmo módulo do requerido), com E, Iv, Lv e Kv/Kp do MESMO modelo e a rigidez a 2/3 de Mj,Rd da curva M–θ do Incremento 3.",
                      "Só o sentido positivo do momento é avaliado (a resposta mecânica não calcula o sentido inverso; não se presume simetria)."};
    if (!directionLabel.empty()) {
        out.hypotheses[1] = "Resposta do sentido " + directionLabel + ": a classificação usa a rigidez Si (2/3 de Mj,Rd) e o Mj,Rd DESTE sentido; os sentidos são classificados separadamente (a NBR 8800:2024, 6.1.2 não define regra para combiná-los e o programa não cria média).";
    }
    for (const std::string &g : item.gaps) {
        out.gaps.push_back("[requerido] " + g);
    }

    // ----------------------------------------------------------------------------------------------- domínio
    if (item.basis == RequirementBasis::NotApplicable || item.kind != JS::JointKind::BeamColumn) {
        out.outcome = ComparisonOutcome::NotApplicable;
        out.summary = "Não aplicável: o critério de rigidez de 6.1.2 vale somente para ligações viga–pilar (" + std::string(JS::kindName(item.kind)) + ").";
        out.reasons.push_back(out.summary);
        finish(out);
        return out;
    }
    if (item.status == ValidityStatus::Unsupported) {
        out.outcome = ComparisonOutcome::Indeterminate;
        out.summary = "Indeterminado: a norma/edição selecionada não tem critério de classificação implementado.";
        out.reasons.push_back(out.summary);
        finish(out);
        return out;
    }
    if (item.required == RequiredKind::Undetermined) {
        out.outcome = ComparisonOutcome::Indeterminate;
        out.summary = "Indeterminado: o comportamento requerido do nó não pôde ser determinado a partir do modelo.";
        out.reasons.push_back(out.summary);
        finish(out);
        return out;
    }
    if (!item.limitsAvailable || !(item.limits.referenceStiffness > 0.0)) {
        out.outcome = ComparisonOutcome::Indeterminate;
        out.summary = "Indeterminado: E, Iv ou Lv da viga ausentes — os limites de 6.1.2.1 não podem ser calculados.";
        out.reasons.push_back(out.summary);
        out.gaps.push_back("E, Iv ou Lv da viga do modelo ausentes ou nulos.");
        finish(out);
        return out;
    }
    out.real.limits = item.limits;
    out.real.kvKp = item.kvKp;
    if (item.required == RequiredKind::SemiRigid && item.springKnm) {
        out.springKnm = *item.springKnm;
    }

    // ----------------------------------------------------------------------------------------------- resposta mecânica
    if (response == nullptr || !allowsNumbers(response->status) || response->kind != ResponseKind::MomentRotation || response->curves.empty()) {
        out.outcome = ComparisonOutcome::Indeterminate;
        const bool partial = response != nullptr && allowsNumbers(response->status) && response->kind == ResponseKind::StiffnessOnly;
        const bool resistanceOnly = response != nullptr && allowsNumbers(response->status) && response->kind == ResponseKind::ResistanceOnly;
        out.summary = resistanceOnly ? "Indeterminado: a resposta mecânica tem só a resistência (Mj,Rd da NBR, nível C); a classificação de rigidez exige Sj,ini (rigidez a 2/3 de Mj,Rd), que esta tipologia não calcula."
                    : partial ? "Indeterminado: a resposta mecânica tem só a rigidez inicial (sem Mj,Rd não há rigidez a 2/3 de Mj,Rd, exigida pela classificação)."
                              : "Indeterminado: a ligação definida não tem resposta M–θ calculada" + (unavailableReason.empty() ? std::string(".") : " (" + unavailableReason + ").");
        out.reasons.push_back(out.summary);
        out.gaps.push_back("Sem resposta M–θ com Mj,Rd da ligação definida: a rigidez inicial não está disponível para a classificação normativa.");
        finish(out);
        return out;
    }
    if (response->initialStiffness) {
        out.real.initialStiffnessKnm = response->initialStiffness->value;
    }

    // ----------------------------------------------------------------------------------------------- classificação (módulo existente)
    const NbrClassifier classifier(item.limits, item.storey, item.kind);
    const JS::JointAnalysis analysis = classifier.analyze(*response);
    const JS::DirectionAnalysis *primary = analysis.primary();
    if (primary == nullptr || !primary->hasData || !primary->classification.computed || !primary->classification.applicable || !primary->stiffness.valid) {
        out.outcome = ComparisonOutcome::Indeterminate;
        out.summary = "Indeterminado: a classificação normativa não pôde ser calculada (" +
                      (primary == nullptr || !primary->hasData ? std::string("sem dados de comportamento M–θ") : primary->classification.justification) + ").";
        out.reasons.push_back(out.summary);
        if (primary != nullptr) {
            for (const std::string &m : primary->stiffness.messages) {
                out.gaps.push_back(m);
            }
        }
        finish(out);
        return out;
    }
    const JS::Classification &cl = primary->classification;
    out.real.available = true;
    out.real.applicable = true;
    out.real.stiffnessKnm = cl.stiffness;
    out.real.eta = cl.eta;
    out.real.byStiffness = cl.byStiffness;
    out.real.finalClass = cl.final;
    out.real.storeyNeeded = cl.storeyConditionNeeded;
    out.real.storeyMet = cl.storeyConditionMet;
    out.real.storeyPending = cl.storeyPending;
    out.real.classLabel = cl.storeyPending ? "semirrígida ou rígida (pendente de Kv/Kp)" : classText(cl.final);
    out.real.justification = cl.justification;
    if (std::isfinite(out.real.initialStiffnessKnm) && std::abs(out.real.stiffnessKnm - out.real.initialStiffnessKnm) > 1e-6 * std::abs(out.real.initialStiffnessKnm)) {
        out.caveats.push_back("Si lido da curva (" + num(out.real.stiffnessKnm) + " kN·m/rad) difere de Sj,ini informado pela resposta (" + num(out.real.initialStiffnessKnm) + " kN·m/rad): usa-se o Si da curva, como pede 6.1.2.1.");
    }

    // ----------------------------------------------------------------------------------------------- comparação
    const JS::StiffnessClass by = cl.byStiffness;
    const JS::StiffnessClass fin = cl.final;
    const std::string si = "Si = " + num(out.real.stiffnessKnm) + " kN·m/rad (η = Si/(EIv/Lv) = " + num(out.real.eta, "%.4g") + ")";
    const std::string limitsTxt = "limites: rotulada ≤ " + num(item.limits.sMin) + "; rígida ≥ " + num(item.limits.sMax) + " kN·m/rad";
    const std::string req = requiredText(item.required);
    ComparisonOutcome oc = ComparisonOutcome::Indeterminate;
    std::string why;
    switch (item.required) {
    case RequiredKind::Pinned:
        if (by == JS::StiffnessClass::Pinned) {
            oc = ComparisonOutcome::Meets;
            why = si + " ≤ 0,5 EIv/Lv: a ligação é rotulada, como requerido (" + limitsTxt + ").";
        } else {
            oc = ComparisonOutcome::DoesNotMeet;
            why = si + " > 0,5 EIv/Lv: a ligação NÃO é rotulada (é " + (by == JS::StiffnessClass::Rigid ? std::string("rígida pela rigidez") : std::string("semirrígida")) + "), mas o modelo a idealiza como rotulada (" + limitsTxt + ").";
        }
        break;
    case RequiredKind::Rigid:
        if (fin == JS::StiffnessClass::Rigid) {
            oc = ComparisonOutcome::Meets;
            why = si + " ≥ 25 EIv/Lv e Kv/Kp ≥ 0,1: a ligação é rígida, como requerido (" + limitsTxt + ").";
        } else if (cl.storeyPending) {
            oc = ComparisonOutcome::Indeterminate;
            why = si + " ≥ 25 EIv/Lv, mas Kv/Kp do andar não pôde ser verificado: a classe rígida (6.1.2.2) fica pendente.";
            out.gaps.push_back("Kv/Kp do andar indeterminado: a classe rígida não pode ser confirmada.");
        } else if (by == JS::StiffnessClass::Rigid) {
            oc = ComparisonOutcome::DoesNotMeet;
            why = si + " ≥ 25 EIv/Lv, porém Kv/Kp = " + num(item.kvKp, "%.4g") + " < 0,1: pela 6.1.2.2 a ligação é considerada SEMIRRÍGIDA; a idealização rígida não se sustenta.";
        } else {
            oc = ComparisonOutcome::DoesNotMeet;
            why = si + " < 25 EIv/Lv: a ligação é " + classText(fin) + ", mas o modelo a considera rígida (contínua) (" + limitsTxt + ").";
        }
        break;
    case RequiredKind::SemiRigid:
        if (by == JS::StiffnessClass::Pinned) {
            oc = ComparisonOutcome::DoesNotMeet;
            why = si + " ≤ 0,5 EIv/Lv: a ligação é rotulada, mas o modelo a idealiza como semirrígida (mola declarada) (" + limitsTxt + ").";
        } else if (by == JS::StiffnessClass::SemiRigid) {
            oc = ComparisonOutcome::Meets;
            why = si + " entre 0,5 e 25 EIv/Lv: a ligação é semirrígida, como requerido (" + limitsTxt + ").";
        } else if (cl.storeyPending) {
            oc = ComparisonOutcome::Indeterminate;
            why = si + " ≥ 25 EIv/Lv, mas Kv/Kp do andar não pôde ser verificado: não se sabe se a norma a trata como rígida ou semirrígida (6.1.2.2).";
            out.gaps.push_back("Kv/Kp do andar indeterminado: a classe (semirrígida × rígida) não pode ser confirmada.");
        } else if (fin == JS::StiffnessClass::SemiRigid) {
            oc = ComparisonOutcome::Meets;
            why = si + " ≥ 25 EIv/Lv com Kv/Kp = " + num(item.kvKp, "%.4g") + " < 0,1: pela 6.1.2.2 a ligação é considerada semirrígida, como requerido.";
        } else {
            oc = ComparisonOutcome::DoesNotMeet;
            why = si + " ≥ 25 EIv/Lv e Kv/Kp ≥ 0,1: a ligação é rígida, mas o modelo a idealiza como semirrígida (mola declarada).";
        }
        if (std::isfinite(out.springKnm) && out.springKnm > 0.0) {
            out.stiffnessRatioToSpring = out.real.stiffnessKnm / out.springKnm;
            out.caveats.push_back("Mola do modelo k = " + num(out.springKnm) + " kN·m/rad; Si/k = " + num(out.stiffnessRatioToSpring, "%.4g") + ". A comparação é de CLASSE: a NBR não fixa tolerância entre a rigidez adotada na análise e a da ligação real, e o programa não inventa uma. Se Si difere de k, atualize a mola do modelo (a análise global deve usar a rigidez da ligação real).");
        }
        break;
    case RequiredKind::Undetermined:
        break;
    }
    out.outcome = oc;
    out.reasons.push_back(why);
    out.summary = comparisonOutcomeName(oc) + ": requerido " + req + "; ligação real " + out.real.classLabel + ". " + why;

    // contexto que acompanha o resultado (sem alterar o veredito)
    if (item.basis == RequirementBasis::ModelImplicitContinuity) {
        out.caveats.push_back("O comportamento requerido (rígido) vem da CONTINUIDADE IMPLÍCITA do modelo, não de uma idealização declarada pelo projetista: o resultado compara a ligação com a hipótese padrão do modelo (6.1.2.3).");
    }
    if (item.systemConsistency == SystemConsistency::NeedsAttention) {
        out.caveats.push_back("Sistema/contraventamento declarados: " + item.systemNote);
    } else if (item.systemConsistency == SystemConsistency::Undetermined) {
        out.gaps.push_back("Compatibilidade do comportamento requerido com o sistema estrutural/contraventamento não avaliada (declaração ausente ou sistema não porticado).");
    }
    if (response->status == ValidityStatus::ValidWithCaveats) {
        out.caveats.push_back("A resposta mecânica da ligação (Incremento 3) é válida COM RESSALVAS: ver as limitações da aba de resposta mecânica.");
    }
    if (oc == ComparisonOutcome::DoesNotMeet) {
        out.caveats.push_back("Não atende: ajuste a ligação (tipologia/geometria) OU revise a idealização do modelo (liberação, mola ou continuidade). O programa não altera nenhuma das duas automaticamente.");
    }
    finish(out);
    return out;
}

std::string ConnectionRequirementComparison::memory() const
{
    std::ostringstream s;
    s << "COMPARAÇÃO DA LIGAÇÃO REAL COM O COMPORTAMENTO REQUERIDO — viga " << beamId << " / barra " << otherId;
    if (!directionLabel.empty()) {
        s << " — sentido " << directionLabel;
    }
    s << "\n";
    s << "  Resultado: " << comparisonOutcomeName(outcome) << " (" << statusLabelPt(status) << ")\n";
    s << "  Requerido: " << requiredText(required) << " (base: " << requirementBasisName(basis) << ")\n";
    if (real.available) {
        s << "  Rigidez Si (2/3 Mj,Rd) = " << num(real.stiffnessKnm) << " kN·m/rad; Sj,ini da resposta = " << num(real.initialStiffnessKnm) << " kN·m/rad; η = " << num(real.eta, "%.4g") << "\n";
        s << "  Limites (6.1.2.1): EIv/Lv = " << num(real.limits.referenceStiffness) << "; rotulada ≤ " << num(real.limits.sMin) << "; rígida ≥ " << num(real.limits.sMax) << " kN·m/rad; Kv/Kp = " << num(real.kvKp, "%.4g") << "\n";
        s << "  Classe pela rigidez: " << classText(real.byStiffness) << "; classe final: " << real.classLabel << "\n";
        s << "  " << real.justification << "\n";
    }
    s << "  Critérios:\n";
    for (const std::string &c : real.criteria) {
        s << "    - " << c << "\n";
    }
    s << "  Justificativas:\n";
    for (const std::string &r : reasons) {
        s << "    - " << r << "\n";
    }
    s << "  Hipóteses:\n";
    for (const std::string &h : hypotheses) {
        s << "    - " << h << "\n";
    }
    if (!caveats.empty()) {
        s << "  Ressalvas:\n";
        for (const std::string &c : caveats) {
            s << "    - " << c << "\n";
        }
    }
    if (!gaps.empty()) {
        s << "  Lacunas:\n";
        for (const std::string &g : gaps) {
            s << "    - " << g << "\n";
        }
    }
    return s.str();
}

DirectionalComparison combineDirections(const ConnectionRequirementComparison &positive, const ConnectionRequirementComparison &negative)
{
    DirectionalComparison d;
    d.positive = positive;
    d.negative = negative;
    const ComparisonOutcome p = positive.outcome;
    const ComparisonOutcome n = negative.outcome;
    d.rule = "Regra de combinação (decisão CONSERVADORA do programa; a NBR 8800:2024, 6.1.2 não define como reunir classes direcionais): Atende somente se os dois sentidos atendem; "
             "Não atende se algum sentido não atende; Indeterminado se nenhum não atende mas algum sentido é indeterminado ou está indisponível; Não aplicável se ambos o são. Não há média de rigidezes nem de resistências.";
    if (p == ComparisonOutcome::DoesNotMeet || n == ComparisonOutcome::DoesNotMeet) {
        d.overall = ComparisonOutcome::DoesNotMeet;
        d.summary = std::string("Não atende: ") + (p == ComparisonOutcome::DoesNotMeet && n == ComparisonOutcome::DoesNotMeet ? "nenhum dos dois sentidos atende ao requerido."
                                                   : p == ComparisonOutcome::DoesNotMeet ? "o sentido positivo (mesa superior tracionada) não atende; sentido negativo: " + comparisonOutcomeName(n) + "."
                                                                                         : "o sentido negativo (mesa inferior tracionada) não atende; sentido positivo: " + comparisonOutcomeName(p) + ".");
    } else if (p == ComparisonOutcome::NotApplicable && n == ComparisonOutcome::NotApplicable) {
        d.overall = ComparisonOutcome::NotApplicable;
        d.summary = "Não aplicável: o critério de rigidez de 6.1.2 não se aplica a este par em nenhum sentido.";
    } else if (p == ComparisonOutcome::Meets && n == ComparisonOutcome::Meets) {
        d.overall = ComparisonOutcome::Meets;
        d.summary = "Atende: os dois sentidos do momento atendem ao comportamento requerido.";
    } else {
        d.overall = ComparisonOutcome::Indeterminate;
        d.summary = "Indeterminado: positivo = " + comparisonOutcomeName(p) + "; negativo = " + comparisonOutcomeName(n) + ". Não se declara atendimento global com um sentido indeterminado ou indisponível.";
    }
    const bool caveat = d.overall == ComparisonOutcome::Meets && (!positive.caveats.empty() || !negative.caveats.empty() || !positive.gaps.empty() || !negative.gaps.empty());
    switch (d.overall) {
    case ComparisonOutcome::NotApplicable: d.status = ValidityStatus::OutOfDomain; break;
    case ComparisonOutcome::Indeterminate: d.status = ValidityStatus::InsufficientData; break;
    default: d.status = d.overall == ComparisonOutcome::Meets && !caveat ? ValidityStatus::Valid : ValidityStatus::ValidWithCaveats; break;
    }
    return d;
}

std::string DirectionalComparison::memory() const
{
    std::ostringstream s;
    s << "RESULTADO GLOBAL (dois sentidos): " << comparisonOutcomeName(overall) << "\n  " << summary << "\n  " << rule << "\n\n";
    s << positive.memory() << "\n" << negative.memory();
    return s.str();
}

} // namespace Structura::Design::JointEngine
