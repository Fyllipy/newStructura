#include "app/services/ConnectionCatalogInternal.h"

#include "design/CompositeConnection.h"

#include <QObject>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services::catalog {

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;
using Cat = ConnectionCatalog;

namespace {

int variantOf(const NodeConnection &c) { return static_cast<int>(val(c, "cmVar", 1.0) + 0.5); }
int supportOf(const NodeConnection &c) { return static_cast<int>(val(c, "cmSup", 0.0) + 0.5); }

/// Tipologia que verifica a ligacao da alma (somente cortante, O.2.5.1); -1 se a ligacao e so a cantoneira da mesa inferior (O.4).
int subTypeOf(int variant, int support)
{
    switch (variant) {
    case 0: return 107;   // chapa de extremidade flexivel (altura parcial) na mesa do pilar
    case 1: return NodeConnection::kDoubleAngle;
    case 2: return support == 0 ? 104 : 102;   // chapa simples parafusada: pilar (mesa) ou viga principal
    default: return -1;
    }
}

int subTypeOf(const NodeConnection &c) { return subTypeOf(variantOf(c), supportOf(c)); }

D::CompositeConnectionInput toInput(const NodeConnection &c, double shear, double axial, double moment)
{
    D::CompositeConnectionInput in;
    in.variant = variantOf(c);
    in.support = supportOf(c);
    in.d = val(c, "cmD", 400.0);
    in.bf = val(c, "cmBf", 180.0);
    in.tf = val(c, "cmTf", 12.5);
    in.tw = val(c, "cmTw", 8.0);
    in.radius = val(c, "cmR", 0.0);
    in.area = val(c, "cmA", 0.0);
    in.inertia = val(c, "cmIa", 0.0) * 1e4;   // cm4 -> mm4
    in.fy = val(c, "cmFy", 250.0);
    in.fu = val(c, "cmFu", 400.0);
    in.welded = val(c, "cmWelded", 0.0) != 0.0;
    in.weldLeg = val(c, "cmWeld", 6.0);
    in.weldFw = val(c, "weldFw", 485.0);
    in.concrete.fck = val(c, "cmFck", 30.0);
    in.slabKind = static_cast<Structura::Design::Composite::SlabKind>(std::clamp(static_cast<int>(val(c, "cmKind", 0.0) + 0.5), 0, 2));
    in.tc = val(c, "cmTc", 120.0);
    in.hF = in.slabKind == Structura::Design::Composite::SlabKind::Solid ? 0.0 : val(c, "cmHf", 75.0);
    in.ribWidth = val(c, "cmRibW", 110.0);
    in.span = val(c, "cmSpan", 8000.0);
    in.leftSpan = val(c, "cmLSpan", 8000.0);
    in.rightSpan = val(c, "cmRSpan", 8000.0);
    in.leftSpacing = val(c, "cmLSp", 3000.0);
    in.rightSpacing = val(c, "cmRSp", 3000.0);
    in.bars = std::max(1, static_cast<int>(val(c, "cmBars", 8.0) + 0.5));
    in.barDiameter = val(c, "cmPhi", 12.5);
    in.barSpacing = val(c, "cmSbar", 100.0);
    in.fys = val(c, "cmFys", 500.0);
    in.fts = 1.08 * in.fys;
    in.epsSu = val(c, "cmEpsSu", 5.0) / 100.0;
    in.y = val(c, "cmY", 80.0);
    in.transverseArea = val(c, "cmTrans", 0.0);
    in.ha = val(c, "cmHa", 260.0);
    in.bc = val(c, "cmBc", 260.0);
    in.a = val(c, "cmA1", 120.0);
    in.stiffeners = val(c, "cmStiff", 1.0) != 0.0;
    in.stiffenerArea = val(c, "cmStiffA", 0.0);
    in.columnD = val(c, "supD", 260.0) > 0.0 ? val(c, "supD", 260.0) : 260.0;
    in.columnTf = val(c, "supT", 12.5);
    in.studs.diameter = val(c, "cmStudD", 19.0);
    in.studs.fu = val(c, "cmStudFu", 415.0);
    in.studs.rows = std::max(1, static_cast<int>(val(c, "cmStudRows", 2.0) + 0.5));
    in.studs.perRib = std::max(1, static_cast<int>(val(c, "cmStudRib", 1.0) + 0.5));
    in.studs.emh = val(c, "cmStudEmh", 60.0);
    in.studs.throughDeck = true;
    in.connectors = static_cast<int>(val(c, "cmN", 0.0) + 0.5);
    in.connectorSpacing = val(c, "cmSstud", 200.0);
    in.krOverride = val(c, "cmKr", 0.0);
    in.grade = gradeByName(c.grade);
    in.db = val(c, "cmDb", 20.0);
    in.threadInShearPlane = val(c, "thread", 1.0) != 0.0;
    in.rowsL = std::max(1, static_cast<int>(val(c, "cmRowsL", 2.0) + 0.5));
    in.boltSpacing = val(c, "cmS", 70.0);
    in.boltGauge = val(c, "cmGauge", 90.0);
    in.boltEnd = val(c, "cmEnd", 35.0);
    in.angleLeg = val(c, "cmLegH", 150.0);
    in.angleLegVertical = val(c, "cmLegV", 100.0);
    in.angleT = val(c, "cmT", 16.0);
    in.angleFy = val(c, "cmAFy", 250.0);
    in.angleFu = val(c, "cmAFu", 400.0);
    in.verticalBolts = std::max(1, static_cast<int>(val(c, "cmVBolts", 2.0) + 0.5));
    in.verticalLever = val(c, "cmLever", 50.0);
    in.endPlateCjp = val(c, "cmCjp", 1.0) != 0.0;
    in.endPlateWeldLeg = val(c, "cmFw", 10.0);
    // ligacao da alma: dados da tipologia que a verifica
    const int variant = in.variant;
    if (variant == 0) {
        in.webT = val(c, "plateT", 8.0);
        in.webDb = val(c, "db", 20.0);
        in.webFy = val(c, "plateFy", 250.0);
        const double hp = val(c, "plateH", 0.0) > 0.0 ? val(c, "plateH", 0.0) : (count(c, "rows", 3.0) - 1) * val(c, "pitch", 70.0) + 2.0 * val(c, "edgeV", 35.0);
        in.webLc = hp;
        in.webBottom = std::max((in.d - hp) / 2.0, 0.0);
    } else if (variant == 1) {
        in.webT = val(c, "angleT", 9.5);
        in.webDb = val(c, "db", 20.0);
        in.webFy = val(c, "angleFy", 250.0);
        in.webLc = (count(c, "rows", 3.0) - 1) * val(c, "pitch", 70.0) + 2.0 * val(c, "edgeV", 35.0);
        in.webBottom = std::max((in.d - in.webLc) / 2.0, 0.0);
    } else if (variant == 2) {
        in.webT = val(c, "plateT", 9.5);
        in.webDb = val(c, "db", 20.0);
        in.webFy = val(c, "plateFy", 250.0);
        in.webLc = (count(c, "rows", 3.0) - 1) * val(c, "pitch", 70.0) + 2.0 * val(c, "edgeV", 35.0);
        in.webBottom = std::max((in.d - in.webLc) / 2.0, 0.0);
        in.webPlateWeldLeg = val(c, "weldSup", 6.0);
        in.webEndDistance = val(c, "edgeH", 35.0);
        in.webSlotted = val(c, "cmSlot", 1.0) != 0.0;
        in.webPretensioned = val(c, "cmPre", 1.0) != 0.0;
    }
    in.betaVm = val(c, "cmBeta", 0.95);
    in.loading = static_cast<int>(val(c, "cmLoad", 0.0) + 0.5);
    in.shored = val(c, "cmShored", 0.0) != 0.0;
    in.positiveMoment = val(c, "cmMpos", 0.0) * 1e6;
    in.positiveSpacing = val(c, "cmSpos", 150.0);
    in.moment = moment;
    in.shear = shear;
    in.axial = axial;
    return in;
}

} // namespace

