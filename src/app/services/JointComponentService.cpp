#include "design/ColumnFlangeLever.h"
#include "app/services/JointComponentService.h"

#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionService.h"
#include "design/EndPlateConnection.h"
#include "design/MomentConnection.h"

#include <QCryptographicHash>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>

namespace Structura::App::Services {

namespace D = Structura::Design::Connections;
namespace JC = Structura::Design::JointComponents;
namespace JS = Structura::Design::JointClass;
using NodeConnection = Structura::App::ProjectData::NodeConnection;

namespace {

constexpr double kPi = 3.14159265358979323846;

enum class Family { None, EndPlate, EndPlateSplice, EndPlateSpliceOutside, MomentWelded, MomentTStub };

Family familyOf(const NodeConnection &c)
{
    if (c.type == NodeConnection::kEndPlate) {
        return Family::EndPlate;
    }
    if (ConnectionCatalog::isCatalog(c.type)) {
        const auto *entry = ConnectionCatalog::find(c.type);
        if (entry && entry->legacyType == NodeConnection::kEndPlate) {
            return Family::EndPlate;
        }
        // INCREMENTO 5: emenda de viga com duas chapas de extremidade (viga-viga, sem pilar).
        // INCREMENTO 6: a variante "flangeada fora do no" (outside != 0) e a MESMA emenda ligada ao pilar por mesas soldadas: composicao em serie (ver `evaluate`).
        if (entry && entry->legacyType < 0 && entry->family == ConnectionCatalog::Family::EndPlateSplice) {
            return entry->preset.value(QStringLiteral("outside"), 0.0) == 0.0 ? Family::EndPlateSplice : Family::EndPlateSpliceOutside;
        }
        if (const auto in = ConnectionCatalog::momentInput(c)) {
            return in->flangeMode == D::MomentConnectionInput::Welded ? Family::MomentWelded : Family::MomentTStub;
        }
    }
    return Family::None;
}

/// A secao do pilar foi DECLARADA? (barra de apoio I/H vinculada, ou geometria informada e confirmada pelo projetista). `ConnectionCatalog::momentInput` substitui o pilar ausente
/// por 260 x 146 x 12,7 x 7,6; esse valor padrao NAO pode alimentar uma resposta mecanica (regra A1: nenhum pilar padrao).
bool columnDeclared(const NodeConnection &c)
{
    auto v = [&c](const char *key) { return c.values.value(QString::fromLatin1(key), 0.0); };
    const bool numbers = v("supD") != 0.0 || v("supBf") != 0.0 || v("supTf") != 0.0 || v("supTw") != 0.0;
    if (v("supMember") > 0.5) {
        return v("supType") >= 0.5 && numbers;
    }
    return numbers && v("supConfirmed") >= 0.5;
}

template <typename... Args>
std::string fmt(const char *format, Args... args)
{
    char buf[512];
    std::snprintf(buf, sizeof buf, format, static_cast<double>(args)...);
    return buf;
}

using Ratios = std::vector<std::pair<std::string, double>>;

Ratios ratiosOf(const std::vector<D::Check> &checks)
{
    Ratios out;
    for (const D::Check &c : checks) {
        if (D::isColumnFlangeLeverCheck(c)) {
            continue;   // verificacoes do A2 (alavanca da mesa do pilar): a ferramenta momento-rotacao mantem o seu modelo proprio nesta etapa
        }
        out.emplace_back(c.name, c.ratio());
    }
    return out;
}

/// Maior momento (N.mm) para o qual todas as verificacoes que dependem do momento ficam <= 1; negativo se nao ha como determinar.
double momentResistance(const std::function<Ratios(double)> &ratiosAt, std::vector<std::string> &notes, std::string &governingName)
{
    const Ratios base = ratiosAt(0.0);
    const Ratios probe = ratiosAt(1.0e6);
    std::vector<std::size_t> dependent;
    for (std::size_t i = 0; i < base.size() && i < probe.size(); ++i) {
        if (probe[i].second > base[i].second + 1e-12) {
            dependent.push_back(i);
        }
    }
    for (std::size_t i = 0; i < base.size(); ++i) {
        if (base[i].second > 1.0 + 1e-9 && std::find(dependent.begin(), dependent.end(), i) == dependent.end()) {
            notes.push_back("Verificação não atendida independente do momento: " + base[i].first);
        }
    }
    if (dependent.empty()) {
        notes.push_back("Nenhuma verificação da ligação depende do momento: Mj,Rd não pode ser determinado pelas verificações da NBR.");
        return -1.0;
    }
    auto worst = [&](double m, std::string *name = nullptr) {
        const Ratios r = ratiosAt(m);
        double g = 0.0;
        for (const std::size_t i : dependent) {
            if (i < r.size() && r[i].second > g) {
                g = r[i].second;
                if (name) {
                    *name = r[i].first;
                }
            }
        }
        return g;
    };
    double lo = 0.0, hi = 1.0e6;
    while (worst(hi) <= 1.0 && hi < 1.0e15) {
        lo = hi;
        hi *= 2.0;
    }
    if (worst(hi) <= 1.0) {
        return -1.0;
    }
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (lo + hi);
        (worst(mid) <= 1.0 ? lo : hi) = mid;
    }
    worst(lo * 1.0001, &governingName);
    return lo;
}

} // namespace

