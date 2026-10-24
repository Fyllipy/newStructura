#pragma once

#include "shell/ShellModel.h"
#include <QString>
#include <QStringList>
#include <QMap>
#include <algorithm>
#include <array>
#include <cmath>
#include <QPair>
#include <QUuid>
#include <QVector>

#include <optional>

namespace Structura::App::ProjectData {

struct MaterialInfo {
    QUuid uuid;
    int externalId {0};
    QString name;
    double youngModulus {0.0};
    double shearModulus {0.0};
    double unitWeight {0.0}; // kN/m3, usado apenas para peso proprio
    QString library;         // entrada da biblioteca de materiais (aco/concreto); vazio = personalizado
    double fy {0.0};         // MPa (aco) ou fck (concreto); 0 = nao informado
    double fu {0.0};         // MPa (aco)
};

struct SectionInfo {
    QUuid uuid;
    int externalId {0};
    QString name;
    double area {0.0};
    double iz {0.0};
    double iy {0.0};
    double j {0.0};
    // Geometria usada apenas na exibicao extrudada (nao afeta o solver).
    QString family;                 ///< Familia de perfil (ex.: "W Laminado"); vazio = sem geometria
    QMap<QString, double> geometry; ///< Dimensoes em mm
    bool mirrored {false};
};

struct NodeSupport {
    int nodeId {0};
    bool restraints[6] {false, false, false, false, false, false};
};

struct NodalLoad {
    int nodeId {0};
    double fx {0.0};
    double fy {0.0};
    double fz {0.0};
    double mx {0.0};
    double my {0.0};
    double mz {0.0};
    QString loadCase; ///< Caso de carga ao qual a carga pertence.
};

struct MemberLoad {
    int memberId {0};
    QString system;
    double qx {0.0};
    double qy {0.0};
    double qz {0.0};
    QString loadCase;
};

/// Momento uniformemente distribuido ao longo da barra (N*m/m).
struct MemberMoment {
    int memberId {0};
    QString system;
    double mx {0.0};
    double my {0.0};
    double mz {0.0};
    QString loadCase;
};

/// Ponto de referencia da secao que fica sobre o eixo (no) da barra. Numeracao no
/// retangulo envolvente da secao, vista do no I para o J (a = largura/eixo z, b = altura/eixo y):
///   3 -- 7 -- 4        0 = centro (centroide)
///   |    |    |        1 = inferior esquerdo   2 = inferior direito
///   8 -- 0 -- 6        3 = superior esquerdo   4 = superior direito
///   |    |    |        5 = meio inferior       6 = meio direito
///   1 -- 5 -- 2        7 = meio superior       8 = meio esquerdo
enum class SectionReference : int {
    Center = 0,
    BottomLeft = 1,
    BottomRight = 2,
    TopLeft = 3,
    TopRight = 4,
    BottomMiddle = 5,
    RightMiddle = 6,
    TopMiddle = 7,
    LeftMiddle = 8
};

/// Liberacoes de extremidade: bit i (0-5) = ux uy uz rx ry rz no I; bit 6+i = o mesmo no J (eixos locais).
namespace MemberRelease {
constexpr int kStartRx = 1 << 3;
constexpr int kStartRy = 1 << 4;
constexpr int kStartRz = 1 << 5;
constexpr int kEndRx = 1 << 9;
constexpr int kEndRy = 1 << 10;
constexpr int kEndRz = 1 << 11;
} // namespace MemberRelease

/// Tipo funcional da barra: define articulacoes e comportamento axial.
namespace MemberType {
constexpr int kNormal = 0;   ///< viga/pilar (ligacoes conforme as liberacoes)
constexpr int kTruss = 1;    ///< trelica: biarticulada (so esforco axial), traciona e comprime
constexpr int kTie = 2;      ///< tirante: biarticulado e so trabalha a tracao (desativa ao encurtar)
constexpr int kReleaseRotations = MemberRelease::kStartRy | MemberRelease::kStartRz | MemberRelease::kEndRy | MemberRelease::kEndRz;
} // namespace MemberType

/// Posicionamento da secao em relacao ao eixo da barra e vinculos de extremidade (por barra, chave = ID externo).
struct MemberPlacement {
    int memberId {0};
    double rotationDeg {0.0};   ///< Giro da secao em torno do eixo x local (regra da mao direita).
    int reference {0};          ///< SectionReference: ponto da secao que fica sobre o no.
    double offsetA {0.0};       ///< m: deslocamento adicional da secao ao longo da largura (eixo local z), extremidade I.
    double offsetB {0.0};       ///< m: deslocamento adicional da secao ao longo da altura (eixo local y), extremidade I.
    bool endDiffers {false};    ///< Extremidade J com deslocamento proprio (`offsetA2/B2`); senao igual a I.
    double offsetA2 {0.0};
    double offsetB2 {0.0};
    int releaseMask {0};        ///< MemberRelease: graus de liberdade liberados (articulacoes).
    int memberType {0};         ///< MemberType: normal, trelica ou tirante.

