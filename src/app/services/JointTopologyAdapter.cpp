#include "app/services/JointTopologyAdapter.h"

#include "app/services/ConnectionCatalog.h"
#include "app/services/JointComponentService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace Structura::App::Services {

namespace JE = Structura::Design::JointEngine;
using NC = Structura::App::ProjectData::NodeConnection;

namespace {

struct Builder {
    JE::JointTopology t;
    int nextPart {1};
    int nextLink {1};

    int part(JE::PartKind kind, const std::string &role, int memberId = 0, const std::string &note = {})
    {
        JE::PartRef p;
        p.id = nextPart++;
        p.kind = kind;
        p.role = role;
        p.memberId = memberId;
        p.note = note;
        t.parts.push_back(p);
        return p.id;
    }
    int concretePart(JE::PartKind kind, const std::string &role)
    {
        const int id = part(kind, role);
        JE::MaterialSpec m;   // vazio de proposito: nenhuma propriedade presumida (D3)
        m.kind = kind == JE::PartKind::Grout ? JE::MaterialKind::Grout : JE::MaterialKind::Concrete;
        t.parts.back().material = m;
        return id;
    }
    void link(JE::LinkKind kind, std::vector<int> ids, JE::Link::Resolution res, const std::string &note = {})
    {
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        if (ids.size() < 2) {   // uniao de uma parte so nao e representavel: vira lacuna, nao parte inventada
            gap("LINK_NOT_REPRESENTABLE", "uniao sem partes suficientes: " + note);
            return;
        }
        JE::Link l;
        l.id = nextLink++;
        l.kind = kind;
        l.partIds = std::move(ids);
        l.resolution = res;
        l.note = note;
        t.links.push_back(l);
    }
    void gap(const std::string &code, const std::string &description) { t.gaps.push_back({code, description}); }
    std::vector<int> allPartIds() const
    {
        std::vector<int> ids;
        for (const JE::PartRef &p : t.parts) ids.push_back(p.id);
        return ids;
    }
};

enum class Rule {
    ShearTab, DoubleAngle, Gusset, Splice, BasePlate, Tubular, ColdFormed, EndPlate,   // tipos legados
    WebShear, Moment, CoverSplice, TubeSplice, EndPlateSplice, TrussNode, FlexEndPlate, Composite, Pin, Unknown
};

Rule ruleFor(const NC &c, const ConnectionCatalog::Entry **entryOut)
{
    const ConnectionCatalog::Entry *e = nullptr;
    int type = c.type;
    if (ConnectionCatalog::isCatalog(c.type)) {
        e = ConnectionCatalog::find(c.type);
        if (!e) {
            return Rule::Unknown;
        }
        if (e->legacyType >= 0) {
            type = e->legacyType;
        } else {
            *entryOut = e;
            switch (e->family) {
            case ConnectionCatalog::Family::WebShear: return Rule::WebShear;
            case ConnectionCatalog::Family::Moment: return Rule::Moment;
            case ConnectionCatalog::Family::CoverSplice: return Rule::CoverSplice;
            case ConnectionCatalog::Family::TubeSplice: return Rule::TubeSplice;
            case ConnectionCatalog::Family::EndPlateSplice: return Rule::EndPlateSplice;
            case ConnectionCatalog::Family::TrussNode: return Rule::TrussNode;
            case ConnectionCatalog::Family::FlexEndPlate: return Rule::FlexEndPlate;
            case ConnectionCatalog::Family::Composite: return Rule::Composite;
            case ConnectionCatalog::Family::Pin: return Rule::Pin;
            }
            return Rule::Unknown;
        }
        *entryOut = e;
    }
    switch (type) {
    case NC::kShearTab: return Rule::ShearTab;
    case NC::kDoubleAngle: return Rule::DoubleAngle;
    case NC::kGusset: return Rule::Gusset;
    case NC::kSplice: return Rule::Splice;
    case NC::kBasePlate: return Rule::BasePlate;
    case NC::kTubular: return Rule::Tubular;
    case NC::kColdFormed: return Rule::ColdFormed;
    case NC::kEndPlate: return Rule::EndPlate;
    }
    return Rule::Unknown;
}

std::string ruleName(Rule r)
{
    switch (r) {
    case Rule::ShearTab: return "legacy:kShearTab";
    case Rule::DoubleAngle: return "legacy:kDoubleAngle";
    case Rule::Gusset: return "legacy:kGusset";
    case Rule::Splice: return "legacy:kSplice";
    case Rule::BasePlate: return "legacy:kBasePlate";
    case Rule::Tubular: return "legacy:kTubular";
    case Rule::ColdFormed: return "legacy:kColdFormed";
    case Rule::EndPlate: return "legacy:kEndPlate";
    case Rule::WebShear: return "WebShear";
    case Rule::Moment: return "Moment";
    case Rule::CoverSplice: return "CoverSplice";
    case Rule::TubeSplice: return "TubeSplice";
    case Rule::EndPlateSplice: return "EndPlateSplice";
    case Rule::TrussNode: return "TrussNode";
    case Rule::FlexEndPlate: return "FlexEndPlate";
    case Rule::Composite: return "Composite";
    case Rule::Pin: return "Pin";
    case Rule::Unknown: return "unknown";
    }
    return "unknown";
}

double presetValue(const ConnectionCatalog::Entry *e, const char *key, double fallbackNaN)
{
    if (!e) {
        return fallbackNaN;
    }
    const auto it = e->preset.constFind(QString::fromLatin1(key));
    return it == e->preset.constEnd() ? fallbackNaN : it.value();
}

} // namespace

