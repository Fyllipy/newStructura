#include "design/jointengine/ResistanceWrappers.h"

#include <algorithm>
#include <cstdlib>

namespace Structura::Design::JointEngine {

namespace C = Structura::Design::Connections;

std::string provenanceName(FormulationProvenance p)
{
    switch (p) {
    case FormulationProvenance::ReproducedFromCode: return "ReproducedFromCode";
    case FormulationProvenance::PartiallyDocumented: return "PartiallyDocumented";
    case FormulationProvenance::ConfrontedWithStandard: return "ConfrontedWithStandard";
    case FormulationProvenance::DomainVerified: return "DomainVerified";
    case FormulationProvenance::Unavailable: return "Unavailable";
    }
    return "Unknown";
}

std::string basisName(StandardBasis b)
{
    switch (b) {
    case StandardBasis::Nbr8800_2024: return "Nbr8800_2024";
    case StandardBasis::ComplementaryAisc: return "ComplementaryAisc";
    case StandardBasis::ComplementaryCidect: return "ComplementaryCidect";
    case StandardBasis::EnIndependent: return "EnIndependent";
    case StandardBasis::Unclassified: return "Unclassified";
    }
    return "Unknown";
}

std::string demandOriginName(DemandOrigin d)
{
    switch (d) {
    case DemandOrigin::ProvidedInput: return "ProvidedInput";
    case DemandOrigin::IdealizedByLegacy: return "IdealizedByLegacy";
    case DemandOrigin::DerivedAndConfronted: return "DerivedAndConfronted";
    }
    return "Unknown";
}

namespace {

constexpr const char *kNg04 = "Ng, Deng, Grondin & Driver (2004), AISC Engineering Journal, 2o trim., pp. 39-42 (lido)";
constexpr const char *kNbr24 = "ABNT NBR 8800:2024: Tabela 3 (gamma), Tabela 9, 6.2.5.2(a), 6.2.6.2.1-2 (Tabela 11), 6.5.5 (lidos no PDF fornecido pelo usuario)";
constexpr const char *kDg29 = "AISC Design Guide 29: exemplos de flambagem da secao de Whitmore com K = 0,50, 0,65 e 1,2 (PDF paginas 59, 204, 111/189; fornecido pelo usuario)";
constexpr const char *kCidect1 = "CIDECT Design Guide 1, 2a ed. 2008: Tabela 4.1 e 4.2 (copia fornecida pelo usuario, pag. 32-33 do guia)";
constexpr const char *kTh84 = "Thornton (1984), AISC Engineering Journal 21(3), pp. 139-148 (lido integralmente)";
constexpr const char *kCb07 = "Chambers & Bartley (2007), AISC Engineering Journal, 3o trim., pp. 255-258 (lido)";

bool contains(const std::string &s, const char *part) { return s.find(part) != std::string::npos; }
bool endsWith(const std::string &s, const std::string &suffix) { return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0; }

WrappedCheck wrap(const C::Check &c, FormulationProvenance p, std::string note)
{
    WrappedCheck w;
    w.name = c.name;
    w.clause = c.clause;
    w.unit = c.unit;
    w.demand = c.demand;
    w.resistance = c.resistance;
    w.ratio = c.ratio();
    w.status = c.status;
    w.reason = c.reason;
    w.provenance = p;
    w.provenanceNote = std::move(note);
    w.lines = c.lines;
    return w;
}

void finalize(WrappedCheck &w, StandardBasis basis, DemandOrigin demand, const char *demandNote, bool conditionsVerified, const char *conditionsNote)
{
    w.basis = basis;
    w.demandOrigin = demand;
    w.demandNote = demandNote;
    w.conditionsVerified = conditionsVerified;
    w.conditionsNote = conditionsNote;
    const bool resistanceOk = approvalEligible(w.provenance);
    const bool demandOk = demand != DemandOrigin::IdealizedByLegacy;
    w.confrontedWithOwnSource = resistanceOk && demandOk && conditionsVerified;
    w.approvalEligible = basis == StandardBasis::Nbr8800_2024 && w.confrontedWithOwnSource;
    std::string why;
    if (!resistanceOk) why += "formulacao de resistencia nao confrontada com a fonte; ";
    if (!demandOk) why += "demanda idealizada pelo legado nao confrontada; ";
    if (!conditionsVerified) why += "condicoes de aplicacao/entradas obrigatorias nao verificadas; ";
    if (basis != StandardBasis::Nbr8800_2024) why += "base nao e a NBR 8800:2024 (metodo complementar/nao classificado); ";
    w.eligibilityReason = w.approvalEligible ? "elegivel: resistencia, demanda e condicoes confrontadas com a NBR 8800:2024" : why;
}

ModelDescriptor base(const char *id, const char *version, const char *formulation, std::vector<std::string> refs, std::vector<std::string> hyp, std::vector<std::string> lim)
{
    ModelDescriptor d;
    d.id = id;
    d.version = version;
    d.formulation = formulation;
    d.references = std::move(refs);
    d.supportedDofs.dofs = {{"F", DofKind::Force, "-", "N", "mm"}};   // so verificacao de resistencia: sem GDL deformaveis
    d.hypotheses = std::move(hyp);
    d.limitations = std::move(lim);
    d.resultKind = ResultKind::NormativeResistance;
    return d;
}

const char *kNoText = "texto da norma NAO conferido nesta fase; reproduzida do codigo legado";

} // namespace

// ------------------------------------------------------------------------------------------------------------------------ solda

const ModelDescriptor &WeldSupportResistance::descriptor()
{
    static const ModelDescriptor d = base(
        kId, kVersion, "Verificacoes de resistencia da solda viga-apoio (mesas e alma) delegadas a SupportWeld (NBR 8800:2024, 6.2 conforme o legado)",
        {"src/design/SupportWeld.{h,cpp} (legado, nao alterado)", kNg04},
        {"momento e axial resistidos pelas mesas; cortante pelos filetes da alma (modelo do legado)", "fator de angulo 1,0 + 0,5 sen^1,5 theta"},
        {"somente resistencia: sem rigidez, sem lei F-delta/M-theta, sem ICR", "so dois arranjos (mesas e alma); sem grupo generico, penetracao parcial, fora do plano",
         "NBR 8800:2024 conferida so em Tabela 3, Tabela 9, 6.2.5.2(a), 6.2.6.2 e 6.5.5; 6.2.5.2(b)(c) (ICR e combinacao) nao implementados no legado; EN 1993-1-8 nao implementada no legado"});
    return d;
}

ResistanceReport WeldSupportResistance::evaluate(const C::SupportWeldInput &input)
{
    ResistanceReport r;
    r.modelId = kId;
    r.modelVersion = kVersion;
    r.hypotheses = descriptor().hypotheses;
    r.limitations = descriptor().limitations;
    r.sourcesRead = {kNg04, kNbr24};
    for (const C::Check &c : C::checkSupportWeld(input)) {
        const bool leg = contains(c.name, "perna minima") || contains(c.name, "perna maxima");
        const bool length = c.clause == "6.2.6.2.3";
        if (length) {
            r.checks.push_back(wrap(c, FormulationProvenance::ConfrontedWithStandard,
                                    "NBR 8800:2024, 6.2.6.2.3 conferido integralmente: comprimento efetivo >= 4 x perna e >= 40 mm, ou perna considerada no calculo <= 25 % do comprimento; a regra dos filetes exclusivamente longitudinais de ligacoes extremas de chapas tracionadas NAO se aplica a solda viga-apoio; interpretacao adotada: o limite de 40 mm nao tem alternativa de calculo"));
        } else if (leg) {
            r.checks.push_back(wrap(c, FormulationProvenance::ConfrontedWithStandard,
                                    "Tabela 11 (3/5/6/8 mm; limite 6,3 mm, corrigido no S-03.1) e perna maxima (6.2.6.2.2: t se t < 6,3 mm; t - 1,5 mm se t >= 6,3 mm) conferidas com a NBR 8800:2024; nota a da Tabela 11 (filete executado com um unico passe) nao e verificavel: passes nao informados"));
        } else if (contains(c.name, "penetracao total")) {
            r.checks.push_back(wrap(c, FormulationProvenance::ConfrontedWithStandard,
                                    "resistencia = metal-base AMB fy/gamma_a1 (NBR 8800:2024, Tabela 9, penetracao total, tracao normal; gamma_a1 = 1,10, Tabela 3) conferida; a demanda F = M/(d - tf) + N/2 e idealizacao do legado, NAO da norma"));
        } else if (contains(c.name, "(filete)") || contains(c.name, "alma (filetes)")) {
            r.checks.push_back(wrap(c, FormulationProvenance::ConfrontedWithStandard,
                                    "metal da solda 0,60 Aw fw (1 + 0,5 sen^1,5 theta)/gamma_w2 (Tabela 9 e 6.2.5.2(a)) e metal-base 0,60 t fy/gamma_a1 e 0,60 Anv fu/gamma_a2 (6.5.5), gamma_w2 = 1,35 e gamma_a = 1,10/1,35 (combinacoes normais, Tabelas 3 e 9) conferidos; theta = 90 (mesas) / 0 (alma) e a demanda (distribuicao de esforcos entre mesas e alma) sao idealizacoes do legado NAO conferidas; combinacoes excepcionais (gamma 1,15/1,00) nao tratadas; perna efetiva (6.2.6.2.3) aplicada quando acionada"));
        } else if (contains(c.name, "metal-base")) {
            r.checks.push_back(wrap(c, FormulationProvenance::ConfrontedWithStandard,
                                    "0,60 t fy/gamma_a1 e 0,60 t fu/gamma_a2 conferidos com 6.5.5 (a Tabela 9 manda o metal-base atender 6.5); demanda 2 fv e idealizacao do legado"));
        } else {
            r.checks.push_back(wrap(c, FormulationProvenance::ReproducedFromCode, kNoText));
        }
    }
    for (WrappedCheck &w : r.checks) {
        const bool leg = contains(w.name, "perna minima") || contains(w.name, "perna maxima");
        const bool length = w.clause == "6.2.6.2.3";
        const DemandOrigin demand = leg ? DemandOrigin::ProvidedInput : DemandOrigin::IdealizedByLegacy;
        const char *demandNote = leg ? "a demanda e a perna adotada (dado de entrada)"
                                 : length ? "comprimentos dos trechos assumem filetes de perna plena em toda a face (sem retornos, inicio/termino ou raio de concordancia): idealizacao do legado (auditoria S-02.4)"
                                          : "F = M/(d - tf) + N/2 nas mesas e V uniforme nos filetes da alma: idealizacao do legado, NAO confrontada com a NBR (auditoria S-02.4)";
        finalize(w, StandardBasis::Nbr8800_2024, demand, demandNote, false,
                 "nao verificados pelo legado: compatibilidade do metal da solda (Tabelas 8 e 9), passes do filete (nota a da Tabela 11), interacao cisalhamento-normal nos filetes das mesas");
    }
    // perna geometrica informada x perna efetiva considerada no calculo (6.2.6.2.3), sempre registradas
    const C::SupportWeldRuns runs = C::supportWeldRuns(input);
    const auto record = [&](const char *family, const std::vector<C::WeldRun> &v) {
        if (v.empty()) {
            return;
        }
        double minEff = 1e300, minLength = 1e300;
        for (const C::WeldRun &w : v) {
            minEff = std::min(minEff, w.effectiveLeg);
            minLength = std::min(minLength, w.length);
        }
        const std::string f = family;
        r.parameters["geometricLeg." + f] = Quantity::make(v.front().geometricLeg, "mm", Origin::UserProvided, "SupportWeldInput (perna informada)");
        r.parameters["effectiveLeg." + f] = Quantity::make(minEff, "mm", Origin::Derived, "supportWeldRuns: min(perna, comprimento do trecho/4), NBR 8800:2024 6.2.6.2.3");
        r.parameters["shortestRun." + f] = Quantity::make(minLength, "mm", Origin::Derived, "supportWeldRuns: trecho continuo mais curto");
    };
    record("flange", runs.flange);
    record("web", runs.web);
    return r;
}

// ------------------------------------------------------------------------------------------------------------------------ gusset

const ModelDescriptor &GussetResistance::descriptor()
{
    static const ModelDescriptor d = base(
        kId, kVersion, "Verificacoes de resistencia da ligacao de diagonal a chapa de no delegadas a checkGusset (parafusos, diagonal, secao de Whitmore, block shear, solda ao apoio)",
        {"src/design/TrussConnections.{h,cpp} (legado, nao alterado)", kCb07},
        {"largura de Whitmore com 30 graus, limitada a largura da chapa (legado)", "compressao: coluna de Whitmore com K e comprimento do legado (preservados)"},
        {"somente resistencia: sem distribuicao entre barras, sem rigidez, sem flambagem fora do plano", "K = 0,65 sem fundamentacao por fonte lida (faixa 0,5-1,2 em Chambers & Bartley 2007)",
         "contorno da chapa nao armazenado: so gussetWidth e freeLength", "largura ponderada quando a secao entra no apoio (Thornton & Kane via Chambers & Bartley) nao implementada no legado"});
    return d;
}

ResistanceReport GussetResistance::evaluate(const C::GussetInput &in)
{
    ResistanceReport r;
    r.modelId = kId;
    r.modelVersion = kVersion;
    r.hypotheses = descriptor().hypotheses;
    r.limitations = descriptor().limitations;
    r.sourcesRead = {kCb07, kTh84, kDg29};
    const std::vector<C::Check> checks = C::checkGusset(in);
    for (const C::Check &c : checks) {
        if (contains(c.name, "tracao na secao de Whitmore")) {
            r.checks.push_back(wrap(c, FormulationProvenance::PartiallyDocumented,
                                    "largura de Whitmore (2 L tan30 + s) confrontada com Chambers & Bartley (2007), Tabela 1 e Fig. 1; a limitacao pela largura da chapa e as resistencias NBR NAO foram conferidas"));
        } else if (contains(c.name, "compressao na secao de Whitmore")) {
            r.checks.push_back(wrap(c, FormulationProvenance::PartiallyDocumented,
                                    "metodo da pseudo-coluna: K = 0,65 tem origem em Thornton (1984), p. 147 (coluna biengastada) e em Dowswell (via AISC Design Guide 29, exemplo com K = 0,65); o Design Guide 29 usa K = 0,50 (chapa apoiada nas duas bordas, Gross 1990) e K = 1,2 quando ha possibilidade de deslocamento lateral, e Chambers & Bartley (2007), p. 255, relata K de 0,5 a 1,2: K DEPENDE das condicoes de vinculo da chapa, que o legado nao representa; o comprimento e informado pelo usuario (freeLength) e Thornton (p. 147) adverte que l1 pode ser contra a seguranca; a NBR 8800:2024 nao traz o metodo de Whitmore (busca textual sem ocorrencia) e o AISC 360 J4.4 nao foi lido; a combinacao com a curva de coluna da NBR NAO foi conferida"));
            // K registrado a partir da memoria do PROPRIO legado (sem duplicar o literal)
            for (const std::string &line : c.lines) {
                const std::size_t pos = line.find("K = ");
                if (pos == 0) {
                    r.parameters["K"] = Quantity::make(std::strtod(line.c_str() + 4, nullptr), "-", Origin::ModelAssumption,
                                                       "valor fixo do legado (TrussConnections.cpp); NAO ajustado; origem: Thornton (1984) p. 147 (biengastada) e Dowswell via DG 29; DG 29 usa 0,50 (duas bordas apoiadas) e 1,2 (deslocamento lateral); faixa 0,5-1,2 (Chambers & Bartley 2007) nao determina valor de projeto");
                    break;
                }
            }
        } else {
            r.checks.push_back(wrap(c, FormulationProvenance::ReproducedFromCode, kNoText));
        }
    }
    r.parameters["bucklingLength"] = Quantity::make(in.freeLength, "mm", Origin::UserProvided, "GussetInput.freeLength (usado como comprimento da coluna de Whitmore pelo legado)");
    r.parameters["widthLimit"] = Quantity::make(in.gussetWidth, "mm", Origin::UserProvided, "GussetInput.gussetWidth (o legado limita a largura de Whitmore a este valor)");
    r.parameters["whitmoreWidth"] = Quantity::make(C::whitmoreWidth(in), "mm", Origin::Derived, "TrussConnections::whitmoreWidth (sem limitar pela largura da chapa)");
    for (WrappedCheck &w : r.checks) {
        const bool whitmore = contains(w.name, "Whitmore");
        finalize(w, whitmore ? StandardBasis::ComplementaryAisc : StandardBasis::Nbr8800_2024, DemandOrigin::ProvidedInput,
                 "esforcos de tracao/compressao fornecidos como entrada; sem distribuicao entre barras",
                 false, whitmore ? "metodo de Whitmore/pseudo-coluna nao consta da NBR 8800:2024; K, comprimento e largura dependem do vinculo e do contorno da chapa, nao representados"
                                 : "entradas e condicoes de aplicacao nao verificadas pelo wrapper; referencia NBR nao conferida item a item");
    }
    return r;
}

// ------------------------------------------------------------------------------------------------------------------------ tubular

const ModelDescriptor &TubularResistance::descriptor()
{
    static const ModelDescriptor d = base(
        kId, kVersion, "Verificacoes de resistencia de ligacoes soldadas de trelicas tubulares (CIDECT DG1, 2a ed., 2008, conforme o legado) com equacao ou limite numerado e inequivoco",
        {"src/design/TubularJoints.{h,cpp} (legado, nao alterado)", "CIDECT Design Guide 1, 2a ed. (2008): copia fornecida pelo usuario; Tabelas 4.1 e 4.2 lidas"},
        {"banzo circular (CHS); resistencias de calculo N* do proprio guia (conforme o legado)"},
        {"somente resistencia: sem rigidez de no, sem lei F-delta", "so as Tabelas 4.1 e 4.2 do CIDECT DG1 foram conferidas (CHS); demais tabelas e EN 1993-1-8 secao 7 NAO conferidas",
         "verificacoes sem equacao numerada unica ficam excluidas do wrapper (ver `excluded`)"});
    return d;
}

ResistanceReport TubularResistance::evaluate(const C::TubularJointInput &input)
{
    ResistanceReport r;
    r.modelId = kId;
    r.modelVersion = kVersion;
    r.hypotheses = descriptor().hypotheses;
    r.limitations = descriptor().limitations;
    r.sourcesRead = {kCidect1};
    const bool chs = input.braceKind == C::TubularJointInput::Circular;
    for (const C::Check &c : C::checkTubularJoint(input)) {
        const bool numberedEquation = c.clause.rfind("CIDECT eq. ", 0) == 0;
        const bool validity = endsWith(c.clause, "(validade)") || c.clause == "CIDECT 1.2.1";
        if (!(numberedEquation || validity)) {
            r.excluded.push_back(c.name + " [" + c.clause + "]: referencia nao inequivoca (sem equacao/limite CIDECT numerado unico)");
            continue;
        }
        // conferidos com a Tabela 4.1 do DG1 (2008): eq. 4.1 (com Qu 4.4-4.6 e Qf 4.7), eq. 4.2 (punçao) e faixa de validade 4.1 (CHS)
        const bool tab41 = chs && (c.clause == "CIDECT eq. 4.1" || c.clause == "CIDECT eq. 4.2" || c.clause == "CIDECT 4.1 (validade)");
        if (tab41 && !(input.fy0 > 355.0)) {
            r.checks.push_back(wrap(c, FormulationProvenance::ConfrontedWithStandard,
                                    "conferido com CIDECT DG1 (2008), Tabela 4.1 e 4.2 (eq. 4.1-4.7 e faixa de validade); a copia do guia foi fornecida pelo usuario; o EN 1993-1-8 secao 7 NAO foi comparado (base IIW diferente)"));
        } else if (tab41) {
            r.checks.push_back(wrap(c, FormulationProvenance::PartiallyDocumented,
                                    "equacao conferida com a Tabela 4.1, mas fy0 > 355 MPa aciona a nota (**) (secao 1.2.1, fator de reducao 0,9 do legado) NAO lida"));
        } else {
            r.checks.push_back(wrap(c, FormulationProvenance::ReproducedFromCode,
                                    chs ? "tabela/equacao do CIDECT DG1 nao conferida nesta fase (so Tabela 4.1 e 4.2 foram lidas)" : "ligacao com chapa transversal (equacoes 7.x): tabela 7.1 NAO lida"));
        }
    }
    bool allValid = true;
    for (const WrappedCheck &w : r.checks) {
        if (w.clause.find("(validade)") != std::string::npos && !(w.status == C::CheckStatus::Evaluated && w.ratio <= 1.0)) {
            allValid = false;
        }
    }
    for (WrappedCheck &w : r.checks) {
        finalize(w, StandardBasis::ComplementaryCidect, DemandOrigin::ProvidedInput, "forcas axiais das diagonais e do banzo fornecidas como entrada",
                 allValid, allValid ? "faixa de validade da Tabela 4.1 satisfeita nas verificacoes de validade incluidas" : "faixa de validade da Tabela 4.1 NAO satisfeita: resistencia fora do dominio");
    }
    return r;
}

} // namespace Structura::Design::JointEngine