    [[nodiscard]] double endOffsetA() const { return endDiffers ? offsetA2 : offsetA; }
    [[nodiscard]] double endOffsetB() const { return endDiffers ? offsetB2 : offsetB; }

    [[nodiscard]] bool isDefault() const
    {
        return rotationDeg == 0.0 && reference == 0 && offsetA == 0.0 && offsetB == 0.0
            && !endDiffers && releaseMask == 0 && memberType == 0;
    }
    bool operator==(const MemberPlacement &o) const
    {
        return memberId == o.memberId && rotationDeg == o.rotationDeg && reference == o.reference
            && offsetA == o.offsetA && offsetB == o.offsetB && endDiffers == o.endDiffers
            && offsetA2 == o.offsetA2 && offsetB2 == o.offsetB2 && releaseMask == o.releaseMask && memberType == o.memberType;
    }
};

/// Alteracao parcial de um posicionamento (campos ausentes sao preservados; usado em selecoes multiplas).
struct MemberPlacementPatch {
    std::optional<double> rotationDeg;
    std::optional<int> reference;
    std::optional<double> offsetA;
    std::optional<double> offsetB;
    std::optional<bool> endDiffers;
    std::optional<double> offsetA2;
    std::optional<double> offsetB2;
    std::optional<int> releaseMask;
    std::optional<int> memberType;

    void applyTo(MemberPlacement &placement) const
    {
        if (rotationDeg) placement.rotationDeg = *rotationDeg;
        if (reference) placement.reference = std::clamp(*reference, 0, 8);
        if (offsetA) placement.offsetA = *offsetA;
        if (offsetB) placement.offsetB = *offsetB;
        if (endDiffers) placement.endDiffers = *endDiffers;
        if (offsetA2) placement.offsetA2 = *offsetA2;
        if (offsetB2) placement.offsetB2 = *offsetB2;
        if (releaseMask) placement.releaseMask = *releaseMask;
        if (memberType) placement.memberType = std::clamp(*memberType, 0, 2);
    }
};

/// Caso de carga (ex.: permanente, sobrecarga). Os resultados sao calculados por caso e combinados.
struct LoadCase {
    QString name;
    QString abbreviation;
    bool selfWeight {false}; ///< Inclui o peso proprio das barras (gama do material x area da secao).

    bool operator==(const LoadCase &other) const
    {
        return name == other.name && abbreviation == other.abbreviation && selfWeight == other.selfWeight;
    }
};

/// Combinacao linear de casos de carga: soma de fator x resultado de cada caso.
struct LoadCombination {
    QString name;
    QVector<QPair<QString, double>> factors; ///< (nome do caso, fator)
    /// Variante com imperfeicao geometrica inicial (so em tempo de execucao, nao e gravada): direcao 0 = X, 1 = Y (-1 = nenhuma) e sentido +1/-1.
    int imperfectionAxis {-1};
    int imperfectionSign {0};

    bool operator==(const LoadCombination &other) const
    {
        return name == other.name && factors == other.factors;
    }
};

/// Nome do caso de carga criado automaticamente para projetos sem casos definidos.
inline QString defaultLoadCaseName()
{
    return QStringLiteral("CASO1");
}

/// Eixo de referencia nomeado (ex.: "A", "1") em uma coordenada global.
struct ReferenceAxis {
    QString label;
    double value {0.0};

    bool operator==(const ReferenceAxis &other) const
    {
        return label == other.label && value == other.value;
    }
};

/// Plano de eixos (nivel/pavimento): elevacao Z e os eixos X/Y nomeados nessa altura.
struct AxisLevel {
    QString name;
    double elevation {0.0};
    QVector<ReferenceAxis> x;
    QVector<ReferenceAxis> y;

