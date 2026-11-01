#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <array>

namespace Structura::App::Services {

/// Segmento de reta importado (eixo de uma barra), em metros.
struct ImportedSegment {
    std::array<double, 3> a {0.0, 0.0, 0.0};
    std::array<double, 3> b {0.0, 0.0, 0.0};
    QString layer;     ///< DXF: camada; IFC: classe da peca (IFCBEAM, IFCCOLUMN, ...)
    QString profile;   ///< IFC: nome do perfil da peca (ex.: "W 410x53"), vazio se nao informado
    QString name;      ///< IFC: nome da peca
};

struct ImportResult {
    QVector<ImportedSegment> segments;
    QStringList warnings;   ///< entidades ignoradas e premissas adotadas
};

/// Rede de nos e barras obtida dos segmentos (nos coincidentes fundidos).
struct ImportedTopology {
    struct Bar {
        int a {0}, b {0};   ///< indices em `nodes`
        QString layer;
        QString profile;
        QString name;
    };
    QVector<std::array<double, 3>> nodes;   ///< m
    QVector<Bar> bars;
    int discardedZeroLength {0};
    int mergedDuplicates {0};
};

/**
 * @brief Importa geometria de um DXF ASCII (R12 a 2018): LINE, LWPOLYLINE e POLYLINE/VERTEX viram eixos de barras (a camada fica na etiqueta).
 *
 * Arcos, circulos, splines, blocos (INSERT), faces e textos nao viram barras (o numero de entidades ignoradas sai nos avisos). `unitScale` e o fator para
 * metros; 0 = usa $INSUNITS do cabecalho (polegada, pe, milimetro, centimetro, metro) e, na falta dele, metros. Polilinhas fechadas fecham o contorno.
 */
class DxfImporter
{
public:
    [[nodiscard]] static ImportResult parse(const QString &text, double unitScale = 0.0);
};

/**
 * @brief Importa pecas lineares de um arquivo IFC (STEP, IFC2X3 ou IFC4): IFCBEAM, IFCCOLUMN, IFCMEMBER e IFCSTRUCTURALCURVEMEMBER (e as variantes STANDARDCASE).
 *
 * O eixo vem da representacao "Axis" (polilinha) quando existe e, senao, do solido extrudado da representacao "Body" (origem do solido e comprimento da extrusao),
 * levados ao sistema global pela cadeia de IfcLocalPlacement. Unidade de comprimento lida de IFCSIUNIT / IFCCONVERSIONBASEDUNIT. O nome do perfil
 * (IfcProfileDef.ProfileName) fica na etiqueta. Nao importa chapas, parafusos, geometria de ligacao, cascas nem propriedades.
 */
class IfcImporter
{
public:
    [[nodiscard]] static ImportResult parse(const QString &text);
};

/// Funde os extremos dentro de `tolerance` (m) em nos, descarta segmentos de comprimento nulo e duplicados (mesmo par de nos).
[[nodiscard]] ImportedTopology buildTopology(const QVector<ImportedSegment> &segments, double tolerance = 1e-3);

} // namespace Structura::App::Services
