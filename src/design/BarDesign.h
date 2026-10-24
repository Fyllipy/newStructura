#pragma once

#include "design/DesignSection.h"
#include "design/DesignTypes.h"
#include "design/Nbr14762.h"
#include "design/Nbr8800.h"

#include <string>
#include <vector>

namespace Structura::Design {

/// Esforcos solicitantes de calculo em uma secao. Convencao: N > 0 e tracao; Mx > 0 comprime a fibra +b
/// (topo); My > 0 comprime a fibra +a; Vy e a cortante paralela a b (alma), Vz a paralela a a.
struct Effects {
    double N {0.0};
    double Mx {0.0};
    double My {0.0};
    double Vy {0.0};
    double Vz {0.0};
    double T {0.0};
    double B {0.0};     ///< bimomento (N*mm2): torcao com empenamento; B = -E Cw theta'', positivo no sentido da convencao da face positiva
    double Tw {0.0};    ///< parcela do torque resistida por empenamento (N*mm); a de Saint-Venant e T - Tw
};

struct StationEffects {
    double x {0.0};      ///< posicao ao longo da barra (mm)
    Effects e;           ///< esforcos da analise de 1a ordem da estrutura original (nt + lt)
    Effects nt;          ///< esforcos da estrutura "nt" (nos impedidos de se deslocar horizontalmente, Anexo C.2.1); valido se `hasNt`
};

/// Combinacao ultima: esforcos nas secoes de verificacao e informacao sobre cargas transversais (para Cm).
struct ComboEffects {
    std::string name;
    std::vector<StationEffects> stations;   ///< ordenadas por x, de 0 a L
    bool transverseLoadX {false};           ///< ha cargas transversais no plano de flexao em torno de X (plano x-y local)
    bool transverseLoadY {false};
    double b2 {1.0};                        ///< amplificacao global B2 desta combinacao (Anexo C)
    /// Esforcos separados em estrutura nt e lt (Anexo C.2.1): M_Sd = B1 M_nt + B2 M_lt e N_Sd = N_nt + B2 N_lt, com lt = original - nt.
    /// Sem separacao (`hasNt` falso), B1 e B2 amplificam o momento total e a forca axial nao e amplificada.
    bool hasNt {false};
    /// Fator de reducao da rigidez (imperfeicoes de material, 4.10.7.1.2) aplicado a Ne de B1 nas estruturas de media deslocabilidade (1,0 = nominal).
    double bStiffness {1.0};
};

struct BarDesignInput {
    const DesignSection *section {nullptr};
    SteelGrade steel;
    double length {0.0};                    ///< mm
    double Kx {1.0}, Ky {1.0}, Kz {1.0};    ///< coeficientes de flambagem (comprimento efetivo = K L)
    double lxOverride {0.0}, lyOverride {0.0}, lzOverride {0.0};   ///< >0: comprimento de flambagem informado (mm), substitui K L
    int lateralSegments {1};                ///< numero de trechos destravados da mesa comprimida (>= 1)
    bool compressionFlangeBraced {false};   ///< mesa comprimida contida continuamente (sem FLT)
    double lbOverride {0.0};                ///< >0: comprimento destravado informado (mm)
    bool cbEqualsOne {false};               ///< Cb = 1,0 (adotado pelo usuario)
    bool cantilever {false};                ///< balanco com a extremidade livre: Cb = 1,0 (5.4.2.3-b; limite inferior seguro nos demais casos de balanco)
    int oneFlangeBraced {0};                ///< 5.4.2.4: 0 = nao; 1 = mesa do lado + com contencao lateral continua; 2 = mesa do lado -
    bool forceAwayFromFreeFlange {false};   ///< 5.4.2.4: forcas transversais no sentido oposto a mesa livre
    bool amplifyB1 {true};                  ///< amplificacao local B1 (Anexo C)
    double b2 {1.0};                        ///< amplificacao global B2 aplicada aos momentos
    bool slendernessGoverns {true};         ///< limite de esbeltez entra no aproveitamento
    double netAreaRatio {1.0};              ///< An/Ag
    double stiffenerSpacing {0.0};          ///< a entre enrijecedores transversais de alma (mm); 0 = sem (NBR 8800, 5.4.3.1.1)
    int panelTensionFlange {0};             ///< NBR 14762 Anexo F: 0 = nao; 1 = painel na mesa tracionada, biapoiada; 2 = continua
    bool coldWork {false};                  ///< NBR 14762 Anexo B: aumento de fy nas dobras
    double bearingLength {0.0};             ///< NBR 14762 Anexo D: comprimento de apoio c (mm); 0 = so aviso
    int fatigueCategory {0};                ///< NBR 8800 Anexo H: 0 = nao; 1 A, 2 B, 3 B', 4 C, 5 D, 6 E, 7 E', 8 F, 9 G
    double fatigueCycles {0.0};             ///< N; 0 = vida infinita (limite sigma_TH)
    double openingDiameter {0.0};           ///< NBR 8800 Anexo F: D da abertura circular na alma (mm); 0 = nao ha
    double openingPosition {0.0};           ///< centro da abertura ate o apoio esquerdo (mm)
    double openingOffset {0.0};             ///< excentricidade em relacao a meia altura (mm)
    double openingNeighbourD {0.0};         ///< abertura adjacente: diametro (mm)
    double openingSpacing {0.0};            ///< distancia entre centros (mm)
    double ct {1.0};
    double compositeSpacing {0.0};          ///< barra composta (5.3.6): distancia entre as ligacoes dos perfis (mm); 0 = L/3
    bool compositeWelded {false};           ///< ligacoes intermediarias soldadas ou com parafusos protendidos
    bool threadedEnds {false};              ///< barra redonda com extremidades rosqueadas (5.2.7)
    double webWeldLeg {0.0};                ///< 5.4.5: perna do filete mesa-alma do perfil soldado (mm); 0 = nao verificar
    double webWeldFw {485.0};
    double flangeLineLoad {0.0};            ///< forca aplicada diretamente na mesa (N/mm)
    double coverB {0.0}, coverT {0.0};      ///< 5.4.4: lamela (largura e espessura, mm); 0 = sem lamela
    double coverStart {0.0}, coverEnd {0.0};///< extremidades da lamela a partir do inicio da barra (mm)
    int coverFlanges {2};
    double coverWeldLeg {0.0};
    bool coverEndWeld {true};
    bool plasticAnalysis {false};           ///< 4.11.5: analise rigido-plastica de viga continua
    // Barras de secao variavel (Anexo G): a barra e verificada secao a secao por `designTaperedBar`
    int onlyStation {-1};                   ///< >= 0: so a secao de verificacao de indice dado entra nas verificacoes de esforcos (demais combinacoes inalteradas)
    const DesignSection *axialSection {nullptr};          ///< G.2 e G.3: secao de menor altura para tracao e compressao
    const DesignSection *slendernessSection {nullptr};    ///< G.4.2: secao de maior altura para os parametros de esbeltez da flexao
    double ixEquivalent {0.0};              ///< G.3: inercia equivalente da barra variavel (analise racional, flambagem em relacao ao eixo perpendicular a alma), mm4
    const struct CompositeColumnSection *compositeColumn {nullptr};   ///< Anexo M: secao mista; usada por `designCompositeColumn` (nao por `designBar`)
    std::vector<std::string> tensionNotes; ///< memoria da area liquida e de Ct (5.2.3 a 5.2.5), acrescentada ao resumo
    bool warping {false};                   ///< a barra foi analisada com torcao com empenamento (Vlasov): B e Tw nos esforcos
    bool torsionReleasedStart {false};      ///< extremidade I sem restricao a rotacao em torno do eixo longitudinal (5.7.8)
    bool torsionReleasedEnd {false};
    Standard rectangularTubeStandard {Standard::NBR14762};
    Nbr8800::Options opt8800;
    Nbr14762::Options opt14762;
    std::vector<ComboEffects> combos;
};

struct BarDesignOutput {
    bool supported {false};
    std::string standard;                   ///< "NBR 8800:2024" ou "NBR 14762:2010"
    double maxRatio {0.0};
    std::string governingCheck;             ///< id do estado-limite que governa
    std::string governingCombo;
    double governingX {0.0};                ///< mm
    std::vector<CheckResult> checks;        ///< pior ocorrencia de cada verificacao
    std::vector<std::string> warnings;
    std::vector<std::string> summaryLines;  ///< propriedades e parametros usados
};

BarDesignOutput designBar(const BarDesignInput &input);

} // namespace Structura::Design
