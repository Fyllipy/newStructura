#pragma once

#include "app/ProjectDataTypes.h"
#include "core/section/DesignSection.h"

#include <QString>
#include <QVector>

#include <utility>

namespace Structura::App::Services {

/**
 * @brief Propriedades de secao e fatores de unidade usados pela analise estrutural e pelo dimensionamento.
 *
 * Faz parte do nucleo compartilhado (StructuraCore): a analise precisa da area de cisalhamento e do comprimento em mm,
 * e nao depende de nenhum modulo de dimensionamento. Sem logica de verificacao de norma.
 */
class SectionPropertiesService
{
public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;
    using MaterialInfo = Structura::App::ProjectData::MaterialInfo;

    /// Fatores de unidade deduzidos do modulo de elasticidade do material (Pa -> N e m; kN/m2 -> kN e m).
    static std::pair<double, double> unitFactors(const QVector<MaterialInfo> &materials);

    /// Secao de dimensionamento correspondente ao perfil (propriedades brutas nos eixos principais).
    static Structura::Design::DesignSection makeSection(const SectionInfo &section, const QString &rectTubeStandard);

    /// Areas efetivas de cisalhamento (mm2) para a deformacao por cortante: {na direcao da altura (alma), na direcao da largura (mesas)}.
    /// I/U/T: d.tw e 2.bf.tf (T: bf.tf); tubo retangular: 2.(d-2t).t e 2.(bf-2t).t; tubo circular: A/2; macico redondo: 9/10 A; macico retangular: 5/6 A;
    /// formados a frio: d.t e 2.bf.t. {0, 0} = sem area definida (cantoneiras, secao nao suportada): a barra fica rigida ao cortante.
    static std::pair<double, double> shearAreasMm2(const Structura::Design::DesignSection &section);
};

} // namespace Structura::App::Services
