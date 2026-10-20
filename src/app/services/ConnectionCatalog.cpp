#include "app/services/ConnectionCatalog.h"

#include "app/services/ConnectionCatalogInternal.h"

#include <QObject>

namespace Structura::App::Services {

using Structura::App::ProjectData::NodeConnection;
namespace cat = catalog;
using NC = Structura::App::ProjectData::NodeConnection;

namespace {

ConnectionCatalog::Entry webShear(int id, const QString &group, const QString &name, const QString &summary, double plates, bool welded, int support)
{
    ConnectionCatalog::Entry e;
    e.id = id;
    e.family = ConnectionCatalog::Family::WebShear;
    e.group = group;
    e.name = name;
    e.summary = summary;
    e.preset = {{QStringLiteral("plates"), plates}, {QStringLiteral("webWelded"), welded ? 1.0 : 0.0}, {QStringLiteral("support"), static_cast<double>(support)}};
    e.usesBolts = !welded;
    e.usesSupport = true;
    return e;
}

ConnectionCatalog::Entry alias(int id, const QString &group, const QString &name, const QString &summary, int legacy, const QMap<QString, double> &preset, bool bolts, bool support,
                               bool vertical = false)
{
    ConnectionCatalog::Entry e;
    e.id = id;
    e.family = ConnectionCatalog::Family::WebShear;   // ignorada: legacyType >= 0
    e.group = group;
    e.name = name;
    e.summary = summary;
    e.preset = preset;
    e.usesBolts = bolts;
    e.usesSupport = support;
    e.legacyType = legacy;
    e.vertical = vertical;
    return e;
}

ConnectionCatalog::Entry trussNode(int id, const QString &name, const QString &summary, int barMode, int chordMode)
{
    ConnectionCatalog::Entry e;
    e.id = id;
    e.family = ConnectionCatalog::Family::TrussNode;
    e.group = QObject::tr("Nós de treliça");
    e.name = name;
    e.summary = summary;
    e.preset = {{QStringLiteral("barMode"), static_cast<double>(barMode)}, {QStringLiteral("chordMode"), static_cast<double>(chordMode)}};
    e.usesBolts = barMode == 1 || chordMode == 3;
    e.usesSupport = false;   // o banzo e a barra principal da ligacao
    return e;
}

QVector<ConnectionCatalog::Entry> buildEntries()
{
    QVector<ConnectionCatalog::Entry> list;
    const QString single = QObject::tr("Ligação de viga – tala simples de alma");
    const QString dbl = QObject::tr("Ligação de viga – dupla tala de alma");
    const QString singleBase = QObject::tr("Uma tala soldada ao apoio e ligada à alma da viga por %1. Verifica parafusos/solda, tala (escoamento, ruptura, rasgamento, flexão), alma da viga, solda ao apoio, geometria e recortes (NBR 8800:2024, 6.2, 6.3 e 6.5).");
    const QString dblBase = QObject::tr("Duas talas (uma de cada lado da alma) soldadas ao apoio e ligadas à alma da viga por %1: corte duplo nos parafusos. Verifica parafusos/solda, talas, alma da viga, solda ao apoio, geometria e recortes (NBR 8800:2024, 6.2, 6.3 e 6.5).");
    const QString bolted = QObject::tr("parafusos"), welded = QObject::tr("solda");
    list.append(webShear(101, single, QObject::tr("Viga–viga soldada com tala simples de alma"), singleBase.arg(welded), 1, true, 0));
    list.append(webShear(102, single, QObject::tr("Viga–viga parafusada com tala simples de alma"), singleBase.arg(bolted), 1, false, 0));
    list.append(webShear(103, single, QObject::tr("Viga–pilar parafusada com tala simples (alma do pilar)"), singleBase.arg(bolted), 1, false, 1));
    list.append(webShear(104, single, QObject::tr("Viga–pilar parafusada com tala simples (mesa do pilar)"), singleBase.arg(bolted), 1, false, 2));
    list.append(webShear(105, single, QObject::tr("Viga–pilar soldada com tala simples (alma do pilar)"), singleBase.arg(welded), 1, true, 1));
    list.append(webShear(106, single, QObject::tr("Viga–pilar soldada com tala simples (mesa do pilar)"), singleBase.arg(welded), 1, true, 2));
    list.append(webShear(111, dbl, QObject::tr("Viga–viga parafusada com dupla tala de alma"), dblBase.arg(bolted), 2, false, 0));
    list.append(webShear(112, dbl, QObject::tr("Viga–viga soldada com dupla tala de alma"), dblBase.arg(welded), 2, true, 0));
    list.append(webShear(113, dbl, QObject::tr("Viga–pilar parafusada com dupla tala (mesa do pilar)"), dblBase.arg(bolted), 2, false, 2));
    list.append(webShear(114, dbl, QObject::tr("Viga–pilar parafusada com dupla tala (alma do pilar)"), dblBase.arg(bolted), 2, false, 1));
    list.append(webShear(115, dbl, QObject::tr("Viga–pilar soldada com dupla tala (mesa do pilar)"), dblBase.arg(welded), 2, true, 2));
    list.append(webShear(116, dbl, QObject::tr("Viga–pilar soldada com dupla tala (alma do pilar)"), dblBase.arg(welded), 2, true, 1));

    // ---- chapa de extremidade flexivel (altura parcial) -----------------------------------------------------------------------------------------------
    {
        const QString group = QObject::tr("Ligação de viga – chapa de extremidade flexível");
        const QString base = QObject::tr("Chapa de altura parcial soldada à alma da viga (filetes dos dois lados) e parafusada ao pilar em duas colunas simétricas em relação à alma: o cortante passa pelo centroide dos parafusos. Verifica parafusos (corte, contato, tração), chapa (escoamento, ruptura, rasgamento), solda e alma da viga (NBR 8800:2024, 6.2, 6.3 e 6.5).");
        for (const auto &spec : {std::pair<int, int>{107, 2}, std::pair<int, int>{108, 1}}) {
            ConnectionCatalog::Entry e;
            e.id = spec.first;
            e.family = ConnectionCatalog::Family::FlexEndPlate;
            e.group = group;
            e.name = spec.second == 2 ? QObject::tr("Viga–pilar com chapa de extremidade flexível (mesa do pilar)") : QObject::tr("Viga–pilar com chapa de extremidade flexível (alma do pilar)");
            e.summary = base;
            e.preset = {{QStringLiteral("support"), static_cast<double>(spec.second)}};
            e.usesBolts = true;
            e.usesSupport = true;
            list.append(e);
        }
    }

    // ---- ligacoes mistas de aco e concreto (Anexo O) --------------------------------------------------------------------------------------------------
    {
        const QString group = QObject::tr("Ligações para vigas mistas");
        const QString base = QObject::tr("Ligação mista de viga semicontínua sob momento negativo (NBR 8800:2024, Anexo O): o momento é resistido pela armadura da laje (tração) e pela ligação da mesa inferior (compressão); verifica armadura, conectores, ligação inferior, rigidez inicial, momento resistente, capacidade de rotação contra a necessária e a ligação da alma só ao cortante.");
        auto composite = [&](int id, const QString &name, const QString &summary, int variant, int support) {
            ConnectionCatalog::Entry e;
            e.id = id;
            e.family = ConnectionCatalog::Family::Composite;
            e.group = group;
            e.name = name;
            e.summary = base + QStringLiteral(" ") + summary;
            e.preset = {{QStringLiteral("cmVar"), static_cast<double>(variant)}, {QStringLiteral("cmSup"), static_cast<double>(support)}};
            e.usesBolts = true;
            e.usesSupport = true;
            return e;
        };
        list.append(composite(161, QObject::tr("Ligação mista com pilar de apoio: chapa de extremidade de altura total"), QObject::tr("Chapa de extremidade de altura total, mesa inferior soldada (Figura O.1)."), 0, 0));
        list.append(composite(162, QObject::tr("Ligação mista com pilar de apoio: cantoneiras na alma e na mesa inferior"), QObject::tr("Duas cantoneiras parafusadas na alma e cantoneira parafusada na mesa inferior (Figura O.2)."), 1, 0));
        list.append(composite(163, QObject::tr("Ligação mista com viga de apoio: cantoneiras na alma e na mesa inferior"), QObject::tr("Como a anterior, apoiada na alma de uma viga principal (Figura O.2)."), 1, 1));
        list.append(composite(164, QObject::tr("Ligação mista com pilar de apoio: chapa simples na alma e cantoneira na mesa inferior"), QObject::tr("Chapa simples parafusada na alma e cantoneira na mesa inferior (Figura O.3)."), 2, 0));
        list.append(composite(165, QObject::tr("Ligação mista com viga de apoio: chapa simples na alma e cantoneira na mesa inferior"), QObject::tr("Como a anterior, apoiada na alma de uma viga principal, com recorte das mesas (Figura O.3)."), 2, 1));
        list.append(composite(166, QObject::tr("Ligação mista com pilar de apoio: cantoneira apenas na mesa inferior"), QObject::tr("Somente a cantoneira da mesa inferior, que transmite também o cortante (Figura O.4)."), 3, 0));
        list.append(composite(167, QObject::tr("Ligação mista com viga de apoio: cantoneira apenas na mesa inferior"), QObject::tr("Somente a cantoneira da mesa inferior, apoiada em viga principal (Figura O.4)."), 3, 1));
    }

    // ---- variantes de tipologias anteriores ----------------------------------------------------------------------------------------------------
    {
        const QString rigid = QObject::tr("Ligações rígidas viga–pilar");
        list.append(alias(148, rigid, QObject::tr("Viga–pilar com talas fora do nó"), QObject::tr("Emenda por talas de mesa e de alma parafusadas a curta distância do pilar (a viga se liga ao pilar por solda): transmite momento, cortante e axial (NBR 8800:2024, 6.3)."), NC::kSplice,
                          {{QStringLiteral("outside"), 1.0}}, true, false));
        list.append(alias(152, QObject::tr("Cantoneiras"), QObject::tr("Viga–viga flexível com cantoneiras"), QObject::tr("Viga apoiada na alma de viga principal por duas cantoneiras parafusadas, com recorte das mesas da viga apoiada (NBR 8800:2024, 6.3 e 6.5)."), NC::kDoubleAngle,
                          {{QStringLiteral("supBeam"), 1.0}}, true, true));
        // bases de pilar
        const QString bases = QObject::tr("Bases de pilar");
        const QString baseBase = QObject::tr("Pilar I/H sobre bloco de concreto com chumbadores (NBR 8800:2024, 6.7): placa, chumbadores, pressão no concreto, atrito e nervuras de enrijecimento.");
        auto base = [&](int id, const QString &name, const QString &summary, const QMap<QString, double> &preset) {
            auto entry = alias(id, bases, name, baseBase + QStringLiteral(" ") + summary, NC::kBasePlate, preset, false, false, true);
            entry.extraFields = cat::baseRibFields;
            entry.extraDefaults = cat::baseRibDefaults;
            return entry;
        };
        list.append(base(172, QObject::tr("Base de pilar rígida com chapa de enrijecimento"), QObject::tr("Nervuras entre a placa e as mesas reduzem o balanço da placa."), {{QStringLiteral("ribs"), 4.0}}));
        list.append(base(173, QObject::tr("Base de pilar engastada"), QObject::tr("Seis nervuras e oito chumbadores para absorver momento."), {{QStringLiteral("ribs"), 6.0}, {QStringLiteral("anchors"), 8.0}}));
    }

    // ---- ligacoes rigidas viga-pilar ----------------------------------------------------------------------------------------------------------
    {
        auto moment = [](int id, const QString &name, const QString &summary, double flangeMode, double webMode, double fillet) {
            ConnectionCatalog::Entry e;
            e.id = id;
            e.family = ConnectionCatalog::Family::Moment;
            e.group = QObject::tr("Ligações rígidas viga–pilar");
            e.name = name;
            e.summary = summary;
            e.preset = {{QStringLiteral("flangeMode"), flangeMode}, {QStringLiteral("webMode"), webMode}, {QStringLiteral("flFillet"), fillet}};
            e.usesBolts = webMode > 0.0 || flangeMode > 0.0;
            e.usesSupport = true;
            return e;
        };
        const QString base = QObject::tr("O momento da viga vira binário nas mesas (F = M/(d − tf) ± N/2); verifica as mesas, a alma, a flexão local da mesa do pilar, escoamento e enrugamento da alma, flambagem lateral e o painel de alma (NBR 8800:2024, 5.7, 6.2, 6.3 e 6.5).");
        list.append(moment(143, QObject::tr("Viga–pilar rígida com perfis T (e tala de alma)"), base + QObject::tr(" Perfis T parafusados nas mesas (efeito de alavanca) e tala simples na alma."), 1.0, 1.0, 0.0));
        list.append(moment(144, QObject::tr("Viga–pilar soldada (mesas e alma)"), base + QObject::tr(" Mesas por solda de penetração total e alma por filetes direto no pilar."), 0.0, 0.0, 0.0));
        list.append(moment(145, QObject::tr("Viga–pilar rígida com simples tala de alma"), base + QObject::tr(" Mesas soldadas ao pilar e tala simples de alma."), 0.0, 1.0, 1.0));
        list.append(moment(146, QObject::tr("Viga–pilar rígida com dupla tala de alma"), base + QObject::tr(" Mesas soldadas ao pilar e dupla tala de alma."), 0.0, 2.0, 1.0));
    }

    // ---- emendas de barras axiais (cobrejuntas) e de tubos ---------------------------------------------------------------------------------
    {
        auto cover = [](int id, const QString &name, const QString &summary, double shape, double covers) {
            ConnectionCatalog::Entry e;
            e.id = id;
            e.family = ConnectionCatalog::Family::CoverSplice;
            e.group = QObject::tr("Emendas");
            e.name = name;
            e.summary = summary;
            e.preset = {{QStringLiteral("shape"), shape}, {QStringLiteral("covN"), covers}};
            e.usesBolts = true;
            e.usesSupport = false;
            return e;
        };
        const QString base = QObject::tr("Emenda parafusada com cobrejuntas, na tração ou compressão: parafusos (corte e contato), seção líquida efetiva, rasgamento e cobrejuntas (NBR 8800:2024, 5.2, 5.3, 6.3 e 6.5).");
        list.append(cover(132, QObject::tr("Emenda de perfis à tração (chapas com cobrejuntas duplas)"), base + QObject::tr(" Duas cobrejuntas (corte duplo)."), 0.0, 2.0));
        list.append(cover(137, QObject::tr("Emenda entre chapas (cobrejunta simples)"), base + QObject::tr(" Uma cobrejunta (corte simples, com flexão secundária)."), 0.0, 1.0));
        list.append(cover(133, QObject::tr("Emenda entre cantoneiras"), base + QObject::tr(" Cantoneira com cobrejunta na aba ligada (Ct pela excentricidade)."), 1.0, 1.0));
        ConnectionCatalog::Entry tube;
        tube.id = 134;
        tube.family = ConnectionCatalog::Family::TubeSplice;
        tube.group = QObject::tr("Emendas");
        tube.name = QObject::tr("Emenda entre tubos (chapas de extremidade parafusadas)");
        tube.summary = QObject::tr("Tubo circular com chapas de extremidade soldadas e parafusadas em círculo: parafusos sob tração e momento (efeito de alavanca, NBR 8800:2024, 6.3.5), cortante, solda e disposições.");
        tube.usesBolts = true;
        tube.usesSupport = false;
        list.append(tube);
    }

    // ---- emendas de vigas com chapa de extremidade -------------------------------------------------------------------------------------------
    {
        auto splice = [](int id, const QString &group, const QString &name, const QString &summary, double outside) {
            ConnectionCatalog::Entry e;
            e.id = id;
            e.family = ConnectionCatalog::Family::EndPlateSplice;
            e.group = group;
            e.name = name;
            e.summary = summary;
            e.preset = {{QStringLiteral("outside"), outside}};
            e.usesBolts = true;
            e.usesSupport = false;
            return e;
        };
        const QString base = QObject::tr("Duas chapas de extremidade soldadas às vigas e unidas por parafusos tracionados (efeito de alavanca, NBR 8800:2024, 6.3.5) e de cortante; soldas das mesas e da alma, largura e gabarito da chapa.");
        list.append(splice(135, QObject::tr("Emendas"), QObject::tr("Emenda de vigas I com chapa de topo"), base, 0.0));
        list.append(splice(147, QObject::tr("Ligações rígidas"), QObject::tr("Viga–pilar flangeada fora do nó"), base + QObject::tr(" Junta flangeada a curta distância do pilar (a viga se liga rigidamente ao pilar por solda)."), 1.0));
    }

    // ---- nos de trelica (NBR 8800:2024 e NBR 14762:2010) ----------------------------------------------------------------------------------
    const QString nodeBase = QObject::tr("Diagonais e montante de treliça plana ligados ao banzo. Verifica a ligação de cada barra, a barra e a chapa de nó (seção de Whitmore) e a interface com o banzo sob a resultante das barras (esforços e momento da excentricidade), mais a verificação local do banzo (NBR 8800:2024, 5.2, 5.3, 6.2, 6.3, 6.5 e 5.7).");
    list.append(trussNode(121, QObject::tr("Nó de treliça soldado: peças soldadas direto no banzo"), nodeBase + QObject::tr(" Barras unidas diretamente ao banzo por filetes."), 0, 0));
    list.append(trussNode(122, QObject::tr("Nó de treliça soldado: gousset com solda de topo no banzo"), nodeBase + QObject::tr(" Barras soldadas no gousset; gousset ao banzo por solda de penetração total."), 0, 1));
    list.append(trussNode(123, QObject::tr("Nó de treliça soldado: gousset com filetes no banzo"), nodeBase + QObject::tr(" Barras soldadas no gousset; gousset sobreposto ao banzo com filetes em 3 ou 4 lados."), 0, 2));
    list.append(trussNode(124, QObject::tr("Nó de treliça parafusado: gousset com solda de topo no banzo"), nodeBase + QObject::tr(" Barras parafusadas no gousset; gousset ao banzo por solda de penetração total."), 1, 1));
    list.append(trussNode(125, QObject::tr("Nó de treliça parafusado: gousset parafusado ao banzo"), nodeBase + QObject::tr(" Barras e gousset parafusados."), 1, 3));

    // ---- reticulado tubular (CIDECT DG1): variantes da ligacao tubular ----------------------------------------------------------------------
    const QString tub = QObject::tr("Reticulado tubular");
    const QString tubBase = QObject::tr("Diagonais de seção circular soldadas ao banzo tubular (CIDECT DG1, 2ª ed.): plastificação do banzo, punção, esmagamento e resistência das diagonais. ");
    list.append(alias(181, tub, QObject::tr("Reticulado tubular: diagonal–diagonal"), tubBase + QObject::tr("Duas diagonais no mesmo banzo (afastamento ou sobreposição conforme a geometria)."), NC::kTubular,
                      {{QStringLiteral("kindT"), 2.0}, {QStringLiteral("th1"), 60.0}, {QStringLiteral("th2"), 60.0}, {QStringLiteral("gap"), 20.0}}, false, false));
    list.append(alias(182, tub, QObject::tr("Reticulado tubular: diagonal–diagonal com afastamento (gap)"), tubBase + QObject::tr("Ligação K/N com afastamento entre as diagonais."), NC::kTubular,
                      {{QStringLiteral("kindT"), 2.0}, {QStringLiteral("th1"), 50.0}, {QStringLiteral("th2"), 50.0}, {QStringLiteral("gap"), 40.0}}, false, false));
    list.append(alias(183, tub, QObject::tr("Reticulado tubular: diagonal–diagonal com sobreposição (overlap)"), tubBase + QObject::tr("Ligação K/N com sobreposição das diagonais."), NC::kTubular,
                      {{QStringLiteral("kindT"), 3.0}, {QStringLiteral("th1"), 40.0}, {QStringLiteral("th2"), 40.0}, {QStringLiteral("overlap"), 60.0}}, false, false));
    list.append(alias(184, tub, QObject::tr("Reticulado tubular: diagonal–montante–diagonal"), tubBase + QObject::tr("Diagonal e montante (ângulo 90°) no mesmo nó do banzo (ligação N)."), NC::kTubular,
                      {{QStringLiteral("kindT"), 2.0}, {QStringLiteral("th1"), 45.0}, {QStringLiteral("th2"), 90.0}, {QStringLiteral("gap"), 30.0}}, false, false));
    list.append(alias(185, tub, QObject::tr("Reticulado tubular: diagonal–diagonal (tipo knee)"), tubBase + QObject::tr("Ligação em joelho: diagonais concorrentes no banzo com inclinação acentuada."), NC::kTubular,
                      {{QStringLiteral("kindT"), 1.0}, {QStringLiteral("th1"), 60.0}, {QStringLiteral("th2"), 60.0}}, false, false));
    list.append(alias(186, tub, QObject::tr("Reticulado tubular: diagonal–banzo"), tubBase + QObject::tr("Uma diagonal soldada ao banzo (ligação T/Y)."), NC::kTubular,
                      {{QStringLiteral("kindT"), 0.0}, {QStringLiteral("th1"), 90.0}}, false, false));

    // ---- emendas -----------------------------------------------------------------------------------------------------------------------------
    const QString splices = QObject::tr("Emendas");
    list.append(alias(131, splices, QObject::tr("Emenda entre perfis U"), QObject::tr("Continuidade de perfil U com talas de mesa e uma tala de alma parafusadas: momento, cortante e axial (NBR 8800:2024, 6.3 e 6.5)."), NC::kSplice,
                      {{QStringLiteral("wN"), 1.0}, {QStringLiteral("ipT"), 0.0}}, true, false));
    list.append(alias(136, splices, QObject::tr("Emenda de pilares"), QObject::tr("Continuidade de pilar I/H com talas de mesa e de alma parafusadas: compressão/tração, cortante e momento (NBR 8800:2024, 6.3)."), NC::kSplice,
                      {{QStringLiteral("gap"), 0.0}}, true, false, true));
    return list;
}

} // namespace

const QVector<ConnectionCatalog::Entry> &ConnectionCatalog::entries()
{
    static const QVector<Entry> list = buildEntries();
    return list;
}

const ConnectionCatalog::Entry *ConnectionCatalog::find(int type)
{
    if (!isCatalog(type)) {
        return nullptr;
    }
    for (const Entry &e : entries()) {
        if (e.id == type) {
            return &e;
        }
    }
    return nullptr;
}

const ConnectionCatalog::FamilyOps &ConnectionCatalog::ops(Family family)
{
    static const FamilyOps momentOps = [] {
        FamilyOps o;
        o.fields = cat::momentFields;
        o.makeDefault = cat::momentDefault;
        o.evaluate = cat::momentEvaluate;
        o.describe = cat::momentDescribe;
        o.solids = cat::momentSolids;
        o.steelKeys = cat::momentSteelKeys;
        o.notes = cat::momentNotes;
        return o;
    }();
    static const FamilyOps coverSpliceOps = [] {
        FamilyOps o;
        o.fields = cat::coverSpliceFields;
        o.makeDefault = cat::coverSpliceDefault;
        o.evaluate = cat::coverSpliceEvaluate;
        o.describe = cat::coverSpliceDescribe;
        o.solids = cat::coverSpliceSolids;
        o.steelKeys = cat::coverSpliceSteelKeys;
        return o;
    }();
    static const FamilyOps tubeSpliceOps = [] {
        FamilyOps o;
        o.fields = cat::tubeSpliceFields;
        o.makeDefault = cat::tubeSpliceDefault;
        o.evaluate = cat::tubeSpliceEvaluate;
        o.describe = cat::tubeSpliceDescribe;
        o.solids = cat::tubeSpliceSolids;
        o.steelKeys = cat::tubeSpliceSteelKeys;
        return o;
    }();
    static const FamilyOps endPlateSpliceOps = [] {
        FamilyOps o;
        o.fields = cat::endPlateSpliceFields;
        o.makeDefault = cat::endPlateSpliceDefault;
        o.evaluate = cat::endPlateSpliceEvaluate;
        o.describe = cat::endPlateSpliceDescribe;
        o.solids = cat::endPlateSpliceSolids;
        o.steelKeys = cat::endPlateSpliceSteelKeys;
        return o;
    }();
    static const FamilyOps trussNodeOps = [] {
        FamilyOps o;
        o.fields = cat::trussNodeFields;
        o.makeDefault = [](const Entry &e, const BeamInfo &beam, double fy, double fu) { return cat::trussNodeDefault(e, beam, fy, fu, {}); };
        o.makeWithBraces = cat::trussNodeDefault;
        o.evaluate = cat::trussNodeEvaluate;
        o.describe = cat::trussNodeDescribe;
        o.solids = cat::trussNodeSolids;
        o.steelKeys = cat::trussNodeSteelKeys;
        o.dims = cat::trussNodeDims;
        o.notes = cat::trussNodeNotes;
        return o;
    }();
    static const FamilyOps flexEndPlateOps = [] {
        FamilyOps o;
        o.fields = cat::flexEndPlateFields;
        o.makeDefault = cat::flexEndPlateDefault;
        o.evaluate = cat::flexEndPlateEvaluate;
        o.describe = cat::flexEndPlateDescribe;
        o.solids = cat::flexEndPlateSolids;
        o.steelKeys = cat::flexEndPlateSteelKeys;
        o.dims = cat::flexEndPlateDims;
        o.notes = cat::flexEndPlateNotes;
        return o;
    }();
    static const FamilyOps compositeOps = [] {
        FamilyOps o;
        o.fields = cat::compositeConnFields;
        o.makeDefault = cat::compositeConnDefault;
        o.evaluate = cat::compositeConnEvaluate;
        o.describe = cat::compositeConnDescribe;
        o.solids = cat::compositeConnSolids;
        o.steelKeysFor = cat::compositeConnSteelKeys;
        o.notes = cat::compositeConnNotes;
        return o;
    }();
    static const FamilyOps webShearOps = [] {
        FamilyOps o;
        o.fields = cat::webShearFields;
        o.makeDefault = cat::webShearDefault;
        o.evaluate = cat::webShearEvaluate;
        o.describe = cat::webShearDescribe;
        o.solids = cat::webShearSolids;
        o.steelKeys = cat::webShearSteelKeys;
        o.dims = cat::webShearDims;
        o.notes = cat::webShearNotes;
        return o;
    }();
    switch (family) {
    case Family::WebShear: return webShearOps;
    case Family::TrussNode: return trussNodeOps;
    case Family::EndPlateSplice: return endPlateSpliceOps;
    case Family::CoverSplice: return coverSpliceOps;
    case Family::Moment: return momentOps;
    case Family::FlexEndPlate: return flexEndPlateOps;
    case Family::Composite: return compositeOps;
    case Family::TubeSplice: return tubeSpliceOps;
    }
    return webShearOps;
}

bool ConnectionCatalog::usesBraces(int type)
{
    const Entry *e = find(type);
    return e && e->legacyType < 0 && static_cast<bool>(ops(e->family).makeWithBraces);
}

NodeConnection ConnectionCatalog::makeDefaultWithBraces(int type, const BeamInfo &beam, double plateFy, double plateFu, const QVector<BraceInfo> &braces)
{
    const Entry *e = find(type);
    if (!e || !ops(e->family).makeWithBraces) {
        return makeDefault(type, beam, plateFy, plateFu);
    }
    NodeConnection c = ops(e->family).makeWithBraces(*e, beam, plateFy, plateFu, braces);
    c.type = type;
    return c;
}

int ConnectionCatalog::baseType(int type)
{
    const Entry *e = find(type);
    return e && e->legacyType >= 0 ? baseType(e->legacyType) : type;
}

NodeConnection ConnectionCatalog::asLegacy(const NodeConnection &connection)
{
    NodeConnection legacy = connection;
    legacy.type = baseType(connection.type);
    return legacy;
}

bool ConnectionCatalog::isVertical(int type)
{
    const Entry *e = find(type);
    return e && e->vertical;
}

QVector<ConnectionCatalog::FieldDef> ConnectionCatalog::fields(int type)
{
    const Entry *e = find(type);
    if (e && e->legacyType >= 0) {
        QVector<FieldDef> list = ConnectionService::fields(e->legacyType);
        if (e->extraFields) {
            list += e->extraFields();
        }
        return list;
    }
    return e ? ops(e->family).fields(*e) : QVector<FieldDef>();
}

NodeConnection ConnectionCatalog::makeDefault(int type, const BeamInfo &beam, double plateFy, double plateFu)
{
    NodeConnection c;
    const Entry *e = find(type);
    if (!e) {
        c.type = type;
        return c;
    }
    if (e->legacyType >= 0) {
        c = e->legacyType == NodeConnection::kTubular ? ConnectionService::makeTubular(beam, {}) : ConnectionService::makeDefault(e->legacyType, beam, plateFy, plateFu);
        for (auto it = e->preset.constBegin(); it != e->preset.constEnd(); ++it) {
            c.values[it.key()] = it.value();
        }
        if (e->extraDefaults) {
            e->extraDefaults(c, beam);
        }
    } else {
        c = ops(e->family).makeDefault(*e, beam, plateFy, plateFu);
    }
    c.type = type;
    return c;
}

ConnectionCatalog::Evaluation ConnectionCatalog::evaluate(const NodeConnection &connection, const Forces &forces)
{
    const Entry *e = find(connection.type);
    if (e && e->legacyType >= 0) {
        return ConnectionService::evaluate(asLegacy(connection), forces);
    }
    return e ? ops(e->family).evaluate(connection, forces) : Evaluation();
}

QString ConnectionCatalog::describe(const NodeConnection &connection)
{
    const Entry *e = find(connection.type);
    if (e && e->legacyType >= 0) {
        return ConnectionService::describe(asLegacy(connection));
    }
    return e ? ops(e->family).describe(connection) : QString();
}

void ConnectionCatalog::solids(const NodeConnection &connection, bool context, std::vector<Primitive> &out)
{
    if (const Entry *e = find(connection.type)) {
        if (e->legacyType >= 0) {
            out = ConnectionSolids::build(asLegacy(connection), context);
            cat::variantDecorate(connection, context, out);
            return;
        }
        ops(e->family).solids(connection, context, out);
    }
}

bool ConnectionCatalog::propose(NodeConnection &connection, const Forces &forces)
{
    const Entry *e = find(connection.type);
    if (e && e->legacyType >= 0) {
        NodeConnection legacy = asLegacy(connection);
        if (!ConnectionService::propose(legacy, forces)) {
            return false;
        }
        legacy.type = connection.type;
        connection = legacy;
        return true;
    }
    return e && ops(e->family).propose ? ops(e->family).propose(connection, forces) : false;
}

bool ConnectionCatalog::hasProposal(int type)
{
    const Entry *e = find(type);
    if (e && e->legacyType >= 0) {
        return ConnectionService::hasProposal(e->legacyType);
    }
    return e && static_cast<bool>(ops(e->family).propose);
}

QPair<QString, QString> ConnectionCatalog::steelKeys(int type)
{
    const Entry *e = find(type);
    if (e && e->legacyType >= 0) {
        return ConnectionService::steelKeys(e->legacyType);
    }
    if (e && ops(e->family).steelKeysFor) {
        return ops(e->family).steelKeysFor(*e);
    }
    return e && ops(e->family).steelKeys ? ops(e->family).steelKeys() : QPair<QString, QString>();
}

} // namespace Structura::App::Services