    bool operator==(const AxisLevel &other) const
    {
        return name == other.name && elevation == other.elevation && x == other.x && y == other.y;
    }
};

/// Eixos de referencia organizados por niveis. A direcao Z e definida pelas elevacoes dos niveis.
struct ReferenceAxes {
    QVector<AxisLevel> levels; ///< Ordenados por elevacao

    [[nodiscard]] bool isEmpty() const
    {
        for (const AxisLevel &level : levels) {
            if (!level.x.isEmpty() || !level.y.isEmpty() || std::abs(level.elevation) > 1e-9) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] const AxisLevel *levelAt(double elevation, double tolerance = 1e-6) const
    {
        for (const AxisLevel &level : levels) {
            if (std::abs(level.elevation - elevation) <= tolerance) {
                return &level;
            }
        }
        return nullptr;
    }

    /// Uniao dos eixos X (axis=0) ou Y (axis=1) de todos os niveis, sem valores repetidos.
    [[nodiscard]] QVector<ReferenceAxis> unionAlong(int axis) const
    {
        QVector<ReferenceAxis> result;
        for (const AxisLevel &level : levels) {
            for (const ReferenceAxis &entry : axis == 0 ? level.x : level.y) {
                bool present = false;
                for (const ReferenceAxis &existing : result) {
                    present = present || std::abs(existing.value - entry.value) < 1e-9;
                }
                if (!present) {
                    result.append(entry);
                }
            }
        }
        return result;
    }

    /// Eixos exibidos no plano de construcao cuja normal e `normal` (0=X, 1=Y, 2=Z) na posicao `offset`.
    /// Plano XY: eixos X/Y do nivel nessa elevacao (nivel sem eixos proprios: uniao dos eixos de todos os niveis). Planos XZ/YZ: uniao dos eixos horizontais e os niveis
    /// (nome + elevacao) como eixos verticais.
    void planeAxes(int normal, double offset, QVector<ReferenceAxis> &u, QVector<ReferenceAxis> &v) const
    {
        u.clear();
        v.clear();
        if (normal == 2) {
            const AxisLevel *level = levelAt(offset);
            if (level && (!level->x.isEmpty() || !level->y.isEmpty())) {
                u = level->x;
                v = level->y;
            } else {
                // nivel sem eixos proprios (ou sem nivel nessa elevacao): herda os eixos dos demais, para que apareçam em todos os niveis
                u = unionAlong(0);
                v = unionAlong(1);
            }
            return;
        }
        u = unionAlong(normal == 1 ? 0 : 1);
        for (const AxisLevel &level : levels) {
            v.append({level.name, level.elevation});
        }
    }

    /// Posicoes disponiveis para o plano com normal `normal` (sempre inclui 0), ordenadas.
    [[nodiscard]] QVector<double> planeLevels(int normal) const
    {
        QVector<double> values {0.0};
        if (normal == 2) {
            for (const AxisLevel &level : levels) {
                values.append(level.elevation);
            }
        } else {
            for (const ReferenceAxis &entry : unionAlong(normal == 1 ? 1 : 0)) {
                values.append(entry.value);
            }
        }
        std::sort(values.begin(), values.end());
        QVector<double> unique;
        for (const double value : values) {
            if (unique.isEmpty() || std::abs(unique.last() - value) > 1e-9) {
                unique.append(value);
            }
        }
        return unique;
    }

    bool operator==(const ReferenceAxes &other) const { return levels == other.levels; }
};

/// Parametros de dimensionamento de uma barra (sobrepoe os padroes do projeto). Comprimentos em metros.
struct DesignMemberParams {
    int memberId {0};
    QString steel;              ///< Aco da barra; vazio = aco padrao do projeto.
    double kx {1.0};            ///< Coeficiente de comprimento efetivo, flexao em torno de X (plano da altura)
    double ky {1.0};            ///< idem, em torno de Y
    double kz {1.0};            ///< idem, torcao
    double lx {0.0};            ///< comprimento de flambagem informado (m) para flexao em torno de X; 0 = Kx.L
    double ly {0.0};            ///< idem, em torno de Y
    double lz {0.0};            ///< idem, torcao
    int segments {1};           ///< Trechos com travamento lateral igualmente espacados (Lb = L / trechos)
    double lb {0.0};            ///< Lb informado (m); 0 = usa L / trechos
    bool braced {false};        ///< Mesa comprimida contida continuamente
    bool cbOne {false};         ///< Adota Cb = 1,0
    int oneFlangeBraced {0};    ///< NBR 8800, 5.4.2.4: 0 = nao; 1 = mesa do lado + (superior) com contencao lateral continua; 2 = mesa do lado - (inferior)
    bool forceAwayFromFreeFlange {false};   ///< 5.4.2.4: forcas transversais orientadas no sentido oposto a mesa livre (senao, no sentido da mesa livre)
    double b2 {1.0};            ///< Amplificacao dos efeitos globais (B2)
    double netAreaRatio {1.0};  ///< An / Ag na ligacao
    int deflDenominator {0};    ///< Limite de flecha L/n (0 = automatico: L/350 horizontais, L/250 inclinadas)
    double stiffenerSpacing {0.0};   ///< Distancia entre enrijecedores transversais de alma, a (mm); 0 = sem enrijecedores (kv = 5,34)
    int panelTensionFlange {0};      ///< NBR 14762 Anexo F: 0 = nao, 1 = mesa tracionada ligada a painel (barra biapoiada), 2 = idem (barra continua)
    bool coldWork {false};           ///< NBR 14762 Anexo B: aumento de fy nas dobras (trabalho a frio)
    double bearingLength {0.0};      ///< NBR 14762 Anexo D: comprimento de apoio c (mm) das reacoes de extremidade; 0 = so aviso
    int fatigueCategory {0};         ///< NBR 8800 Anexo H: categoria do detalhe (0 = nao verifica; 1 A, 2 B, 3 B', 4 C, 5 D, 6 E, 7 E', 8 F, 9 G)
    double fatigueCycles {0.0};      ///< numero de ciclos de variacao de tensoes N na vida util
    double openingDiameter {0.0};    ///< NBR 8800 Anexo F: diametro D da abertura circular na alma (mm); 0 = sem abertura
    double openingPosition {0.0};    ///< distancia do centro da abertura ao apoio esquerdo (mm)
    double openingOffset {0.0};      ///< excentricidade do centro da abertura em relacao a meia altura (mm)
    double openingNeighbourD {0.0};  ///< diametro da abertura adjacente (mm); 0 = nao ha
    double openingSpacing {0.0};     ///< distancia entre os centros das duas aberturas, ao longo da viga (mm)
    // Ligacao da barra tracionada (NBR 8800, 5.2.3 a 5.2.5): area liquida pelos furos e coeficiente Ct
    int ctMode {0};                  ///< 0 = Ct = 1,0 e An/Ag informado; 1 = 5.2.5-c: Ct = 1 - ec/lc; 2 = 5.2.5-b (soldas transversais): Ct = Ac/Ag
    double ctEccentricity {0.0};     ///< ec: distancia do centro geometrico da barra (ou da barra ficticia) ao plano de cisalhamento da ligacao (mm)
    double ctLength {0.0};           ///< lc: comprimento da ligacao na direcao da forca (mm)
    double ctConnectedAreaRatio {0.0};   ///< Ac/Ag: area dos elementos conectados / area bruta (limite inferior de Ct em 5.2.5-c; valor de Ct em 5.2.5-b)
    double holeBoltDiameter {0.0};   ///< db dos parafusos da linha de ruptura (mm); 0 = sem furos (usa An/Ag informado)
    int holesInChain {0};            ///< numero de furos na cadeia de ruptura
    double holeThickness {0.0};      ///< espessura do elemento furado (mm)
    double staggerPitch {0.0};       ///< s: espacamento longitudinal entre furos em zigue-zague (mm)
    double staggerGauge {0.0};       ///< g: gabarito transversal entre esses furos (mm)
    int staggerLinks {0};            ///< numero de linhas ligando dois furos (cada uma soma s2/(4g))
    // Barras compostas (NBR 8800, 5.3.6)
    double compositeSpacing {0.0};   ///< l: distancia entre as ligacoes dos perfis (mm); 0 = duas ligacoes intermediarias uniformemente espacadas (l = L/3)
    bool compositeWelded {false};    ///< ligacoes intermediarias soldadas ou com parafusos protendidos (5.3.6.3-b); senao parafusos com aperto normal (5.3.6.3-a)
    // Itens adicionais da NBR 8800:2024 (gravados como pares chave=valor ao fim da linha, so quando diferentes do padrao)
    bool threadedEnds {false};       ///< barra redonda com extremidades rosqueadas (5.2.7): ruptura da parte rosqueada Abe fu/ga2, Abe = 0,75 Ab
    double webWeldLeg {0.0};         ///< perna do filete mesa-alma de perfil soldado (mm); 0 = nao verificar (5.4.5)
    double webWeldFw {485.0};        ///< fw do metal da solda mesa-alma (MPa)
    double flangeLineLoad {0.0};     ///< forca aplicada diretamente na mesa e transmitida a alma pela solda (N/mm), somada ao cisalhamento da flexao (5.4.5)
    double coverB {0.0};             ///< lamela (chapa de reforco sobreposta a mesa): largura (mm); 0 = sem lamela (5.4.4)
    double coverT {0.0};             ///< espessura da lamela (mm)
    double coverStart {0.0};         ///< posicao da extremidade inicial da lamela, a partir do inicio da barra (mm)
    double coverEnd {0.0};           ///< posicao da extremidade final (mm)
    int coverFlanges {2};            ///< lamelas em 1 (mesa superior) ou 2 mesas
    double coverWeldLeg {0.0};       ///< perna do filete da lamela (mm)
    bool coverEndWeld {true};        ///< solda de filete na extremidade da lamela (5.4.4.2)
    bool plasticAnalysis {false};    ///< viga continua com analise rigido-plastica: limite do comprimento destravado entre rotulas (4.11.5)
    // Pilar misto de aco e concreto (NBR 8800:2024, Anexo M): o perfil de aco e o da secao da barra (I/H, tubular retangular ou circular)
    int mixType {0};                 ///< 0 = barra de aco; 1 = I/H totalmente revestido; 2 = I/H parcialmente revestido; 3 = tubular preenchido
    int mixModel {1};                ///< modelo de calculo: 1 (M.5.2), 2 (M.5.3), 3 (M.5.4, so tubulares)
    double mixHc {0.0};              ///< revestimento de concreto: altura hc (mm), na direcao da alma do perfil
    double mixBc {0.0};              ///< largura bc (mm)
    double mixFck {30.0};            ///< fck do concreto (MPa)
    double mixFsk {500.0};           ///< fyk da armadura (MPa)
    double mixBarArea {0.0};         ///< area de cada barra (mm2); 0 = sem armadura
    int mixBarsH {0};                ///< barras em cada face superior/inferior (inclui os cantos)
    int mixBarsV {0};                ///< barras intermediarias em cada face lateral
    double mixCover {0.0};           ///< distancia do centro das barras a face do concreto (mm)
    double mixAlphaE {1.0};          ///< alfa_E do agregado (NBR 6118)
    bool operator==(const DesignMemberParams &o) const
    {
        return memberId == o.memberId && steel == o.steel && kx == o.kx && ky == o.ky && kz == o.kz && lx == o.lx && ly == o.ly && lz == o.lz
            && segments == o.segments && lb == o.lb && braced == o.braced && cbOne == o.cbOne && oneFlangeBraced == o.oneFlangeBraced
            && forceAwayFromFreeFlange == o.forceAwayFromFreeFlange && b2 == o.b2
            && netAreaRatio == o.netAreaRatio && deflDenominator == o.deflDenominator && stiffenerSpacing == o.stiffenerSpacing
            && panelTensionFlange == o.panelTensionFlange && coldWork == o.coldWork && bearingLength == o.bearingLength
            && fatigueCategory == o.fatigueCategory && fatigueCycles == o.fatigueCycles && openingDiameter == o.openingDiameter
            && openingPosition == o.openingPosition && openingOffset == o.openingOffset && openingNeighbourD == o.openingNeighbourD
            && openingSpacing == o.openingSpacing && ctMode == o.ctMode && ctEccentricity == o.ctEccentricity && ctLength == o.ctLength
            && ctConnectedAreaRatio == o.ctConnectedAreaRatio && holeBoltDiameter == o.holeBoltDiameter && holesInChain == o.holesInChain
            && holeThickness == o.holeThickness && staggerPitch == o.staggerPitch && staggerGauge == o.staggerGauge && staggerLinks == o.staggerLinks
            && compositeSpacing == o.compositeSpacing && compositeWelded == o.compositeWelded
            && threadedEnds == o.threadedEnds && webWeldLeg == o.webWeldLeg && webWeldFw == o.webWeldFw && flangeLineLoad == o.flangeLineLoad
            && coverB == o.coverB && coverT == o.coverT && coverStart == o.coverStart && coverEnd == o.coverEnd && coverFlanges == o.coverFlanges
            && coverWeldLeg == o.coverWeldLeg && coverEndWeld == o.coverEndWeld && plasticAnalysis == o.plasticAnalysis
            && mixType == o.mixType && mixModel == o.mixModel && mixHc == o.mixHc && mixBc == o.mixBc && mixFck == o.mixFck && mixFsk == o.mixFsk
            && mixBarArea == o.mixBarArea && mixBarsH == o.mixBarsH && mixBarsV == o.mixBarsV && mixCover == o.mixCover && mixAlphaE == o.mixAlphaE;
    }
};

/// Opcoes globais do dimensionamento do projeto.
struct DesignSettings {
    QString defaultSteel {QStringLiteral("MR250")};
    QString rectTubeStandard {QStringLiteral("AUTO")};   ///< Norma dos tubulares retangulares: AUTO (reconhece laminado x formado a frio pela serie), NBR14762 ou NBR8800
    bool amplifyB1 {true};
    bool slendernessGoverns {true};
    bool secondOrder {false};           ///< Analise de segunda ordem (P-delta global e local) nas combinacoes
    bool stabilityFunctions {false};    ///< 2a ordem pelas funcoes de estabilidade (Livesley): P-delta e P-delta local exatos em cada barra, sem subdividi-las
    bool shearDeformation {false};      ///< Deformacao por cortante (Timoshenko) nas barras, com as areas de cisalhamento das secoes
    bool warpingTorsion {false};        ///< Torcao com empenamento (Vlasov, 7o grau de liberdade) nas barras de secao I/H; senao so Saint-Venant
    double rigidZoneFactor {0.0};       ///< Zonas rigidas nas extremidades das barras: fracao (0 a 1) da penetracao da barra no perfil que a recebe; 0 = desligado
    int loadSteps {0};                  ///< 2a ordem: 0 = iteracao direta com incremento automatico de carga se divergir; n > 1 = sempre em n passos
    double stiffnessReduction {0.8};    ///< Fator aplicado a EA e EI na analise de 2a ordem das combinacoes ultimas (NBR 8800, 4.10.5)
    bool autoB2 {true};                 ///< Calcula B2 por andar (Anexo C) nas analises de 1a ordem
    double rs {0.85};                   ///< Rs do Anexo C (0,85 em porticos, 1,0 nos demais casos)
    bool singleStorey {false};          ///< Galpao/edificacao de um pavimento (limite de topo H/300; senao H/400)
    bool geometricImperfection {false};  ///< Imperfeicao geometrica inicial direta (desaprumo H/n) nas combinacoes ultimas da 2a ordem, no lugar das forcas nocionais
    double imperfectionRatio {1.0 / 333.0};   ///< Desaprumo: deslocamento horizontal = razao x altura acima da base (NBR 8800, 4.9.7.1: H/333)
    bool notionalLoads {false};       ///< Forcas nocionais (NBR 8800, 4.9.7.2): variantes das combinacoes ultimas com 0,3% das cargas gravitacionais
    double notionalFactor {0.003};
    bool groupByFamily {false};         ///< Dimensionamento automatico: mesma familia recebe o mesmo perfil (senao agrupa por perfil atual)
    double targetEta {1.0};             ///< Aproveitamento alvo do dimensionamento automatico
    QStringList combinations;   ///< Combinacoes ultimas consideradas; vazio = todas as que nao comecam por "ELS"
    DesignMemberParams defaults;
    QVector<DesignMemberParams> members;   ///< So barras com parametros diferentes dos padroes.

