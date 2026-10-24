#pragma once

#include "design/DesignTypes.h"
#include "design/GrossSection.h"
#include "design/Sectorial.h"

#include <string>
#include <vector>

namespace Structura::Design {

/// Elemento plano comprimido para flambagem local (NBR 8800 Tabela 4).
struct PlateElement {
    std::string name;     ///< "alma", "mesa", "aba"...
    double b {0.0};       ///< largura (mm)
    double t {0.0};       ///< espessura (mm)
    int group {2};        ///< grupo da Tabela 4 da NBR 8800 (1 a 6)
    bool supportedBothEdges {true};
    int count {1};        ///< quantidade de elementos iguais na secao
};

/// Secao preparada para o dimensionamento: propriedades brutas e dimensoes caracteristicas.
struct DesignSection {
    SectionKind kind {SectionKind::Unsupported};
    Standard standard {Standard::NBR8800};
    std::string family;
    std::string name;
    GrossSection g;

    double J {0.0};        ///< mm4
    double Cw {0.0};       ///< mm6
    double x0 {0.0};       ///< coordenadas do centro de cisalhamento em relacao ao centroide (mm), a e b
    double y0 {0.0};
    bool symmetricAboutX {false};   ///< simetrica em relacao ao eixo horizontal (b -> -b)
    bool symmetricAboutY {false};   ///< simetrica em relacao ao eixo vertical (a -> -a)

    // Dimensoes parametricas (mm); zero quando nao se aplicam
    double d {0.0}, bf {0.0}, tw {0.0}, tf {0.0}, t {0.0}, r {0.0};
    double D {0.0};        ///< diametro (tubular/macico circular) ou lado
    double h {0.0};        ///< altura da parte reta da alma
    double leg {0.0};      ///< aba de cantoneira / largura plana de tubo
    double lip {0.0};      ///< enrijecedor de borda (formados)
    double flatWidthB {0.0}, flatWidthH {0.0};   ///< largura plana (tubo retangular): mesas / almas
    bool flangeUp {true};  ///< T: mesa na fibra superior (b > 0)
    bool webLeft {true};   ///< U: alma do lado -a (abertura para +a)

    // Eixos principais (cantoneira): angulo (graus, de a-b para os eixos principais), inercias principais
    double principalAngleDeg {0.0};      ///< giro dos eixos do desenho ate os de projeto
    double nativeCentroidA {0.0};        ///< centroide nas coordenadas do desenho (para reconstruir a geometria)
    double nativeCentroidB {0.0};
    ProfileSpec spec;                    ///< dimensoes parametricas do perfil
    double I1 {0.0}, I2 {0.0};     ///< maior e menor inercia principal (mm4)
    double rMin {0.0};             ///< cantoneira: raio minimo da cantoneira; barra composta: raio de giracao minimo de um perfil componente (mm)
    double componentArea {0.0};    ///< barra composta: area de um perfil componente (mm2)
    double gapE {0.0};             ///< barra composta: afastamento entre os dois perfis (mm)

    SectorialData sectorial;   ///< secao aberta de paredes finas: coordenada setorial, centro de cisalhamento e Cw da linha media (valid = false nos demais casos)

    std::vector<PlateElement> elements;   ///< elementos para flambagem local axial (NBR 8800)
    Loop outline;          ///< contorno externo nos eixos de projeto, origem no centroide (mm)

    [[nodiscard]] double weightPerMeter() const { return g.area * 1e-6 * 7850.0; }   ///< kg/m
    [[nodiscard]] bool doublySymmetric() const { return symmetricAboutX && symmetricAboutY; }
    [[nodiscard]] bool composite() const
    {
        return kind == SectionKind::DoubleAngleT || kind == SectionKind::DoubleChannelBack || kind == SectionKind::DoubleChannelBox;
    }
    [[nodiscard]] bool supported() const { return kind != SectionKind::Unsupported && g.valid; }
};

/// Tipo de secao e norma aplicavel a uma familia de perfis. `tubularStandard` decide o tubular retangular.
SectionKind classifyFamily(const std::string &family);
Standard standardForKind(SectionKind kind, Standard rectangularTubeStandard);
/// Espessura (mm) a partir da qual um tubular retangular e tratado como laminado/acabado a quente (NBR 8800); abaixo disso e perfil formado a frio (NBR 14762).
constexpr double kHotFinishedTubeMinThickness = 6.0;
/// Norma de um tubular retangular reconhecida automaticamente pela serie: os tubos de parede fina (espessura < 6 mm: series comerciais formadas a frio, NBR 6591 e NBR 8261)
/// seguem a NBR 14762; os de parede grossa (>= 6 mm: series acabadas a quente, NBR 16239) seguem a NBR 8800. A geometria nao informa o processo de fabricacao, por isso o criterio
/// e a espessura (os perfis dobrados da biblioteca vao ate 4,75 mm); a escolha manual continua disponivel nos parametros de dimensionamento.
Standard automaticRectTubeStandard(const ProfileSpec &spec);
std::string kindName(SectionKind kind);

/// Constroi a secao de dimensionamento. `loops` sao os contornos (mm) e `jApp` a constante de torcao do
/// aplicativo (mm4). Devolve secao com `kind == Unsupported` quando a familia nao e de aco.
/// `principalAngleDeg`: giro (graus) dos eixos do desenho ate os eixos principais; as propriedades da secao
/// (`g`) ficam expressas nos eixos principais, que sao os eixos locais usados pelo solver.
DesignSection makeDesignSection(const ProfileSpec &spec, const std::vector<Loop> &loops, double jApp,
                                Standard rectangularTubeStandard = Standard::NBR14762,
                                double principalAngleDeg = 0.0, std::size_t outerCount = 1);

} // namespace Structura::Design