QVector<Cat::FieldDef> compositeConnFields(const Cat::Entry &entry)
{
    const int variant = static_cast<int>(entry.preset.value(QStringLiteral("cmVar")) + 0.5);
    const int support = static_cast<int>(entry.preset.value(QStringLiteral("cmSup")) + 0.5);
    QVector<Cat::FieldDef> list;
    const int sub = subTypeOf(variant, support);
    if (sub >= 0) {
        list = ConnectionService::fields(sub);
        // os campos de aco e de forca de momento da tipologia da alma ja cobrem a ligacao ao cortante; o resto e proprio da ligacao mista
    }
    QString group;
    auto section = [&group](const QString &title) { group = title; };
    auto add = [&](const char *k, const QString &label, double min, double max, int decimals, const QString &suffix, const QStringList &options = {}, const QString &tip = {}) {
        list.append({QString::fromLatin1(k), label, min, max, decimals, suffix, group, options, tip});
    };
    const QString mm = QObject::tr(" mm"), mpa = QObject::tr(" MPa");
    const QStringList noYes = {QObject::tr("Não"), QObject::tr("Sim")};
    section(QObject::tr("Perfil de aço da viga mista"));
    add("cmD", QObject::tr("Altura d"), 100, 2000, 0, mm);
    add("cmBf", QObject::tr("Largura da mesa"), 50, 600, 0, mm);
    add("cmTf", QObject::tr("Espessura da mesa"), 4, 60, 1, mm);
    add("cmTw", QObject::tr("Espessura da alma"), 3, 40, 1, mm);
    add("cmR", QObject::tr("Raio de concordância (perfil laminado)"), 0, 40, 1, mm);
    add("cmFy", QObject::tr("fy do perfil"), 150, 350, 0, mpa, {}, QObject::tr("O Anexo O só vale para aço com fy ≤ 350 MPa (O.1.3)."));
    add("cmFu", QObject::tr("fu do perfil"), 250, 700, 0, mpa);
    add("cmA", QObject::tr("Área do perfil (0 = pelas dimensões)"), 0, 100000, 0, QObject::tr(" mm²"));
    add("cmIa", QObject::tr("Momento de inércia Ia (0 = pelas dimensões)"), 0, 1000000, 0, QObject::tr(" cm⁴"));
    add("cmWelded", QObject::tr("Perfil soldado"), 0, 1, 0, QString(), noYes);
    add("cmWeld", QObject::tr("Filete mesa–alma (perfil soldado)"), 3, 20, 1, mm);
    section(QObject::tr("Laje de concreto"));
    add("cmKind", QObject::tr("Tipo de laje"), 0, 2, 0, QString(), {QObject::tr("Maciça"), QObject::tr("Mista, nervuras perpendiculares"), QObject::tr("Mista, nervuras paralelas")});
    add("cmTc", QObject::tr("Altura do concreto (acima da forma)"), 40, 400, 0, mm);
    add("cmHf", QObject::tr("Altura da nervura da forma"), 0, 200, 0, mm);
    add("cmRibW", QObject::tr("Largura média da nervura"), 0, 400, 0, mm);
    add("cmFck", QObject::tr("fck do concreto"), 20, 80, 0, mpa);
    add("cmSpan", QObject::tr("Vão da viga ligada (L)"), 1000, 40000, 0, mm, {}, QObject::tr("Usado em L/dt (Tabelas O.1 a O.3) e em L1 = 15 % do vão."));
    add("cmLSpan", QObject::tr("Vão adjacente à esquerda do nó"), 1000, 40000, 0, mm);
    add("cmRSpan", QObject::tr("Vão adjacente à direita do nó"), 1000, 40000, 0, mm);
    add("cmLSp", QObject::tr("Distância à viga vizinha (esquerda)"), 0, 20000, 0, mm, {}, QObject::tr("0 = sem laje desse lado."));
    add("cmRSp", QObject::tr("Distância à viga vizinha (direita)"), 0, 20000, 0, mm);
    section(QObject::tr("Armadura da laje (momento negativo)"));
    add("cmBars", QObject::tr("Número de barras (na largura efetiva)"), 1, 60, 0, QString());
    add("cmPhi", QObject::tr("Diâmetro das barras (CA-50, mín. 12,5 mm)"), 6, 40, 1, mm);
    add("cmSbar", QObject::tr("Espaçamento das barras"), 30, 400, 0, mm);
    add("cmFys", QObject::tr("fyk da armadura"), 250, 700, 0, mpa);
    add("cmEpsSu", QObject::tr("Deformação na ruptura da armadura isolada"), 1, 15, 1, QObject::tr(" %"), {}, QObject::tr("εsu da armadura isolada (O.2.3.3); informar conforme a NBR 7480/ensaio."));
    add("cmY", QObject::tr("Topo do perfil – centro da armadura (y)"), 20, 400, 0, mm);
    add("cmTrans", QObject::tr("Armadura transversal de cada lado do pilar"), 0, 20000, 0, QObject::tr(" mm²"));
    section(QObject::tr("Conectores de cisalhamento (região de momento negativo)"));
    add("cmStudD", QObject::tr("Diâmetro do pino"), 12, 30, 1, mm);
    add("cmStudFu", QObject::tr("fu do pino"), 300, 600, 0, mpa);
    add("cmStudRows", QObject::tr("Pinos por seção"), 1, 4, 0, QString());
    add("cmStudRib", QObject::tr("Pinos por nervura (nervuras perpendiculares)"), 1, 3, 0, QString());
    add("cmStudEmh", QObject::tr("Borda do pino – alma da nervura (emh)"), 20, 200, 0, mm);
    add("cmSstud", QObject::tr("Espaçamento longitudinal dos pinos"), 50, 800, 0, mm);
    add("cmN", QObject::tr("Pinos entre o momento máximo e o nulo (0 = automático)"), 0, 200, 0, QString());
    add("cmKr", QObject::tr("Rigidez kr de um pino (0 = tabela O.2.4.1)"), 0, 400, 0, QObject::tr(" kN/mm"));
    section(QObject::tr("Apoio"));
    add("cmHa", QObject::tr("Largura do apoio paralela à armadura (ha)"), 50, 1200, 0, mm);
    add("cmBc", QObject::tr("Largura do pilar transversal às vigas (bc)"), 50, 1200, 0, mm);
    add("cmA1", QObject::tr("Primeiro conector – face do apoio (a)"), 30, 600, 0, mm);
    if (support == 0) {
        add("cmStiff", QObject::tr("Par de enrijecedores na alma do pilar"), 0, 1, 0, QString(), noYes);
        add("cmStiffA", QObject::tr("Área do par de enrijecedores"), 0, 50000, 0, QObject::tr(" mm²"));
    }
    if (variant >= 1) {
        section(QObject::tr("Cantoneira da mesa inferior"));
        add("cmDb", QObject::tr("Diâmetro dos parafusos"), 12, 36, 1, mm);
        add("cmRowsL", QObject::tr("Linhas de parafusos na aba horizontal (2 por linha)"), 1, 6, 0, QString());
        add("cmS", QObject::tr("Espaçamento S na direção da força"), 40, 300, 0, mm);
        add("cmGauge", QObject::tr("Gabarito transversal"), 40, 300, 0, mm);
        add("cmEnd", QObject::tr("Furo – borda"), 20, 100, 0, mm);
        add("cmLegH", QObject::tr("Largura da aba horizontal (transversal à força)"), 50, 400, 0, mm);
        add("cmLegV", QObject::tr("Altura da aba vertical"), 40, 300, 0, mm);
        add("cmT", QObject::tr("Espessura da cantoneira"), 6, 40, 1, mm);
        add("cmAFy", QObject::tr("fy da cantoneira"), 150, 500, 0, mpa);
        add("cmAFu", QObject::tr("fu da cantoneira"), 250, 700, 0, mpa);
        add("cmVBolts", QObject::tr("Parafusos da aba vertical (cortante em O.4)"), 1, 8, 0, QString());
        add("cmLever", QObject::tr("Eixo dos parafusos – face da aba vertical"), 20, 200, 0, mm);
    } else {
        section(QObject::tr("Mesa inferior soldada à chapa de extremidade"));
        add("cmCjp", QObject::tr("Solda da mesa inferior"), 0, 1, 0, QString(), {QObject::tr("Filete duplo"), QObject::tr("Penetração total")});
        add("cmFw", QObject::tr("Perna do filete duplo"), 4, 30, 1, mm);
    }
    if (variant == 2) {
        section(QObject::tr("Chapa simples da alma (O.2.5.1)"));
        add("cmSlot", QObject::tr("Furos pouco alongados no eixo da viga"), 0, 1, 0, QString(), noYes);
        add("cmPre", QObject::tr("Parafusos montados com pré-tensão"), 0, 1, 0, QString(), noYes);
    }
    section(QObject::tr("Rotação necessária (O.4)"));
    add("cmBeta", QObject::tr("βvm do momento positivo resistente"), 0.85, 0.95, 2, QString());
    add("cmLoad", QObject::tr("Carregamento"), 0, 1, 0, QString(), {QObject::tr("Distribuído ou 1 concentrada"), QObject::tr("2 concentradas nos terços")});
    add("cmShored", QObject::tr("Construção escorada"), 0, 1, 0, QString(), noYes);
    add("cmSpos", QObject::tr("Espaçamento dos conectores no vão (momento positivo)"), 50, 800, 0, mm);
    add("cmMpos", QObject::tr("M+Rd da viga mista (0 = automático, NBR 8800:2024 L.2.3.1)"), 0, 100000, 1, QObject::tr(" kN·m"), {}, QObject::tr("0: o programa calcula M+Rd da viga mista com o perfil, a laje, os conectores do vão e βvm; informe um valor para substituí-lo."));
    return list;
}

