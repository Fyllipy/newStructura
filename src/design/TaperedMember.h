#pragma once

#include "design/BarDesign.h"

#include <vector>

namespace Structura::Design {

/// Barra de secao I de alma variavel (NBR 8800:2024, Anexo G): alturas nas extremidades (d1 no inicio, d2 no fim), mesas e alma constantes. Unidades: mm.
struct TaperedGeometry {
    double d1 {0.0};
    double d2 {0.0};
    double bf {0.0};
    double tw {0.0};
    double tf {0.0};

    [[nodiscard]] double depthAt(double x, double length) const { return d1 + (d2 - d1) * (length > 0.0 ? x / length : 0.0); }
    /// Momento de inercia em relacao ao eixo perpendicular a alma (eixo de maior inercia) na altura d.
    [[nodiscard]] double strongInertia(double d) const;
};

/// Inercia equivalente (mm4) da barra variavel para a flambagem por flexao em relacao ao eixo perpendicular a alma (G.3, analise racional): a barra birrotulada de
/// inercia constante `ixEq` tem a mesma carga critica elastica da barra variavel, obtida pelo metodo de Rayleigh-Ritz com tres semiondas senoidais
/// (E I(x) y'' y'' / y' y'); o comprimento efetivo segue pelo coeficiente K informado.
double taperedEquivalentInertia(const TaperedGeometry &geometry, double length);

/// Verifica a barra variavel secao a secao: cada secao de verificacao usa as propriedades da propria altura (flexao, cortante e interacao), com a secao de menor altura
/// na tracao e na compressao (G.2 e G.3), a inercia equivalente na flambagem em relacao ao eixo perpendicular a alma, Cb = 1,0 e os parametros de esbeltez da secao de maior
/// altura (G.4). `stations[i]` e a secao na posicao da i-esima secao de verificacao de cada combinacao; prevalece a pior razao de cada verificacao.
BarDesignOutput designTaperedBar(const BarDesignInput &base, const TaperedGeometry &geometry, const std::vector<DesignSection> &stations, const DesignSection &smallest,
                                 const DesignSection &largest);

} // namespace Structura::Design