std::vector<int> JointTopologyAdapter::memberIdsAtNode(const Structura::App::SolverBridge::ModelData &model, int solverNodeId)
{
    std::vector<int> ids;
    for (const auto &m : model.members) {
        if (m.nodeI == solverNodeId || m.nodeJ == solverNodeId) {
            ids.push_back(m.id);
        }
    }
    return ids;
}

JE::JointTopology JointTopologyAdapter::derive(const NC &connection, const std::vector<int> &memberIdsAtNode)
{
    const ConnectionCatalog::Entry *entry = nullptr;
    const Rule rule = ruleFor(connection, &entry);
    Builder b;
    b.t.configId = connection.id;
    b.t.familyId = ruleName(rule);
    if (rule == Rule::Unknown) {
        b.t.status = JE::TopologyStatus::Unavailable;
        b.t.unavailableReason = "tipologia desconhecida (tipo " + std::to_string(connection.type) + "): sem regra de derivacao de topologia";
        return b.t;
    }

    // barra da ligacao (sempre parte); demais barras do no entram SEM papel presumido
    const bool mainIsChord = rule == Rule::TrussNode || rule == Rule::Tubular;
    const int beamPart = b.part(JE::PartKind::Member, mainIsChord ? "main" : "beam", connection.memberId);
    std::vector<int> others;
    for (const int id : memberIdsAtNode) {
        if (id != connection.memberId && std::find(others.begin(), others.end(), id) == others.end()) {
            others.push_back(id);
        }
    }
    std::vector<int> otherParts;
    for (const int id : others) {
        otherParts.push_back(b.part(JE::PartKind::Member, "unclassified", id, "barra que chega ao no; papel nao armazenado na ligacao"));
    }
    if (!others.empty()) {
        b.gap("MEMBER_ROLES_NOT_STORED", "papeis (apoio, diagonal, montante...) das demais barras do no nao estao armazenados na ligacao legada");
    }

    const bool usesSupport = entry ? entry->usesSupport : (rule == Rule::ShearTab || rule == Rule::DoubleAngle || rule == Rule::Gusset ||
                                                           rule == Rule::BasePlate || rule == Rule::EndPlate);
    const bool usesBolts = entry ? entry->usesBolts : (rule != Rule::Tubular && rule != Rule::BasePlate);
    if (usesSupport && others.empty() && rule != Rule::BasePlate) {
        const int sup = b.part(JE::PartKind::Member, "support", 0, "barra de apoio nao identificada no modelo");
        otherParts.push_back(sup);
        b.gap("SUPPORT_MEMBER_UNKNOWN", "a ligacao usa apoio, mas nenhuma outra barra chega ao no informado");
    }

    std::vector<int> connectionParts;   // elementos da ligacao (alem das barras)
    bool weldedKnown = false;
    bool welded = false;
    bool boltedKnown = true;
    bool bolted = usesBolts;
    JE::LinkKind extraKind = JE::LinkKind::Bolted;   // so para Pin
    bool pin = false;

    switch (rule) {
    case Rule::ShearTab:
        connectionParts.push_back(b.part(JE::PartKind::Plate, "shear tab"));
        break;
    case Rule::DoubleAngle:
        connectionParts.push_back(b.part(JE::PartKind::Angle, "web angle"));
        connectionParts.push_back(b.part(JE::PartKind::Angle, "web angle"));
        break;
    case Rule::Gusset:
        connectionParts.push_back(b.part(JE::PartKind::Gusset, "gusset"));
        b.gap("WELD_PRESENCE_NOT_DERIVED", "presenca e posicao de soldas da chapa de no nao derivadas do tipo");
        break;
    case Rule::Splice:
        connectionParts.push_back(b.part(JE::PartKind::Plate, "splice plates"));
        b.gap("SPLICE_PARTS_COUNT_NOT_DERIVED", "quantidade de talas de mesa/alma nao derivada do tipo");
        if (others.empty()) {
            otherParts.push_back(b.part(JE::PartKind::Member, "spliced member", 0, "segunda barra da emenda nao identificada"));
            b.gap("SPLICED_MEMBER_UNKNOWN", "a segunda barra da emenda nao esta identificada no modelo");
        }
        break;
    case Rule::BasePlate: {
        connectionParts.push_back(b.part(JE::PartKind::BasePlate, "base plate"));
        connectionParts.push_back(b.part(JE::PartKind::Anchor, "anchor group"));
        connectionParts.push_back(b.concretePart(JE::PartKind::Concrete, "support block"));
        b.gap("CONCRETE_PROPERTIES_NOT_AVAILABLE", "propriedades do concreto nao estao no modelo de dados (MaterialSpec vazio)");
        b.gap("GROUT_PRESENCE_NOT_STORED", "presenca e propriedades do graute nao estao armazenadas");
        bolted = false;
        boltedKnown = true;
        break;
    }
    case Rule::Tubular:
        weldedKnown = true;
        welded = true;
        bolted = false;
        break;
    case Rule::ColdFormed:
        connectionParts.push_back(b.part(JE::PartKind::Auxiliary, "cold-formed connection elements"));
        b.gap("COLD_FORMED_PARTS_NOT_DERIVED", "componentes da ligacao de perfil formado a frio nao derivados do tipo");
        boltedKnown = false;
        bolted = false;
        break;
    case Rule::EndPlate:
        connectionParts.push_back(b.part(JE::PartKind::Plate, "end plate"));
        break;
    case Rule::WebShear: {
        const double plates = presetValue(entry, "plates", std::numeric_limits<double>::quiet_NaN());
        const double webWelded = presetValue(entry, "webWelded", std::numeric_limits<double>::quiet_NaN());
        if (std::isfinite(plates)) {
            for (int i = 0; i < static_cast<int>(plates); ++i) {
                connectionParts.push_back(b.part(JE::PartKind::Plate, "web plate"));
            }
        } else {
            connectionParts.push_back(b.part(JE::PartKind::Plate, "web plate"));
            b.gap("PLATE_COUNT_UNKNOWN", "numero de talas nao informado no preset");
        }
        if (std::isfinite(webWelded)) {
            weldedKnown = true;
            welded = webWelded != 0.0;
        }
        break;
    }
    case Rule::Moment:
        connectionParts.push_back(b.part(JE::PartKind::Auxiliary, "moment connection elements"));
        b.gap("MOMENT_ELEMENTS_NOT_ENUMERATED", "talas/T-stubs/chapas da ligacao de momento nao enumerados pelo catalogo");
        break;
    case Rule::CoverSplice:
        connectionParts.push_back(b.part(JE::PartKind::Plate, "cover plates"));
        b.gap("COVER_PLATE_COUNT_NOT_DERIVED", "numero de cobrejuntas nao derivado");
        if (others.empty()) {
            otherParts.push_back(b.part(JE::PartKind::Member, "spliced member", 0, "segunda barra da emenda nao identificada"));
            b.gap("SPLICED_MEMBER_UNKNOWN", "a segunda barra da emenda nao esta identificada no modelo");
        }
        break;
    case Rule::TubeSplice:
        connectionParts.push_back(b.part(JE::PartKind::Plate, "end plate"));
        connectionParts.push_back(b.part(JE::PartKind::Plate, "end plate"));
        if (others.empty()) {
            otherParts.push_back(b.part(JE::PartKind::Tube, "spliced member", 0, "segundo tubo nao identificado"));
            b.gap("SPLICED_MEMBER_UNKNOWN", "o segundo tubo da emenda nao esta identificado no modelo");
        }
        break;
    case Rule::EndPlateSplice:
        connectionParts.push_back(b.part(JE::PartKind::Plate, "end plate"));
        connectionParts.push_back(b.part(JE::PartKind::Plate, "end plate"));
        if (others.empty()) {
            otherParts.push_back(b.part(JE::PartKind::Member, "spliced member", 0, "segunda viga nao identificada"));
            b.gap("SPLICED_MEMBER_UNKNOWN", "a segunda viga da emenda nao esta identificada no modelo");
        }
        break;
    case Rule::TrussNode:
        b.gap("GUSSET_PRESENCE_NOT_DERIVED", "presenca de chapa de no nao derivada do preset");
        b.gap("WELD_PRESENCE_NOT_DERIVED", "presenca de soldas nao derivada do preset");
        break;
    case Rule::FlexEndPlate:
        connectionParts.push_back(b.part(JE::PartKind::Plate, "flexible end plate"));
        break;
    case Rule::Composite:
        connectionParts.push_back(b.concretePart(JE::PartKind::Concrete, "slab"));
        connectionParts.push_back(b.part(JE::PartKind::Anchor, "shear connectors"));
        b.gap("CONCRETE_PROPERTIES_NOT_AVAILABLE", "propriedades do concreto da laje nao estao no modelo de dados (MaterialSpec vazio)");
        b.gap("REINFORCEMENT_NOT_REPRESENTED", "armadura da laje nao representada como parte");
        break;
    case Rule::Pin:
        connectionParts.push_back(b.part(JE::PartKind::Plate, "pinned plate"));
        connectionParts.push_back(b.part(JE::PartKind::Auxiliary, "pin"));
        pin = true;
        bolted = false;
        break;
    case Rule::Unknown:
        break;
    }

    // uniões: nenhum pareamento e presumido; todas as partes participam da uniao (Aggregate)
    bool aggregate = false;
    const std::vector<int> all = b.allPartIds();
    if (pin) {
        b.link(JE::LinkKind::Pin, all, JE::Link::Resolution::Aggregate, "pino entre chapa e apoio");
        aggregate = true;
    }
    if (bolted) {
        b.link(JE::LinkKind::Bolted, all, JE::Link::Resolution::Aggregate, "parafusos (catalogo: usesBolts)");
        aggregate = true;
    }
    if (weldedKnown && welded) {
        b.link(JE::LinkKind::Welded, all, JE::Link::Resolution::Aggregate, "soldas (preset)");
        aggregate = true;
    }
    if (rule == Rule::EndPlate || rule == Rule::FlexEndPlate || rule == Rule::EndPlateSplice) {
        // por definicao da tipologia, a chapa de extremidade e soldada a viga: uniao entre a viga e a(s) chapa(s)
        std::vector<int> ids{beamPart};
        for (const int p : connectionParts) ids.push_back(p);
        b.link(JE::LinkKind::Welded, ids, JE::Link::Resolution::Aggregate, "solda viga-chapa de extremidade (pela tipologia)");
        aggregate = true;
    }
    if (rule == Rule::BasePlate) {
        b.link(JE::LinkKind::Contact, all, JE::Link::Resolution::Aggregate, "contato placa-bloco (graute nao armazenado)");
        b.link(JE::LinkKind::Embedded, all, JE::Link::Resolution::Aggregate, "chumbadores no concreto");
        b.link(JE::LinkKind::Welded, {beamPart, connectionParts.empty() ? beamPart : connectionParts.front()}, JE::Link::Resolution::Pairwise,
               "solda pilar-placa de base (pela tipologia)");
        aggregate = true;
    }
    if (rule == Rule::Tubular) {
        if (all.size() >= 2) {
            b.link(JE::LinkKind::Welded, all, JE::Link::Resolution::Aggregate, "ligacao soldada tubular");
            aggregate = true;
        } else {   // so uma barra: nao ha uniao representavel (a segunda nao e inventada)
            b.gap("BRACE_MEMBER_UNKNOWN", "a barra secundaria do no tubular nao esta identificada no modelo");
        }
    }
    if (!boltedKnown) {
        b.gap("FASTENER_TYPES_NOT_DERIVED", "tipos de uniao (parafuso/solda) nao derivados do tipo");
    }
    if (!weldedKnown && rule != Rule::BasePlate && rule != Rule::Pin && rule != Rule::EndPlate && rule != Rule::FlexEndPlate && rule != Rule::EndPlateSplice) {
        b.gap("WELD_PRESENCE_NOT_DERIVED_FROM_CATALOG", "presenca de soldas nao derivada do catalogo para esta tipologia (ver valores da ligacao)");
    }
    if (aggregate) {
        b.gap("LINK_PAIRING_NOT_DERIVED", "o pareamento exato das partes em cada uniao nao foi derivado; as uniões listam todas as partes (Aggregate)");
    }
    (void)extraKind;
    b.gap("LOAD_PATHS_NOT_DERIVED", "caminhos de carga (serie/paralelo) nao inferidos: exigem fundamento declarado");
    b.gap("LEGACY_VALUE_ORIGINS_NOT_RECORDED", "a origem dos valores numericos da ligacao legada nao esta registrada");
    b.t.status = JE::TopologyStatus::Partial;
    return b.t;
}