NodeConnection compositeConnDefault(const Cat::Entry &entry, const Cat::BeamInfo &beam, double plateFy, double plateFu)
{
    const int variant = static_cast<int>(entry.preset.value(QStringLiteral("cmVar")) + 0.5);
    const int support = static_cast<int>(entry.preset.value(QStringLiteral("cmSup")) + 0.5);
    const int sub = subTypeOf(variant, support);
    NodeConnection c = sub >= 0 ? ConnectionService::makeDefault(sub, beam, plateFy, plateFu) : NodeConnection();
    if (c.grade.isEmpty()) {
        c.grade = defaultBoltGradeName();
    }
    const double d = beam.depth > 0.0 ? beam.depth : 400.0, bf = beam.bf > 0.0 ? beam.bf : 180.0, tf = beam.tf > 0.0 ? beam.tf : 12.5;
    const double r = beam.geometry.value(QStringLiteral("r"), 0.0);
    auto &v = c.values;
    v["cmVar"] = variant;
    v["cmSup"] = support;
    v["supType"] = support == 0 ? 1.0 : 2.0;
    v["cmD"] = d; v["cmBf"] = bf; v["cmTf"] = tf; v["cmTw"] = beam.tw > 0.0 ? beam.tw : 8.0; v["cmR"] = r;
    v["cmFy"] = std::min(beam.fy, 350.0); v["cmFu"] = beam.fu; v["cmA"] = beam.area; v["cmIa"] = 0.0; v["cmWelded"] = 0.0; v["cmWeld"] = 6.0;
    v["cmKind"] = 0.0; v["cmTc"] = 120.0; v["cmHf"] = 75.0; v["cmRibW"] = 110.0; v["cmFck"] = 30.0;
    v["cmSpan"] = 8000.0; v["cmLSpan"] = 8000.0; v["cmRSpan"] = 8000.0; v["cmLSp"] = 3000.0; v["cmRSp"] = 3000.0;
    v["cmBars"] = 8.0; v["cmPhi"] = 12.5; v["cmSbar"] = 100.0; v["cmFys"] = 500.0; v["cmEpsSu"] = 5.0; v["cmY"] = 85.0; v["cmTrans"] = 0.0;
    v["cmStudD"] = 19.0; v["cmStudFu"] = 415.0; v["cmStudRows"] = 2.0; v["cmStudRib"] = 1.0; v["cmStudEmh"] = 60.0; v["cmSstud"] = 150.0; v["cmN"] = 0.0; v["cmKr"] = 0.0;
    v["cmHa"] = 260.0; v["cmBc"] = 260.0; v["cmA1"] = 150.0; v["cmStiff"] = 1.0; v["cmStiffA"] = std::round(1.1 * bf * tf);
    v["cmDb"] = 20.0; v["cmRowsL"] = 2.0; v["cmS"] = 70.0; v["cmGauge"] = std::clamp(std::round(bf * 0.5), 60.0, 120.0); v["cmEnd"] = 35.0;
    v["cmLegH"] = std::clamp(std::round(bf * 0.9), 80.0, 250.0); v["cmLegV"] = 100.0; v["cmT"] = 16.0; v["cmAFy"] = 250.0; v["cmAFu"] = 400.0; v["cmVBolts"] = 2.0; v["cmLever"] = 50.0;
    v["cmCjp"] = 1.0; v["cmFw"] = 10.0; v["cmSlot"] = 1.0; v["cmPre"] = 1.0;
    v["cmBeta"] = 0.95; v["cmLoad"] = 0.0; v["cmShored"] = 0.0; v["cmMpos"] = 0.0; v["cmSpos"] = 150.0;
    if (sub < 0) {
        v["supT"] = 12.0; v["supFy"] = plateFy; v["supFu"] = plateFu; v["supD"] = 0.0; v["supBf"] = 0.0; v["supTf"] = 0.0; v["supTw"] = 0.0; v["supR"] = 0.0; v["supLb"] = 0.0; v["supEnd"] = 1.0;
        v["thread"] = 1.0;
    }
    return c;
}

