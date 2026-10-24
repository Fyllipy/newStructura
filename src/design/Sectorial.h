#pragma once

#include "design/GrossSection.h"

#include <array>
#include <vector>

namespace Structura::Design {

/// Linha media de uma secao aberta de paredes finas (arvore de segmentos retos), em coordenadas de desenho (a horizontal, b vertical, mm).
struct CenterLine {
    std::vector<std::array<double, 2>> points;
    struct Segment {
        int first {0};
        int second {0};
        double t {0.0};   ///< espessura (mm)
    };
    std::vector<Segment> segments;
};

/// Propriedades setoriais de uma secao aberta de paredes finas (teoria de Vlasov), nos eixos locais da barra
/// (y = b da secao, z = -a; origem no centroide; eixos principais). Os pontos de amostragem cobrem todos os segmentos.
struct SectorialData {
    bool valid {false};
    double matchScore {0.0};   ///< diferenca entre a linha media e a secao desenhada (0 = igual)
    double Cw {0.0};       ///< constante de empenamento da linha media (mm6)
    double ys {0.0};       ///< centro de cisalhamento em relacao ao centroide, eixos locais (mm)
    double zs {0.0};
    double omegaMax {0.0}; ///< maior |omega| normalizada (mm2)
    struct Sample {
        double y {0.0}, z {0.0};
        double omega {0.0};   ///< coordenada setorial principal normalizada (mm2)
        double sy {0.0};      ///< integral de y dA da parte da secao alem do ponto (mm3)
        double sz {0.0};      ///< integral de z dA (mm3)
        double sw {0.0};      ///< integral de omega dA (mm4)
        double t {0.0};       ///< espessura do segmento (mm)
        double alignedY {0.0};///< |componente do segmento na direcao y| / comprimento (1 = alma vertical)
    };
    std::vector<Sample> samples;
};

/// Calcula os dados setoriais. `canonical` e a linha media na orientacao canonica; escolhe-se a reflexao (em a e/ou b) que reproduz as inercias,
/// o produto de inercia e a posicao do centroide da secao desenhada (`drawn`, eixos do desenho com origem no centroide); `angleDeg` e o giro
/// aplicado as secoes inclinadas ate os eixos principais. `valid = false` quando a linha media nao representa a secao (diferenca grande).
SectorialData computeSectorial(const CenterLine &canonical, const GrossSection &drawn, double angleDeg);

} // namespace Structura::Design