JE::MechanicalCapability JointTopologyAdapter::mechanicalCapability(const NC &connection)
{
    JE::MechanicalCapability cap;
    const ConnectionCatalog::Entry *entry = nullptr;
    const Rule rule = ruleFor(connection, &entry);
    if (rule == Rule::Unknown) {
        cap.status = JE::ValidityStatus::Unsupported;
        cap.justification = "tipologia desconhecida: sem regra de capacidade";
        return cap;
    }
    if (JointComponentService::isSupported(connection)) {
        cap.status = JE::ValidityStatus::ValidWithCaveats;
        cap.modelId = "EnComponentMethod";
        cap.justification = "metodo dos componentes EN 1993-1-8 (JointComponents) implementado para esta tipologia; sem capacidade de rotacao; limites em ModelDescriptor";
        cap.normativeResistanceChecks = true;
        return cap;
    }
    if (JointComponentService::hasResistanceOnlyModel(connection)) {
        cap.status = JE::ValidityStatus::ValidWithCaveats;
        cap.modelId = "NbrResistanceOnly";
        cap.justification = "so RESISTENCIA: Mj,Rd pelas verificacoes da NBR 8800:2024 da ligacao (nivel C); sem rigidez, sem curva M-theta (metodo dos componentes para perfis T nao implementado)";
        cap.normativeResistanceChecks = true;
        cap.missingFoundations = {"espalhamento da compressao do T-stub (sem regra na EN 1993-1-8 para esta junta)", "k11/k12 do T com a mesa da viga", "benchmark de junta completa com perfis T"};
        return cap;
    }
    cap.status = JE::ValidityStatus::NotYetAvailable;
    cap.normativeResistanceChecks = true;   // todas as tipologias do catalogo tem verificacao normativa de resistencia (NBR 8800 / NBR 14762 / CIDECT)
    const std::string base = "existe apenas verificacao normativa de RESISTENCIA; nao ha modelo de rigidez/deformacao fundamentado implementado";
    switch (rule) {
    case Rule::ShearTab: case Rule::DoubleAngle: case Rule::WebShear:
        cap.justification = base + " (ligacoes de cisalhamento: rotacao/rigidez de talas e cantoneiras sem fonte lida)";
        cap.missingFoundations = {"modelo de rigidez de tala/cantoneira (fonte a ler)", "deslizamento de parafusos"};
        break;
    case Rule::Gusset: case Rule::TrussNode:
        cap.justification = base + " (gusset/no de trelica: Whitmore, escoamento, ruptura, block shear e flambagem sao verificacoes, nao lei F-delta)";
        cap.missingFoundations = {"modelo mecanico de gusset (rigidez, distribuicao entre barras)", "topologia com N elementos conectados", "fontes AISC/NBR/CIDECT a ler (faixa F-GUS)"};
        break;
    case Rule::Splice: case Rule::CoverSplice: case Rule::TubeSplice: case Rule::EndPlateSplice:
        cap.justification = base + " (emendas: escorregamento/rigidez axial e de flexao nao modelados)";
        cap.missingFoundations = {"modelo de emenda (rigidez e deslizamento)", "confirmar se o modelo EN de chapa de extremidade cobre emenda"};
        break;
    case Rule::BasePlate:
        cap.justification = base + " (base: contato placa-concreto, graute e ancoragem nao modelados; concreto/graute sem propriedades no modelo de dados)";
        cap.missingFoundations = {"propriedades de concreto/graute", "modelo de contato e de ancoragem (faixa F-BAS)", "fontes EN 1992-4, ACI 318, EN 1993-1-8 6.2.5 a ler"};
        break;
    case Rule::Tubular:
        cap.justification = base + " (ligacao tubular soldada: resistencia por modos CIDECT/NBR; rigidez do no sem fonte)";
        cap.missingFoundations = {"rigidez de no tubular (fonte a ler)", "deformabilidade de solda (faixa F-SOL)"};
        break;
    case Rule::ColdFormed:
        cap.justification = base + " (formado a frio: dominio constitutivo proprio da NBR 14762)";
        cap.missingFoundations = {"modelo de parafuso em chapa fina", "fontes especificas"};
        break;
    case Rule::Moment: case Rule::EndPlate: case Rule::FlexEndPlate:
        cap.justification = base + " (variante nao coberta pelo modelo EN de chapa de extremidade estendida/mesas soldadas: ex.: perfis T, tala, chapa flexivel, altura parcial)";
        cap.missingFoundations = {"modelo de componentes para esta variante (fonte a ler)"};
        break;
    case Rule::Composite:
        cap.justification = base + " (ligacao mista: rigidez da ligacao mista nao modelada)";
        cap.missingFoundations = {"modelo da ligacao mista", "propriedades do concreto da laje"};
        break;
    case Rule::Pin:
        cap.justification = base + " (ligacao por pino)";
        cap.missingFoundations = {"modelo de folga/contato do pino (fonte a ler)"};
        break;
    case Rule::Unknown:
        break;
    }
    return cap;
}

JE::ConfigurationData JointTopologyAdapter::configurationData(const NC &connection, double beamYoungModulusMPa)
{
    JE::ConfigurationData d;
    for (auto it = connection.values.constBegin(); it != connection.values.constEnd(); ++it) {
        d.legacyValues[it.key().toStdString()] = it.value();
    }
    d.texts["type"] = std::to_string(connection.type);
    d.texts["atEnd"] = connection.atEnd ? "1" : "0";
    d.texts["grade"] = connection.grade.toStdString();
    d.texts["memberId"] = std::to_string(connection.memberId);
    d.texts["nodeId"] = std::to_string(connection.nodeId);
    if (std::isfinite(beamYoungModulusMPa)) {
        d.quantities["E_beam"] = JE::Quantity::make(beamYoungModulusMPa, "MPa", JE::Origin::Derived, "material da viga no modelo");
    }
    return d;
}

std::string JointTopologyAdapter::configurationFingerprint(const NC &connection, const std::vector<int> &memberIdsAtNode, double beamYoungModulusMPa)
{
    return JE::configurationFingerprint(derive(connection, memberIdsAtNode), configurationData(connection, beamYoungModulusMPa));
}

} // namespace Structura::App::Services