Cat::Evaluation compositeConnEvaluate(const NodeConnection &c, const Cat::Forces &f)
{
    Cat::Evaluation e;
    const int sub = subTypeOf(c);
    // ligacao da alma: so o cortante (O.2.5.1); o momento negativo e transmitido pela armadura e pela mesa inferior
    Cat::Forces shearOnly = f;
    shearOnly.moment = 0.0;
    for (auto &fc : shearOnly.cases) {
        fc.moment = 0.0;
    }
    if (sub >= 0) {
        NodeConnection s = c;
        s.type = sub;
        e = ConnectionService::evaluate(s, shearOnly);
        for (auto &check : e.checks) {
            check.name += " [alma, só cortante]";
        }
    }
    QVector<ConnectionService::ForceCase> cases = f.cases;
    if (cases.isEmpty()) {
        ConnectionService::ForceCase single;
        single.name = QObject::tr("envoltória");
        single.shear = f.shear;
        single.axial = f.axial;
        single.moment = f.moment;
        cases.append(single);
    }
    double worst = -1.0;
    D::CompositeConnectionResult best;
    QString governingCase;
    for (const auto &fc : cases) {
        auto result = D::checkCompositeConnection(toInput(c, fc.shear, fc.axial, fc.moment));
        const double ratio = D::worstRatio(result.checks);
        if (ratio > worst) {
            worst = ratio;
            best = std::move(result);
            governingCase = fc.name;
        }
    }
    for (auto &check : best.checks) {
        e.checks.push_back(std::move(check));
    }
    const auto parts = D::partsCompositeConnection(toInput(c, 0.0, 0.0, 0.0));
    for (const auto &p : parts) {
        e.parts.push_back(p);
    }
    e.maxRatio = D::worstRatio(e.checks);
    e.governing.clear();
    if (const auto *g = D::governing(e.checks)) {
        e.governing = QString::fromStdString(g->name);
    }
    e.governingCase = governingCase;
    e.description = compositeConnDescribe(c);
    return e;
}