bool JointComponentService::isSupported(const NodeConnection &c)
{
    const Family f = familyOf(c);
    return f == Family::EndPlate || f == Family::EndPlateSplice || f == Family::EndPlateSpliceOutside || f == Family::MomentWelded;
}

bool JointComponentService::hasResistanceOnlyModel(const NodeConnection &c)
{
    return familyOf(c) == Family::MomentTStub;
}

std::string JointComponentService::fingerprint(const NodeConnection &c, double youngModulusMPa)
{
    QByteArray data;
    data += QByteArray::number(c.type) + '|' + QByteArray::number(c.memberId) + '|' + QByteArray::number(c.nodeId) + '|' + (c.atEnd ? '1' : '0') + '|' + c.grade.toUtf8() + '|';
    QStringList keys = c.values.keys();
    keys.sort();
    for (const QString &key : keys) {
        data += key.toUtf8() + '=' + QByteArray::number(c.values.value(key), 'g', 12) + ';';
    }
    data += "E=" + QByteArray::number(youngModulusMPa, 'g', 10);
    return QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(12).toStdString();
}

JointComponentService::Result JointComponentService::evaluate(const NodeConnection &c, double youngModulusMPa)
{
    return evaluate(c, youngModulusMPa, JS::Direction::Positive);
}

