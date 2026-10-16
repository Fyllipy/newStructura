#pragma once

#include "app/ProjectDataTypes.h"

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Quantitativos: lista de materiais por perfil (comprimento, massa, area de pintura) e totais por material.
 *
 * A massa vem de A x L x peso especifico do material. O sistema de unidades e deduzido do modulo de elasticidade:
 * E em Pa => forca em N; caso contrario kN (comprimentos em m).
 */
class QuantityService
{
public:
    struct Bar {
        int id {0};
        double length {0.0};      ///< m
        QUuid sectionId;
        QUuid materialId;
        bool tie {false};
    };

    struct Row {
        QString section;
        QString family;
        QString material;
        int count {0};
        double totalLength {0.0};   ///< m
        double massPerMeter {0.0};  ///< kg/m
        double totalMass {0.0};     ///< kg
        double paintArea {0.0};     ///< m2 (perimetro externo + furos x comprimento); 0 se a secao nao tem geometria
        QVector<int> bars;
    };

    struct Summary {
        QVector<Row> rows;
        double totalLength {0.0};
        double totalMass {0.0};
        double totalPaintArea {0.0};
        int barCount {0};
        QStringList warnings;
    };

    static Summary compute(const QVector<Bar> &bars,
                           const QVector<Structura::App::ProjectData::SectionInfo> &sections,
                           const QVector<Structura::App::ProjectData::MaterialInfo> &materials);

    /// CSV (separador ';', decimal ',') pronto para planilha.
    static QString toCsv(const Summary &summary);
};

} // namespace Structura::App::Services
