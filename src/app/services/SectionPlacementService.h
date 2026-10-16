#pragma once

#include "app/ProjectDataTypes.h"

#include <array>
#include <optional>

namespace Structura::App::Services {

/**
 * @brief Posicionamento da secao em relacao ao eixo da barra (giro + ponto de referencia + excentricidade).
 *
 * Convencao (derivada da analise de barras metalicas 3D do CYPE 3D):
 *  - o giro da secao e feito em torno do eixo x local; as inercias sao as principais da secao
 *    nos eixos girados (equivale a transformar Iy/Iz/Iyz por Mohr);
 *  - o ponto de referencia (9 posicoes no retangulo envolvente) e o ponto da secao que fica sobre
 *    o no; o centroide (eixo da barra) fica deslocado de  (deslocamento adicional) - (ponto de ref.);
 *  - essa excentricidade e tratada como braco rigido entre o no e o eixo da barra.
 */
class SectionPlacementService
{
public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;
    using MemberPlacement = Structura::App::ProjectData::MemberPlacement;

    /// Retangulo envolvente da secao em metros, relativo ao centroide (a = largura/eixo z, b = altura/eixo y).
    struct Extents {
        double minA {0.0};
        double maxA {0.0};
        double minB {0.0};
        double maxB {0.0};
    };

    /// Posicao do centroide em relacao ao no, nos eixos locais girados da barra (m).
    struct AxisOffset {
        double dy {0.0};
        double dz {0.0};
    };

    /// Propriedades da secao nos eixos de referencia (sem giro), obtidas por Mohr.
    struct RotatedInertia {
        double iy {0.0};
        double iz {0.0};
        double iyz {0.0};
    };

    /// Propriedades geometricas por integracao do contorno (centrado no centroide) e eixos principais.
    struct PrincipalProperties {
        bool valid {false};
        bool inclined {false};   ///< Iyz significativo: os eixos principais nao coincidem com os eixos da secao.
        double area {0.0};       ///< m2
        double iy {0.0};         ///< m4, integral de z^2 dA nos eixos da secao (a = z)
        double iz {0.0};         ///< m4, integral de y^2 dA nos eixos da secao (b = y)
        double iyz {0.0};        ///< m4, integral de y*z dA
        double thetaDeg {0.0};   ///< giro (graus) dos eixos da secao ate os principais, mesmo sentido do giro da barra
        double iyPrincipal {0.0};
        double izPrincipal {0.0};
    };

    /// Eixos principais da secao (Iyz por Green, giro por Mohr). `valid=false` sem geometria parametrica.
    [[nodiscard]] static PrincipalProperties principalProperties(const SectionInfo &section);

    /// Desloca um vetor (dy, dz) dado nos eixos da secao girada para os eixos principais (giro adicional `thetaDeg`).
    [[nodiscard]] static AxisOffset toPrincipalFrame(const AxisOffset &offset, double thetaDeg);

    /// Extensoes da secao; vazio quando a secao nao tem geometria parametrica.
    [[nodiscard]] static std::optional<Extents> extents(const SectionInfo &section);

    /// Coordenadas (a, b) do ponto de referencia em relacao ao centroide.
    [[nodiscard]] static std::array<double, 2> referencePoint(const Extents &extents, int reference);

    /// Deslocamento do centroide em relacao ao no (eixos locais girados). Sem geometria so vale o deslocamento adicional.
    [[nodiscard]] static AxisOffset axisOffset(const SectionInfo *section, const MemberPlacement &placement);

    /// Inercias da secao (principais iy, iz) vistas nos eixos de referencia sem giro.
    [[nodiscard]] static RotatedInertia inertiaInReferenceAxes(double iy, double iz, double rotationDeg);

    /// Rotulos dos 9 pontos de referencia, na ordem de SectionReference.
    [[nodiscard]] static QStringList referenceLabels();
};

} // namespace Structura::App::Services