JointComponentService::Result JointComponentService::evaluate(const NodeConnection &c, double youngModulusMPa, JS::Direction direction)
{
    const bool negative = direction == JS::Direction::Negative;
    Result r;
    r.direction = direction;
    const Family family = familyOf(c);
    if (family == Family::None) {
        r.message = "A tipologia desta ligação não é uma ligação viga-pilar que transmita momento: não há modelo de componentes.";
        return r;
    }
    r.fingerprint = fingerprint(c, youngModulusMPa);
    if (negative) {
        r.fingerprint = QCryptographicHash::hash(QByteArray::fromStdString(r.fingerprint) + "|dir=-", QCryptographicHash::Sha1).toHex().left(12).toStdString();
    }
    if ((family == Family::MomentTStub || family == Family::MomentWelded) && !columnDeclared(c)) {
        r.message = "Resposta não calculável: informe a seção do pilar (barra de apoio I/H vinculada ou geometria informada e confirmada). Nenhum pilar padrão é assumido.";
        return r;
    }
    if (family == Family::MomentTStub) {
        // INCREMENTO 6: perfis T parafusados: SO a resistencia (nivel C). O metodo dos componentes para esta junta NAO esta implementado (faltam o espalhamento da compressao do T-stub,
        // k11/k12 do T com a mesa da viga e um benchmark de junta completa); nenhuma rigidez ou curva e inventada.
        const D::MomentConnectionInput in = *ConnectionCatalog::momentInput(c);
        r.family = "mesas da viga ligadas ao pilar por perfis T parafusados (só resistência: método dos componentes não implementado)";
        r.symmetricGeometry = true;   // um perfil T em cada mesa da viga, mesmos dados: entrada identica nos dois sentidos
        r.directionNote = negative ? "sentido NEGATIVO (mesa inferior tracionada): perfis T idênticos nas duas mesas (simetria demonstrada pela configuração)." : "sentido POSITIVO (mesa superior tracionada): perfis T idênticos nas duas mesas.";
        if (in.colDepth <= 0.0 || in.colBf <= 0.0 || in.colTf <= 0.0 || in.colTw <= 0.0) {
            r.message = "Resistência não calculável: informe ou confirme a seção do pilar.";
            return r;
        }
        std::vector<std::string> tnotes;
        std::string tgov;
        const double nbrNmm = momentResistance([in](double m) {
            D::MomentConnectionInput x = in;
            x.moment = m;
            x.shear = 0.0;
            x.axial = 0.0;
            return ratiosOf(D::checkMomentConnection(x));
        }, tnotes, tgov);
        if (!(nbrNmm > 0.0)) {
            r.message = "Mj,Rd não determinado pelas verificações da NBR";
            for (const auto &n : tnotes) {
                r.message += "; " + n;
            }
            return r;
        }
        r.supported = true;
        r.resistanceOnly = true;
        r.nbrCheckAvailable = true;
        r.momentResistanceNbrKnm = nbrNmm * 1.0e-6;
        r.momentResistanceKnm = r.momentResistanceNbrKnm;
        r.nbrGoverning = tgov;
        r.message = "Só a resistência: Mj,Rd pelas verificações da NBR 8800:2024 (nível C). Rigidez inicial e curva M–θ indisponíveis (método dos componentes para perfis T não implementado).";
        r.memory.push_back("MOMENTO RESISTENTE DE CÁLCULO Mj,Rd — SÓ RESISTÊNCIA (perfis T parafusados)");
        r.memory.push_back(fmt("  NÍVEL C (verificações da NBR 8800:2024 da ligação, V = N = 0): Mj,Rd = %.2f kN.m", r.momentResistanceNbrKnm) + (tgov.empty() ? "" : "  (governa: " + tgov + ")"));
        r.memory.push_back("  Não é o método dos componentes: não há Sj,ini, curva M–θ, componentes nem capacidade de rotação para esta junta.");
        for (const auto &n : tnotes) {
            r.memory.push_back("  Aviso: " + n);
        }
        r.behavior.origin = JS::DataOrigin::None;
        return r;
    }
    JC::ModelInput mi;
    mi.youngModulus = youngModulusMPa > 0.0 ? youngModulusMPa : 200000.0;
    std::function<Ratios(double)> ratiosAt;
    NodeConnection columnSource = c;   // INCREMENTO 6: cadastro legado com a SECAO REAL do pilar (parte soldada da ligacao fora do no)
    columnSource.type = NodeConnection::kEndPlate;
    if (family == Family::EndPlate || family == Family::EndPlateSplice || family == Family::EndPlateSpliceOutside) {
        const bool outsideJoint = family == Family::EndPlateSpliceOutside;
        const bool splice = family == Family::EndPlateSplice || outsideJoint;
        // emenda: converte para o cadastro legado da chapa de extremidade (como a verificacao da NBR da propria emenda: so a flexao da outra chapa e mantida); a secao do pilar
        // do cadastro legado NAO e usada no modelo de componentes (nao ha pilar)
        NodeConnection source = c;
        if (splice) {
            source.type = NodeConnection::kEndPlate;
            source.values["supTf"] = c.values.value(QStringLiteral("plateT"), 22.4);
            if (!outsideJoint) {   // emenda viga-viga: sem pilar; a ligacao fora do no mantem a secao real do pilar (usada na parte soldada)
                source.values["supType"] = 0.0;
                source.values["supConfirmed"] = 1.0;
                source.values["supD"] = 260.0;   // valores neutros do cadastro legado; ignorados pelo modelo da emenda
                source.values["supBf"] = 146.0;
                source.values["supTw"] = 7.6;
                source.values["supR"] = 10.0;
            }
            if (negative) {   // sentido negativo: as fileiras tracionadas sao as de baixo; troca o cadastro (tRows <-> sRows) para o modelo E para as verificacoes da NBR
                const double up = source.values.value(QStringLiteral("tRows"), 2.0), down = source.values.value(QStringLiteral("sRows"), 1.0);
                source.values["tRows"] = down;
                source.values["sRows"] = up;
            }
        }
        D::EndPlateInput in = ConnectionService::endPlateInput(source);
        r.symmetricGeometry = std::max(1, in.tensionRows) == std::max(1, in.shearRows);
        {
            // mesa INFERIOR tracionada (sentido negativo): as fileiras tracionadas sao as que existem ABAIXO da mesa inferior (as da zona "comprimida" do sentido positivo) e a
            // chapa acima da mesa superior passa a ser a extensao do lado comprimido (limita a dispersao sp). firstRow, pitch e edgeVertical valem nas duas zonas.
            const int above = std::max(1, in.tensionRows);
            const int below = std::max(1, in.shearRows);
            if (negative && !splice) {
                in.tensionRows = below;
                in.shearRows = above;
                r.directionNote = "sentido NEGATIVO (mesa inferior tracionada): " + std::to_string(below) + " fileira(s) de parafusos abaixo da mesa inferior atuam como fileiras tracionadas; "
                                  + std::to_string(above) + " fileira(s) acima da mesa superior ficam no lado comprimido.";
            } else if (negative) {   // emenda: o cadastro ja foi trocado acima (in.tensionRows = fileiras de baixo)
                r.directionNote = "sentido NEGATIVO (mesa inferior tracionada): " + std::to_string(above) + " fileira(s) de parafusos abaixo da mesa inferior atuam como fileiras tracionadas; "
                                  + std::to_string(below) + " fileira(s) acima da mesa superior ficam no lado comprimido.";
            } else {
                r.directionNote = "sentido POSITIVO (mesa superior tracionada): " + std::to_string(above) + " fileira(s) de parafusos na extensão da chapa acima da mesa superior atuam como fileiras tracionadas.";
            }
        }
        r.family = outsideJoint ? "viga–pilar flangeada fora do nó: mesas soldadas ao pilar em série com emenda por duas chapas de extremidade"
                 : splice ? "emenda de viga com duas chapas de extremidade parafusadas (fileiras na extensão tracionada, sem pilar)" : "chapa de extremidade parafusada (fileiras na extensão tracionada)";
        mi.kind = JC::ModelInput::ExtendedEndPlate;
        mi.beamSplice = splice;
        if (splice) {
            r.hypotheses.push_back("Emenda de viga (EN 1993-1-8, Tabela 6.10 e 6.3.3.1(5); resistência por 6.2.7.2(10)): a força atravessa as duas chapas e os parafusos; não há pilar (k1 a k4 inexistentes).");
            r.hypotheses.push_back("As duas chapas de extremidade são iguais (mesma espessura e aço, como no cadastro da emenda); a rigidez usa k5 de cada chapa em série.");
        }
        mi.beamDepth = in.beamDepth;
        mi.beamBf = in.flangeWidth;
        mi.beamTf = in.flangeThickness;
        mi.beamTw = in.webThickness;
        mi.beamFy = in.beam.fy;
        mi.colDepth = in.colDepth;
        mi.colBf = in.colFlangeWidth;
        mi.colTf = in.colFlangeThickness;
        mi.colTw = in.colWebThickness;
        mi.colRadius = in.colRadius;
        mi.colFy = in.column.fy;
        mi.plateWidth = in.plateWidth;
        mi.plateThickness = in.plateThickness;
        mi.plateFy = in.plate.fy;
        mi.weldLeg = in.weldFlangeLeg;
        mi.flangeThroat = in.weldFlangeLeg / std::sqrt(2.0);   // garganta ap do filete (perna / raiz(2))
        mi.gauge = in.gauge;
        mi.tensionRows = in.tensionRows;
        mi.firstRow = in.firstRow;
        mi.pitch = in.pitch;
        mi.edgeVertical = in.edgeVertical;
        mi.plateBelowFlange = in.firstRow + (std::max(1, in.shearRows) - 1) * in.pitch + in.edgeVertical;
        mi.boltDiameter = in.db;
        const double ab = kPi * in.db * in.db / 4.0;
        mi.boltStressArea = 0.78 * ab;
        mi.boltTensionResistance = 0.75 * ab * in.grade.fub / in.gammaA2;   // NBR 8800:2024, 6.3.3.1 (mesma resistencia da verificacao do programa)
        mi.beta = in.beamOnBothSides ? 0.0 : 1.0;
        mi.webStiffened = in.continuityPlates;
        mi.doublerThickness = in.doublerThickness;
        mi.gammaYield = in.gammaA1;
        if (splice) {
            // verificacoes da NBR da PROPRIA emenda (catalogo: as de pilar sao descartadas); sentido negativo = cadastro com as zonas trocadas (`source`)
            NodeConnection catalogSource = c;
            if (negative) {
                catalogSource.values["tRows"] = source.values.value(QStringLiteral("tRows"), 1.0);
                catalogSource.values["sRows"] = source.values.value(QStringLiteral("sRows"), 2.0);
            }
            ratiosAt = [catalogSource](double m) {
                ConnectionService::Forces forces;
                forces.moment = m;
                const auto evaluation = ConnectionCatalog::evaluate(catalogSource, forces);
                return ratiosOf(evaluation.checks);
            };
        } else {
            ratiosAt = [in](double m) {
                D::EndPlateInput x = in;
                x.moment = m;
                x.shear = 0.0;
                x.axial = 0.0;
                return ratiosOf(D::checkEndPlate(x));
            };
        }
    } else {
        const D::MomentConnectionInput in = *ConnectionCatalog::momentInput(c);
        r.family = "mesas da viga soldadas ao pilar";
        r.symmetricGeometry = true;   // viga e pilar I/H duplamente simetricos e dado unico para as duas mesas: a entrada de um sentido e identica a do outro
        r.directionNote = negative ? "sentido NEGATIVO (mesa inferior tracionada): mesas soldadas, geometria idêntica à do sentido positivo (simetria demonstrada pela configuração)."
                                   : "sentido POSITIVO (mesa superior tracionada): mesas soldadas.";
        mi.kind = JC::ModelInput::WeldedFlanges;
        mi.beamDepth = in.beamDepth;
        mi.beamBf = in.beamBf;
        mi.beamTf = in.beamTf;
        mi.beamTw = in.beamTw;
        mi.beamFy = in.beam.fy;
        mi.colDepth = in.colDepth;
        mi.colBf = in.colBf;
        mi.colTf = in.colTf;
        mi.colTw = in.colTw;
        mi.colRadius = in.colRadius;
        mi.colFy = in.column.fy;
        mi.flangeThroat = in.flangeFillet ? in.flangeWeldLeg / std::sqrt(2.0) : 0.0;   // penetracao total: ap = 0 (conservador)
        mi.beta = in.beamOnBothSides ? 0.0 : 1.0;
        mi.webStiffened = in.panelStiffened;
        mi.doublerThickness = in.doubler;
        mi.gammaYield = in.gammaA1;
        ratiosAt = [in](double m) {
            D::MomentConnectionInput x = in;
            x.moment = m;
            x.shear = 0.0;
            x.axial = 0.0;
            return ratiosOf(D::checkMomentConnection(x));
        };
    }
    // pilar sem secao informada/confirmada (A1): orienta o usuario; nenhum pilar padrao e restaurado e o calculo de componentes nao e alterado
    if (family != Family::EndPlateSplice && family != Family::EndPlateSpliceOutside && (mi.colDepth <= 0.0 || mi.colBf <= 0.0 || mi.colTf <= 0.0 || mi.colTw <= 0.0 || mi.colFy <= 0.0)) {
        r.message = "Rigidez não calculável: informe ou confirme a seção do pilar e suas propriedades geométricas e mecânicas.";
        return r;
    }
    r.stiffness = JC::computeStiffness(mi);
    if (!r.stiffness.ok) {
        r.message = "Rigidez não calculável: " + r.stiffness.error;
        return r;
    }
    r.resistance = JC::computeResistance(mi);
    if (family == Family::EndPlateSpliceOutside) {
        // INCREMENTO 6 — composicao em serie: (A) mesas da viga soldadas ao pilar (EN 6.2.7.1; k1, k2, k3 do pilar) e (B) emenda por chapas de extremidade (acima).
        // Rotacao total = soma das rotacoes (1/S = 1/SA + 1/SB); Mj,Rd = min(MA, MB). O momento na emenda e tomado igual ao da face do pilar (o termo V x distancia nao existe aqui).
        const D::EndPlateInput colIn = ConnectionService::endPlateInput(columnSource);
        JC::ModelInput wi;
        wi.youngModulus = mi.youngModulus;
        wi.kind = JC::ModelInput::WeldedFlanges;
        wi.beamDepth = mi.beamDepth;
        wi.beamBf = mi.beamBf;
        wi.beamTf = mi.beamTf;
        wi.beamTw = mi.beamTw;
        wi.beamFy = mi.beamFy;
        wi.colDepth = colIn.colDepth;
        wi.colBf = colIn.colFlangeWidth;
        wi.colTf = colIn.colFlangeThickness;
        wi.colTw = colIn.colWebThickness;
        wi.colRadius = colIn.colRadius;
        wi.colFy = colIn.column.fy;
        const bool fillet = c.values.value(QStringLiteral("supWeldKind"), 0.0) >= 0.5;
        wi.flangeThroat = fillet ? c.values.value(QStringLiteral("supWeldLegF"), 0.0) / std::sqrt(2.0) : 0.0;   // penetracao total: ap = 0 (conservador, como na familia Moment)
        wi.beta = colIn.beamOnBothSides ? 0.0 : 1.0;
        wi.webStiffened = colIn.continuityPlates;
        wi.doublerThickness = colIn.doublerThickness;
        wi.gammaYield = colIn.gammaA1;
        if (wi.colDepth <= 0.0 || wi.colBf <= 0.0 || wi.colTf <= 0.0 || wi.colTw <= 0.0 || wi.colFy <= 0.0) {
            r.message = "Rigidez não calculável: a parte soldada ao pilar exige a seção do pilar informada ou confirmada.";
            return r;
        }
        const JC::StiffnessResult wst = JC::computeStiffness(wi);
        const JC::Resistance wres = JC::computeResistance(wi);
        if (!wst.ok || !wres.ok || !r.stiffness.ok || !r.resistance.ok) {
            r.message = "Ligação fora do nó não calculável: " + (!wst.ok ? "parte soldada ao pilar: " + wst.error : !wres.ok ? "parte soldada ao pilar: " + wres.error : !r.stiffness.ok ? "emenda: " + r.stiffness.error : "emenda: " + r.resistance.error);
            return r;
        }
        const double sA = wst.initialStiffnessKnm(), sB = r.stiffness.initialStiffnessKnm();
        const double mA = wres.momentResistanceKnm(), mB = r.resistance.momentResistanceKnm();
        const double sTot = 1.0 / (1.0 / sA + 1.0 / sB);
        r.parts = {{"mesas da viga soldadas ao pilar", sA, mA, wres.capComponent}, {"emenda por duas chapas de extremidade", sB, mB, r.resistance.capComponent}};
        std::vector<JC::Component> merged;
        for (JC::Component comp : wst.components) {
            comp.name = "Parte soldada ao pilar — " + comp.name;
            comp.row = 0;
            merged.push_back(comp);
        }
        for (const JC::Component &comp : r.stiffness.components) {
            merged.push_back(comp);
        }
        r.stiffness.components = merged;
        r.stiffness.lines.push_back("");
        r.stiffness.lines.push_back("COMPOSIÇÃO EM SÉRIE (ligação fora do nó): as rotações das duas partes se somam");
        r.stiffness.lines.push_back(fmt("  Parte A (mesas soldadas ao pilar): Sj,ini,A = %.1f kN.m/rad;  Parte B (emenda): Sj,ini,B = %.1f kN.m/rad", sA, sB));
        r.stiffness.lines.push_back(fmt("  1/Sj,ini = 1/Sj,ini,A + 1/Sj,ini,B  ->  Sj,ini = %.1f kN.m/rad", sTot));
        r.stiffness.initialStiffness = sTot * 1.0e6;
        r.resistance.lines.push_back("");
        r.resistance.lines.push_back(fmt("COMPOSIÇÃO (ligação fora do nó): Mj,Rd = min(Mj,Rd,A = %.2f ; Mj,Rd,B = %.2f) kN.m", mA, mB));
        if (mA < mB) {
            r.resistance.momentResistance = mA * 1.0e6;
            r.resistance.capComponent = "ligação soldada ao pilar: " + wres.capComponent;
        } else {
            r.resistance.momentResistance = mB * 1.0e6;
            r.resistance.capComponent = "emenda: " + r.resistance.capComponent;
        }
        r.hypotheses.push_back("Ligação fora do nó: composição em SÉRIE de (A) mesas da viga soldadas ao pilar (EN 1993-1-8, 6.2.7.1 e 6.3.2: k1, k2, k3 do pilar) e (B) emenda por duas chapas de extremidade (Tabela 6.10). "
                               "A rotação total é a soma das rotações das duas partes e o momento resistente é o menor dos dois. É uma composição por cinemática de molas em série, não uma regra literal da EN.");
        r.hypotheses.push_back("O momento na emenda é tomado igual ao da face do pilar (a parcela cortante × distância da junta ao pilar não é considerada); a solda viga–pilar das mesas é a cadastrada (penetração total ou filetes).");
    }
    std::vector<std::string> notes;

    std::string governingNbr;
    const double nbrNmm = momentResistance(ratiosAt, notes, governingNbr);
    r.momentResistanceEnKnm = r.resistance.ok ? r.resistance.momentResistanceKnm() : 0.0;
    r.momentResistanceNbrKnm = nbrNmm > 0.0 ? nbrNmm * 1.0e-6 : 0.0;
    r.memory = r.stiffness.lines;
    r.memory.push_back("");
    for (const std::string &l : r.resistance.lines) {
        r.memory.push_back(l);
    }
    if (!r.resistance.ok && !r.resistance.error.empty()) {
        r.memory.push_back("Resistência por componentes indisponível: " + r.resistance.error);
    }
    r.memory.push_back("");
    r.memory.push_back("MOMENTO RESISTENTE DE CÁLCULO Mj,Rd");
    r.nbrCheckAvailable = r.momentResistanceNbrKnm > 0.0;
    if (r.momentResistanceEnKnm > 0.0) {
        r.memory.push_back(fmt("  NÍVEL A (comportamento mecânico, usado na curva): método dos componentes, EN 1993-1-8, 6.2.7: Mj,Rd = %.2f kN.m", r.momentResistanceEnKnm));
    }
    if (r.nbrCheckAvailable) {
        r.memory.push_back(fmt("  NÍVEL C (verificação construtiva da ligação cadastrada, NBR 8800:2024, V = N = 0): Mj,Rd = %.2f kN.m", r.momentResistanceNbrKnm) + (governingNbr.empty() ? "" : "  (governa: " + governingNbr + ")"));
    }
    for (const auto &n : notes) {
        r.memory.push_back("  Aviso: " + n);
    }
    double mjRd = 0.0;
    if (r.momentResistanceEnKnm > 0.0) {
        mjRd = r.momentResistanceEnKnm;
        if (r.nbrCheckAvailable && r.momentResistanceNbrKnm < r.momentResistanceEnKnm * (1.0 - 1e-9)) {
            r.nbrBelowCurve = true;
            r.memory.push_back(fmt("  ATENÇÃO (nível C): a ligação cadastrada resiste, pela NBR, a %.2f kN.m, menos que o Mj,Rd da curva (%.2f kN.m): ela NÃO sustenta o momento da curva. "
                                   "Redimensione a ligação (etapa posterior) antes de adotar a curva como representativa.",
                                   r.momentResistanceNbrKnm, r.momentResistanceEnKnm));
        }
    } else if (r.nbrCheckAvailable) {
        mjRd = r.momentResistanceNbrKnm;
        r.memory.push_back(fmt("  O cálculo por componentes não foi possível: Mj,Rd da curva = verificação da NBR = %.2f kN.m", mjRd));
    }
    r.behavior.origin = JS::DataOrigin::ComponentMethod;
    if (mjRd <= 0.0) {
        r.message = "Mj,Rd não determinado";
        for (const auto &n : notes) {
            r.message += "; " + n;
        }
        r.behavior.description = "Método dos componentes (EN 1993-1-8): " + r.family + "; Mj,Rd indisponível.";
        (negative ? r.behavior.negative : r.behavior.positive).stiffness = r.stiffness.initialStiffnessKnm();
        r.supported = true;
        return r;
    }
    r.momentResistanceKnm = mjRd;
    r.metrics = JC::curveMetrics(r.stiffness.initialStiffnessKnm(), mjRd, JC::kPsiWeldedAndEndPlate);
    r.memory.push_back("");
    r.memory.push_back("CURVA MOMENTO-ROTAÇÃO — EN 1993-1-8, 6.3.1 (eq. 6.28)");
    r.memory.push_back("  theta = M / Sj,ini                              para M <= 2/3 Mj,Rd");
    r.memory.push_back("  theta = M / Sj,ini x (1,5 M / Mj,Rd)^psi         para 2/3 Mj,Rd < M <= Mj,Rd,  psi = 2,7 (Tabela 6.8)");
    r.memory.push_back("  Domínio de validade: M <= Mj,Rd, V e N pequenos (N <= 5% Npl,Rd, 6.2.7.1(2)). A capacidade de rotação não é calculada: a curva termina em Mj,Rd.");
    r.memory.push_back(fmt("  Início da perda de rigidez: M = 2/3 Mj,Rd = %.2f kN.m (theta = %.5f rad)", r.metrics.onsetOfNonlinearity, r.metrics.rotationAtOnset));
    r.memory.push_back(fmt("  Rigidez secante a Sj,ini/2: M = %.2f kN.m (theta = %.5f rad)", r.metrics.momentAtHalfStiffness, r.metrics.rotationAtHalfStiffness));
    r.memory.push_back(fmt("  Em Mj,Rd: theta = %.5f rad;  secante = %.0f kN.m/rad;  tangente = %.0f kN.m/rad", r.metrics.rotationAtResistance, r.metrics.secantAtResistance, r.metrics.tangentAtResistance));
    r.behavior.description = "Método dos componentes (EN 1993-1-8, 6.2 e 6.3): " + r.family + ". Sj,ini = " + fmt("%.0f", r.stiffness.initialStiffnessKnm())
                             + " kN·m/rad; Mj,Rd = " + fmt("%.1f", mjRd) + " kN·m.";
    JS::DirectionBehavior &computed = negative ? r.behavior.negative : r.behavior.positive;
    computed.curve = JC::momentRotationCurve(r.stiffness.initialStiffnessKnm(), mjRd, JC::kPsiWeldedAndEndPlate);
    computed.resistantMoment = mjRd;
    if (family == Family::MomentWelded) {
        if (!negative) {
            r.behavior.negative = r.behavior.positive;   // ligacao simetrica
        }
    } else if (!negative) {
        r.message = "Sentido inverso (fileiras de parafusos na outra mesa) não calculado: a chapa de extremidade só tem fileiras tracionadas na extensão considerada.";   // o sentido inverso vem de evaluate(c, E, Negative)
    }
    r.supported = true;
    return r;
}