QString compositeConnDescribe(const NodeConnection &c)
{
    const auto r = D::checkCompositeConnection(toInput(c, 0.0, 0.0, 0.0));
    const int variant = variantOf(c);
    const QString kind = variant == 0 ? QObject::tr("chapa de extremidade de altura total") : (variant == 1 ? QObject::tr("cantoneiras na alma e na mesa inferior")
                                                         : (variant == 2 ? QObject::tr("chapa simples na alma e cantoneira na mesa inferior") : QObject::tr("cantoneira na mesa inferior")));
    return QObject::tr("Ligação mista (%1): %2 d%3 CA-50 (As = %4 mm²), M−Rd = %5 kN·m, θu = %6 mrad, Si = %7 kN·m/rad")
        .arg(kind).arg(count(c, "cmBars", 8.0)).arg(val(c, "cmPhi", 12.5), 0, 'f', 1).arg(r.as, 0, 'f', 0).arg(r.mRd / 1e6, 0, 'f', 1).arg(r.thetaU * 1000.0, 0, 'f', 1).arg(r.si / 1e6, 0, 'f', 0);
}

void compositeConnSolids(const NodeConnection &c, bool context, std::vector<Cat::Primitive> &out)
{
    const D::CompositeConnectionInput in = toInput(c, 0.0, 0.0, 0.0);
    const int sub = subTypeOf(c);
    const double d = in.d, bf = in.bf, tf = in.tf, tw = in.tw;
    if (sub >= 0) {
        NodeConnection s = c;
        s.type = sub;
        ConnectionSolids::build(s, context).swap(out);
        if (in.support == D::CompositeConnectionInput::Beam && sub == NodeConnection::kDoubleAngle) {
            out.erase(std::remove_if(out.begin(), out.end(), [](const Cat::Primitive &p) { return p.role == Role::Concrete; }), out.end());
        }
    } else {
        // so a cantoneira da mesa inferior: viga com o apoio de referencia
        const double span = context ? 520.0 : 260.0, gap = 12.0;
        for (const double side : {-1.0, 1.0}) {
            out.push_back(sb::box(Role::Member, gap + span / 2.0, side * (d - tf) / 2.0, 0.0, span, tf, bf));
        }
        out.push_back(sb::box(Role::Member, gap + span / 2.0, 0.0, 0.0, span, d - 2.0 * tf, tw));
    }
    const double gap = 12.0;
    const bool column = in.support == D::CompositeConnectionInput::Column;
    // apoio: pilar (a mesa onde a viga se liga) ou viga principal (alma)
    if (sub < 0 || (in.support == D::CompositeConnectionInput::Beam && sub == NodeConnection::kDoubleAngle)) {
        if (column) {
            const double colTf = std::max(in.columnTf, 10.0);
            out.push_back(sb::box(Role::Concrete, -colTf / 2.0, 0.0, 0.0, colTf, d + 260.0, std::max(bf + 80.0, in.bc)));
        } else {
            const double dm = d + 40.0, tfm = 11.0, twm = 8.0, bfm = 140.0, span = 420.0;
            out.push_back(sb::box(Role::Concrete, -twm / 2.0, 0.0, 0.0, twm, dm - 2.0 * tfm, span));
            for (const double side : {-1.0, 1.0}) {
                out.push_back(sb::box(Role::Concrete, -bfm / 2.0 + twm / 2.0 - twm, side * (dm - tfm) / 2.0, 0.0, bfm, tfm, span));
            }
        }
    }
    // cantoneira da mesa inferior (O.2 a O.4): aba horizontal sob a mesa (parafusada), aba vertical contra o apoio
    if (in.variant >= 1) {
        const double t = in.angleT, legV = in.angleLegVertical, w = in.angleLeg;
        const double lenH = (in.rowsL - 1) * in.boltSpacing + in.boltEnd + in.verticalLever + t;
        const double tb = -(d / 2.0);
        out.push_back(sb::box(Role::Plate, gap + lenH / 2.0, tb - t / 2.0, 0.0, lenH, t, w));
        out.push_back(sb::box(Role::Plate, gap + t / 2.0, tb - legV / 2.0 + t / 2.0, 0.0, t, legV, w));
        for (int r = 0; r < in.rowsL; ++r) {
            for (const double col : {-1.0, 1.0}) {
                sb::addBolt(out, {gap + in.verticalLever + t + r * in.boltSpacing, 0.0, col * in.boltGauge / 2.0}, 1, tb - t - 4.0, tb + tf + 4.0, in.db);
            }
        }
        for (int k = 0; k < in.verticalBolts; ++k) {
            sb::addBolt(out, {0.0, tb - legV / 2.0 + t / 2.0 - ((in.verticalBolts - 1) / 2.0 - k) * std::min(legV / std::max(in.verticalBolts, 1), 60.0), (k % 2 ? 1.0 : -1.0) * w / 4.0}, 0, -3.0, gap + t + 3.0, in.db);
        }
    } else {
        // chapa de extremidade de altura total: reforco da mesa inferior por solda (representada) — a chapa vem da tipologia da alma
        out.push_back(sb::box(Role::Weld, gap + 3.0, -(d / 2.0 - tf / 2.0), 0.0, 6.0, tf + 2.0 * in.endPlateWeldLeg, bf + 2.0 * in.endPlateWeldLeg));
    }
    // pilar completo (alma e mesa oposta) e par de enrijecedores da alma no nivel da mesa inferior
    if (column && context) {
        const double colD = std::max(in.columnD, 200.0), colTf = std::max(in.columnTf, 10.0), colBf = std::max(in.bc, bf + 40.0), colTw = 8.0;
        const double height = d + 260.0;
        out.push_back(sb::box(Role::Concrete, -colD + colTf / 2.0, 0.0, 0.0, colTf, height, colBf));          // mesa oposta
        out.push_back(sb::box(Role::Concrete, -colD / 2.0, 0.0, 0.0, colD - 2.0 * colTf, height, colTw));       // alma
        if (in.stiffeners) {
            const double width = (colBf - colTw) / 2.0;
            const double ts = std::clamp(in.stiffenerArea / std::max(2.0 * width, 1.0), 6.0, 40.0);
            const double tb = -(d / 2.0 - tf / 2.0);
            for (const double side : {-1.0, 1.0}) {
                out.push_back(sb::box(Role::Plate, -colD / 2.0, tb, side * (colTw / 2.0 + width / 2.0), colD - 2.0 * colTf, ts, width));
            }
        }
    }
    if (context) {
        // laje com a armadura de momento negativo
        const double hs = in.tc, off = d / 2.0 + in.hF;
        const double slabWidth = std::max(560.0, std::min(in.bars * in.barSpacing + 200.0, 2400.0));
        out.push_back(sb::box(Role::Concrete, 330.0, off + hs / 2.0, 0.0, 700.0, hs, slabWidth));
        const int nShow = std::min(in.bars, 12);
        for (int i = 0; i < nShow; ++i) {
            const double u = (i - (nShow - 1) / 2.0) * (slabWidth - 160.0) / std::max(nShow - 1, 1);
            out.push_back(sb::cylinder(Role::Weld, {-120.0, off + hs * 0.7, u}, {700.0, off + hs * 0.7, u}, std::max(in.barDiameter / 2.0, 3.0)));
        }
        // conectores sobre a mesa superior
        const int studs = 4;
        for (int i = 0; i < studs; ++i) {
            for (int rr = 0; rr < in.studs.rows; ++rr) {
                const double u = (rr - (in.studs.rows - 1) / 2.0) * std::min(bf * 0.5, 60.0);
                const double s = in.a + i * in.connectorSpacing;
                out.push_back(sb::cylinder(Role::Bolt, {s, d / 2.0, u}, {s, d / 2.0 + std::min(in.studs.diameter * 4.0, in.hF + hs - 20.0), u}, in.studs.diameter / 2.0));
            }
        }
    }
}

