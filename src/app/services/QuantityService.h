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

    /// Peca de ligacao agregada (chapas, cantoneiras, parafusos, solda).
    struct ConnectionRow {
        QString description;
        int quantity {0};
        double massEach {0.0};     ///< kg
        double totalMass {0.0};
        double weldLength {0.0};   ///< mm (soma; so para solda)
    };

    struct Summary {
        QVector<ConnectionRow> connections;
        double connectionMass {0.0};
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

    /// Soma uma peca de ligacao (mesma descricao e massa unitaria = mesma linha).
    static void addConnectionPart(Summary &summary, const QString &description, int quantity, double massEach, double weldLength);

    /// CSV (separador ';', decimal ',') pronto para planilha.
    static QString toCsv(const Summary &summary);
};

} // namespace Structura::App::Services