JointComponentService::Result JointComponentService::evaluatePreliminary(const PreliminaryMembers &m, const PreliminarySettings &set)
{
    Result r;
    r.preliminary = true;
    if (m.beamDepth <= 0.0 || m.beamBf <= 0.0 || m.beamTf <= 0.0 || m.beamTw <= 0.0 || m.colDepth <= 0.0 || m.colBf <= 0.0 || m.colTf <= 0.0 || m.colTw <= 0.0
        || m.beamDepth <= 2.0 * m.beamTf || m.colDepth <= 2.0 * m.colTf) {
        r.message = "Modelo preliminar indisponível: as duas barras precisam ser perfis I/H com dimensões conhecidas (d, bf, tf, tw).";
        return r;
    }
    JC::ModelInput mi;
    mi.youngModulus = m.youngModulusMPa > 0.0 ? m.youngModulusMPa : 200000.0;
    mi.beamDepth = m.beamDepth;
    mi.beamBf = m.beamBf;
    mi.beamTf = m.beamTf;
    mi.beamTw = m.beamTw;
    mi.beamFy = m.beamFy;
    mi.colDepth = m.colDepth;
    mi.colBf = m.colBf;
    mi.colTf = m.colTf;
    mi.colTw = m.colTw;
    const bool radiusKnown = m.colRadius > 0.0;
    mi.colRadius = radiusKnown ? m.colRadius : 0.8 * m.colTf;
    mi.colFy = m.colFy;
    mi.beta = set.beta;
    mi.webStiffened = set.webStiffened;
    mi.doublerThickness = set.doublerThickness;
    mi.gammaYield = 1.10;
    r.hypotheses.push_back("MODELO PRELIMINAR: não é uma ligação dimensionada. Serve para conhecer a ordem de grandeza da rigidez e classificar a conexão antes do dimensionamento construtivo.");
    r.hypotheses.push_back("Perfis I/H laminados; momento fletor no plano da alma da viga; viga de um lado do pilar" + std::string(set.beta <= 0.0 ? " (beta = 0: vigas dos dois lados com momentos iguais)." : " (beta = 1)."));
    r.hypotheses.push_back(fmt("Módulo de elasticidade E = %.0f MPa (do material da viga); escoamento da viga %.0f MPa e do pilar %.0f MPa (as chapas e a solda são admitidas com o aço da viga).", mi.youngModulus, m.beamFy, m.colFy));
    if (!radiusKnown) {
        r.hypotheses.push_back("Raio de concordância mesa-alma do pilar desconhecido: adotado 0,8·tf (dc e larguras efetivas dependem dele).");
    }
    if (set.kind == 0) {
        mi.kind = JC::ModelInput::WeldedFlanges;
        mi.flangeThroat = 0.0;
        r.family = "mesas da viga soldadas ao pilar (referência)";
        r.hypotheses.push_back("Mesas soldadas ao pilar com solda de penetração total (ap = 0, conservador para a largura efetiva); alma sem enrijecedores" + std::string(set.webStiffened ? " (enrijecedores horizontais adotados)." : "."));
    } else {
        mi.kind = JC::ModelInput::ExtendedEndPlate;
        const double db = set.boltDiameter > 0.0 ? set.boltDiameter : 20.0;
        mi.boltDiameter = db;
        const double ab = kPi * db * db / 4.0;
        mi.boltStressArea = 0.78 * ab;
        mi.boltTensionResistance = 0.9 * 800.0 * mi.boltStressArea / 1.25;   // parafuso classe 8.8 (EN 1993-1-8, Tabela 3.4)
        mi.plateThickness = set.plateThickness > 0.0 ? set.plateThickness : std::clamp(db, 12.0, 32.0);
        mi.plateFy = m.beamFy;
        mi.weldLeg = std::clamp(std::round(0.75 * m.beamTf), 6.0, 12.0);
        mi.flangeThroat = mi.weldLeg / std::sqrt(2.0);
        mi.plateWidth = m.beamBf + 20.0;
        mi.gauge = std::min(std::max(0.6 * m.beamBf, 60.0), mi.plateWidth - 40.0);
        mi.tensionRows = set.tensionRows > 0 ? std::min(set.tensionRows, 3) : 1;
        mi.firstRow = 40.0;
        mi.pitch = 70.0;
        mi.edgeVertical = 35.0;
        mi.plateBelowFlange = 150.0;
        r.family = "chapa de extremidade estendida (referência)";
        r.hypotheses.push_back(fmt("Configuração de referência ADOTADA por regras (não dimensionada): parafusos M%.0f classe 8.8; chapa de espessura %.0f mm e largura = bf + 20 = %.0f mm; gabarito w = %.0f mm.", db, mi.plateThickness, mi.plateWidth, mi.gauge));
        r.hypotheses.push_back(fmt("%.0f fileira(s) de 2 parafusos na extensão tracionada, a 40 mm da face externa da mesa (passo 70 mm); solda mesa-chapa de filete de perna %.0f mm.", static_cast<double>(mi.tensionRows), mi.weldLeg));
        r.hypotheses.push_back("Só o sentido de momento que traciona as fileiras de parafusos é representado.");
    }
    r.hypotheses.push_back("Comprimentos efetivos de fileira isolada (efeito de grupo não calculado); a curva é a da EN 1993-1-8, 6.3.1, e termina em Mj,Rd do modelo de referência.");
    r.hypotheses.push_back("Resistência por componentes (EN 1993-1-8, 6.2) com gamma de escoamento 1,10. NÃO há verificação de parafusos, soldas, chapas e estabilidade da NBR 8800 (nível C): essa etapa é posterior.");
    {
        QByteArray data = QByteArray::number(set.kind) + '|' + QByteArray::number(set.plateThickness, 'g', 8) + '|' + QByteArray::number(set.boltDiameter, 'g', 8) + '|'
                          + QByteArray::number(set.tensionRows) + '|' + QByteArray::number(set.beta, 'g', 6) + (set.webStiffened ? 'S' : 'N') + QByteArray::number(set.doublerThickness, 'g', 8) + '|';
        for (const double v : {m.beamDepth, m.beamBf, m.beamTf, m.beamTw, m.beamFy, m.colDepth, m.colBf, m.colTf, m.colTw, m.colRadius, m.colFy, m.youngModulusMPa}) {
            data += QByteArray::number(v, 'g', 10) + ';';
        }
        r.fingerprint = "P" + QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(11).toStdString();
    }
    r.stiffness = JC::computeStiffness(mi);
    if (!r.stiffness.ok) {
        r.message = "Rigidez não calculável com a configuração de referência: " + r.stiffness.error;
        return r;
    }
    r.resistance = JC::computeResistance(mi);
    r.momentResistanceEnKnm = r.resistance.ok ? r.resistance.momentResistanceKnm() : 0.0;
    r.memory.push_back("MODELO PRELIMINAR (nível A) — hipóteses e domínio de validade");
    for (const std::string &h : r.hypotheses) {
        r.memory.push_back("  - " + h);
    }
    r.memory.push_back("");
    for (const std::string &l : r.stiffness.lines) {
        r.memory.push_back(l);
    }
    r.memory.push_back("");
    for (const std::string &l : r.resistance.lines) {
        r.memory.push_back(l);
    }
    r.behavior.origin = JS::DataOrigin::PreliminaryModel;
    r.supported = true;
    if (r.momentResistanceEnKnm <= 0.0) {
        r.message = "Mj,Rd do modelo de referência não determinado: apenas a rigidez inicial é conhecida.";
        r.behavior.description = "Modelo preliminar: " + r.family + "; só Sj,ini.";
        r.behavior.positive.stiffness = r.stiffness.initialStiffnessKnm();
        return r;
    }
    r.momentResistanceKnm = r.momentResistanceEnKnm;
    r.metrics = JC::curveMetrics(r.stiffness.initialStiffnessKnm(), r.momentResistanceKnm, JC::kPsiWeldedAndEndPlate);
    r.memory.push_back("");
    r.memory.push_back("CURVA MOMENTO-ROTAÇÃO (EN 1993-1-8, 6.3.1): theta = M/Sj,ini até 2/3 Mj,Rd; depois theta = M/Sj,ini x (1,5 M/Mj,Rd)^2,7, até Mj,Rd.");
    r.memory.push_back(fmt("  Perda de rigidez a partir de %.2f kN.m (2/3 Mj,Rd); em Mj,Rd: secante %.0f kN.m/rad, tangente %.0f kN.m/rad.", r.metrics.onsetOfNonlinearity, r.metrics.secantAtResistance, r.metrics.tangentAtResistance));
    r.behavior.description = "MODELO PRELIMINAR (" + r.family + "; não dimensionado): Sj,ini = " + fmt("%.0f", r.stiffness.initialStiffnessKnm()) + " kN·m/rad; Mj,Rd do modelo de referência = " + fmt("%.1f", r.momentResistanceKnm) + " kN·m.";
    r.behavior.positive.curve = JC::momentRotationCurve(r.stiffness.initialStiffnessKnm(), r.momentResistanceKnm, JC::kPsiWeldedAndEndPlate);
    r.behavior.positive.resistantMoment = r.momentResistanceKnm;
    if (set.kind == 0) {
        r.behavior.negative = r.behavior.positive;
    } else {
        r.message = "Sentido inverso (fileiras de parafusos na outra mesa) não calculado.";
    }
    return r;
}

} // namespace Structura::App::Services