    [[nodiscard]] DesignMemberParams paramsFor(int memberId) const
    {
        for (const auto &m : members) {
            if (m.memberId == memberId) {
                return m;
            }
        }
        DesignMemberParams p = defaults;
        p.memberId = memberId;
        return p;
    }
    /// A imperfeicao geometrica so existe na 2a ordem (na 1a ordem nao ha P-delta para amplifica-la); quando ativa, substitui as forcas nocionais.
    [[nodiscard]] bool geometricActive() const { return geometricImperfection && secondOrder; }
    [[nodiscard]] bool notionalActive() const { return notionalLoads && !geometricActive(); }
    [[nodiscard]] bool hasImperfectionVariants() const { return notionalActive() || geometricActive(); }
    [[nodiscard]] bool isDefault() const
    {
        return members.isEmpty() && defaultSteel == QLatin1String("MR250") && rectTubeStandard == QLatin1String("AUTO")
            && amplifyB1 && slendernessGoverns && !secondOrder && !stabilityFunctions && !shearDeformation && !warpingTorsion && loadSteps == 0 && rigidZoneFactor == 0.0 && stiffnessReduction == 0.8 && autoB2 && rs == 0.85 && !singleStorey && !notionalLoads && !geometricImperfection && imperfectionRatio == 1.0 / 333.0 && notionalFactor == 0.003 && !groupByFamily && targetEta == 1.0
            && combinations.isEmpty() && defaults == DesignMemberParams {};
    }
};

/// Grupo de barras: perfil, posicionamento e rotacao sao propagados a todos os membros.
struct BarGroup {
    int id {0};
    QString name;
    QVector<int> members;   ///< ids externos das barras (cada barra pertence a no maximo um grupo)
    bool operator==(const BarGroup &o) const { return id == o.id && name == o.name && members == o.members; }
};

/// Pano de carga de area (pressao sobre uma superficie apoiada em barras paralelas), guardado para exibicao e exclusao.
struct AreaLoadPanel {
    int id {0};
    QString loadCase;
    QString kind {QStringLiteral("AREA")};   ///< "AREA" ou "WIND"
    double pressure {0.0};                   ///< kN/m2 (com sinal) no sentido de `direction`
    std::array<double, 3> direction {0.0, 0.0, -1.0};
    QVector<std::array<double, 3>> polygon;  ///< contorno da superficie (m)
    QVector<int> members;                    ///< barras que recebem a carga (ids externos)
    QString description;
};

/// Dados do projeto que nao pertencem ao grafo de cena nem ao solver base.
/// Resultados gravados no .dat para reabrir o projeto sem refazer a analise ([ANALYSIS_RESULTS], [DESIGN_RESULTS], ...).
struct StoredNodeResult {
    int nodeId {0};
    std::array<double, 6> displacements {};
    std::array<double, 6> reactions {};
};
struct StoredMemberResult {
    int memberId {0};
    std::array<double, 12> forces {};
    std::array<double, 3> auxK {};
    bool hasWarping {false};                ///< torcao com empenamento: theta' nas extremidades e forcas generalizadas (bimomentos)
    std::array<double, 2> warpingRate {};
    std::array<double, 2> warpingForce {};
};
struct StoredShellResult {
    int shellId {0};
    std::array<double, 40> values {};
    std::array<double, 9> axes {};
};
struct StoredRun {
    QString name;   ///< caso de carga ou "COMB:<combinacao>" (2a ordem)
    QVector<StoredNodeResult> nodes;
    QVector<StoredMemberResult> members;
    QVector<StoredNodeResult> shellNodes;       ///< nos proprios das cascas acopladas
    QVector<StoredShellResult> shellElements;   ///< resultantes dos elementos de casca
};
struct StoredDesignResult {
    int memberId {0};
    QString section;
    double eta {0.0};
    QString check;        ///< verificacao governante
    QString combination;
    double lengthM {0.0};
};
struct StoredResults {
    QString fingerprint;   ///< impressao digital do modelo analisado; resultados so sao restaurados se o modelo nao mudou
    QVector<StoredRun> runs;
    QVector<StoredDesignResult> design;
    QString bucklingCombination;
    QVector<double> bucklingFactors;
    QString modalSource;
    QVector<double> modalOmega;
    QVector<std::array<double, 3>> modalMassRatio;