std::vector<std::string> compositeConnNotes(const NodeConnection &c)
{
    const D::CompositeConnectionInput in = toInput(c, 0.0, 0.0, 0.0);
    std::vector<std::string> notes;
    char buffer[240];
    std::snprintf(buffer, sizeof buffer, "ARMADURA DA LAJE: %d BARRAS CA-50 d = %.1f mm A CADA %.0f mm (As = %.0f mm2) NA LARGURA EFETIVA.", in.bars, in.barDiameter, in.barSpacing, in.bars * 0.25 * 3.14159265 * in.barDiameter * in.barDiameter);
    notes.push_back(buffer);
    std::snprintf(buffer, sizeof buffer, "CONECTORES: PINOS d = %.0f mm, %d POR SEÇÃO, A CADA %.0f mm.", in.studs.diameter, in.studs.rows, in.connectorSpacing);
    notes.push_back(buffer);
    if (in.variant >= 1) {
        std::snprintf(buffer, sizeof buffer, "CANTONEIRA INFERIOR t = %.1f mm, %d PARAFUSOS d = %.0f mm NA ABA HORIZONTAL.", in.angleT, 2 * in.rowsL, in.db);
        notes.push_back(buffer);
    }
    notes.push_back("A LIGAÇÃO DA ALMA TRANSMITE SOMENTE O CORTANTE (NBR 8800:2024, O.2.5.1).");
    return notes;
}

QPair<QString, QString> compositeConnSteelKeys(const Cat::Entry &entry)
{
    const int sub = subTypeOf(static_cast<int>(entry.preset.value(QStringLiteral("cmVar")) + 0.5), static_cast<int>(entry.preset.value(QStringLiteral("cmSup")) + 0.5));
    return sub >= 0 ? ConnectionService::steelKeys(sub) : QPair<QString, QString>();
}

} // namespace Structura::App::Services::catalog
