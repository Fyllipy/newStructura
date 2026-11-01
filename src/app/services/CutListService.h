#pragma once

#include "app/services/FabricationModel.h"
#include "app/services/MemberFabrication.h"

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Plano de corte: perfis em barras de estoque (1D) e chapas em chapas de estoque (2D, prateleiras).
 *
 * Perfis: primeiro-encaixe decrescente por perfil, com perda de serra (`kerf`) entre pecas e aparas minimas; chapas: prateleiras (First Fit Decreasing Height) com
 * giro de 90 graus permitido, por espessura. E uma heuristica (nao otimo): serve para dimensionar a compra e conferir o aproveitamento.
 */
class CutListService
{
public:
    struct Options {
        double stockLength {12000.0};   ///< mm (barra padrao de perfil)
        double kerf {3.0};              ///< mm entre cortes
        double sheetWidth {1200.0};     ///< mm (chapa padrao)
        double sheetLength {3000.0};
        double plateKerf {4.0};         ///< mm entre pecas de chapa
    };

    struct BarPlan {
        QString section;
        int barCount {0};
        double usedLength {0.0};        ///< soma dos comprimentos de corte (mm)
        double stockLengthTotal {0.0};  ///< mm comprados
        double wasteLength {0.0};
        double massPerMeter {0.0};
        QVector<QVector<QPair<QString, double>>> patterns;   ///< por barra de estoque: (marca, comprimento)
        [[nodiscard]] double utilization() const { return stockLengthTotal > 0.0 ? usedLength / stockLengthTotal : 0.0; }
    };

    struct SheetPlan {
        double thickness {0.0};
        int sheetCount {0};
        double usedArea {0.0};          ///< mm2 de contorno das pecas
        double sheetArea {0.0};         ///< mm2 de chapa comprada
        int pieceCount {0};
        int oversize {0};               ///< pecas maiores que a chapa padrao (nao alocadas)
        [[nodiscard]] double utilization() const { return sheetArea > 0.0 ? usedArea / sheetArea : 0.0; }
    };

    struct Plan {
        QVector<BarPlan> bars;
        QVector<SheetPlan> sheets;
        double barMassKg {0.0};         ///< massa comprada de perfil
        double usedBarMassKg {0.0};
        QStringList warnings;
    };

    static Plan compute(const QVector<FabMember> &members, const FabBom &bom, const Options &options = {});
};

} // namespace Structura::App::Services