    [[nodiscard]] bool isEmpty() const
    {
        return runs.isEmpty() && design.isEmpty() && bucklingFactors.isEmpty() && modalOmega.isEmpty();
    }
};

/// Ligacao viga-apoio associada a um no do modelo (chapa de cisalhamento ou cantoneira dupla).
struct NodeConnection {
    static constexpr int kShearTab = 0;
    static constexpr int kDoubleAngle = 1;
    static constexpr int kGusset = 2;      ///< chapa de no de trelica/contraventamento
    static constexpr int kSplice = 3;      ///< emenda de viga parafusada
    static constexpr int kBasePlate = 4;   ///< base de pilar com chumbadores
    static constexpr int kTubular = 5;     ///< ligacao soldada de trelica tubular
    static constexpr int kColdFormed = 6;  ///< ligacao de perfil formado a frio (NBR 14762, secao 10)
    static constexpr int kEndPlate = 7;    ///< ligacao rigida viga-pilar por chapa de extremidade

    int id {0};
    int nodeId {0};            ///< id externo do no
    int memberId {0};          ///< viga ligada (id externo)
    bool atEnd {false};        ///< true = extremidade final (J) da viga neste no
    int type {kShearTab};
    QString grade;             ///< parafuso (nome em Connections::boltGrades)
    QMap<QString, double> values;   ///< geometria e acos (chaves definidas em ConnectionService)
    // ultimo resultado (apenas informativo; recalculado ao verificar)
    double eta {0.0};
    QString governing;
    bool verified {false};
};

/// Carimbo das pranchas: campos editaveis, tabela de revisoes e titulos personalizados das pranchas.
struct TitleBlockSettings {
    struct Revision {
        QString rev;
        QString date;
        QString description;
        QString author;
    };

