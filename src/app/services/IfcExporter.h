#pragma once

#include "app/services/SheetBuilder.h"

#include <QString>

namespace Structura::App::Services {

/**
 * @brief Exporta a estrutura em IFC4 (STEP, texto) para troca com outros programas de modelagem e BIM.
 *
 * Cada peca de perfil marcada (`MemberFabrication`) vira um IfcColumn (vertical), IfcBeam (horizontal) ou IfcMember (diagonais, trelicas e tirantes)
 * com o comprimento de corte real, extrudado de um perfil I (IfcIShapeProfileDef) ou retangular (largura x altura da secao) e com o giro da secao.
 * Unidade: milimetro. Estrutura espacial: IfcProject > IfcSite > IfcBuilding > IfcBuildingStorey, e as pecas contidas no pavimento.
 *
 * As chapas das ligacoes (IfcPlate) saem com contorno e furos (IfcArbitraryProfileDefWithVoids) posicionadas pelo referencial da ligacao; cada elemento leva material
 * (aco) e um conjunto de propriedades (marca, perfil, comprimento de corte, massa).
 *
 * Limites: perfis que nao sao I/H saem como retangulos de largura x altura; parafusos e soldas nao sao exportados; recortes e furos dos perfis nao entram na
 * geometria; o lado (u) das chapas na alma e inferido do referencial da ligacao e pode sair espelhado. O arquivo nao foi validado em outro programa.
 */
class IfcExporter
{
public:
    struct Options {
        QString projectName {QStringLiteral("Projeto")};
        QString author;
    };

    static QString build(const SheetBuilder::Input &input, const Options &options);

    /// Identificador IFC de 22 caracteres (base 64 do IFC) a partir de 16 bytes.
    static QString compressGuid(const QByteArray &sixteenBytes);
};

} // namespace Structura::App::Services