    QString company;
    QString client;
    QString site;          ///< obra / local
    QString project;
    QString author;        ///< responsavel tecnico
    QString registration;  ///< CREA / CAU
    QString drafter;       ///< desenhista
    QString checker;       ///< verificador
    QString date;
    QString revision {QStringLiteral("0")};
    QString drawingPrefix; ///< prefixo do numero das pranchas (ex.: EST)
    QString notes;         ///< notas gerais (rodape esquerdo das pranchas)
    QVector<Revision> revisions;
    QMap<QString, QString> sheetTitles;   ///< id da vista -> titulo personalizado

    [[nodiscard]] bool isDefault() const
    {
        return company.isEmpty() && client.isEmpty() && site.isEmpty() && project.isEmpty() && author.isEmpty() && registration.isEmpty()
            && drafter.isEmpty() && checker.isEmpty() && date.isEmpty() && revision == QLatin1String("0") && drawingPrefix.isEmpty()
            && notes.isEmpty() && revisions.isEmpty() && sheetTitles.isEmpty();
    }
};

/// Vista 3D salva para as pranchas (desenho em linhas): angulos ou camera explicita (posicao em m).
struct SavedView3D {
    QString name;
    bool perspective {false};
    double azimuthDeg {35.0};     ///< 0 = vista frontal (olho em -Y); cresce para o lado de +X
    double elevationDeg {25.0};
    double fovDeg {30.0};
    bool explicitCamera {false};  ///< usa posicao / alvo / cima abaixo (camera do modelo no momento da captura)
    std::array<double, 3> position {0.0, 0.0, 0.0};
    std::array<double, 3> target {0.0, 0.0, 0.0};
    std::array<double, 3> up {0.0, 0.0, 1.0};
};

/// Folha de desenho montada pelo usuario: varias vistas, cada uma com escala e (opcionalmente) posicao propria.
struct SheetLayoutDef {
    struct Placement {
        QString viewId;
        double scale {0.0};      ///< denominador da escala; 0 = automatica
        bool manual {false};     ///< true: usa o retangulo abaixo (fracoes da area util, origem no canto superior esquerdo)
        double x {0.0}, y {0.0}, w {0.5}, h {0.5};
    };
    QString name;
    QVector<Placement> views;
};

struct ProjectExtras {
    QVector<MemberMoment> memberMoments;
    QVector<MemberPlacement> placements; ///< So barras com posicionamento nao padrao.
    QVector<LoadCase> loadCases;
    QVector<LoadCombination> combinations;
    QString activeLoadCase; ///< Caso exibido no viewport; vazio = todos.
    ReferenceAxes axes;
    /// Opcoes de exibicao persistidas no .dat ([VIEW_OPTIONS]); booleanos como 0/1.
    QMap<QString, double> viewOptions;
    DesignSettings design;
    QVector<BarGroup> groups;
    QVector<AreaLoadPanel> areaLoads;
    StoredResults results;
    QVector<NodeConnection> connections;
    TitleBlockSettings titleBlock;
    QVector<SavedView3D> views3d;
    QVector<SheetLayoutDef> sheetLayouts;   ///< folhas compostas das pranchas
    QMap<int, QMap<QString, double>> compositeBeams;   ///< entradas da viga mista por barra (id externo)
    QMap<QString, double> deckSlab;                    ///< entradas da laje mista (steel deck)
    Structura::Shell::Model shell;                     ///< cascas e chapas (placas, tanques, vasos de pressao)
};

struct NodalLoadPreset {
    double fx {0.0};
    double fy {0.0};
    double fz {0.0};
    double mx {0.0};
    double my {0.0};
    double mz {0.0};
};

struct DistributedLoadPreset {
    QString system {QStringLiteral("GLOBAL")};
    double qx {0.0};
    double qy {0.0};
    double qz {0.0};
};

} // namespace Structura::App::ProjectData
