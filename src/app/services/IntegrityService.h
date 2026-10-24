#pragma once

#include "app/ProjectDataTypes.h"

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Integridade estrutural (ABNT NBR 8800:2024, 4.13): travamento dos pilares em duas direcoes (4.13.4), forca de tracao das contencoes
 * horizontais e de suas ligacoes (4.13.7) e, quando exigido pela legislacao, colapso progressivo por falha acidental de um pilar (4.13.8).
 */
class IntegrityService
{
public:
    struct Node {
        int id {0};
        double x {0.0}, y {0.0}, z {0.0};   ///< m
        bool supported {false};
    };
    struct Bar {
        int id {0};
        int nodeI {0};
        int nodeJ {0};
        QString name;
        double ntRdKn {-1.0};   ///< resistencia de calculo a tracao (kN); < 0 = desconhecida
        double ncRdKn {-1.0};   ///< idem a compressao
    };
    /// Esforco de extremidade de uma barra numa combinacao: componente vertical (N), axial (N, + tracao).
    struct EndForce {
        int memberId {0};
        bool atEnd {false};
        QString combo;
        double vertical {0.0};
        double axial {0.0};
    };
    /// Forca de compressao maxima de calculo (N) de um pilar.
    struct ColumnLoad {
        int memberId {0};
        double compression {0.0};
    };
    struct Options {
        bool roofWithoutSlab {false};         ///< piso ou cobertura sem laje de concreto: contencoes dos pilares de extremidade para tracao e compressao, 45 kN (4.13.7)
        bool progressiveCollapse {false};     ///< 4.13.8: falha acidental de um pilar nao pode causar colapso progressivo
        QString combination;                  ///< combinacao de acoes permanentes diretas e de uso e ocupacao (4.13.8); vazio = envoltoria
        double verticalTolerance {0.80};      ///< barra vertical: |dz|/L >= tolerancia
        double horizontalTolerance {0.35};    ///< contencao horizontal: |dz|/L <= tolerancia (inclui banzos de tesoura com pouca inclinacao)
    };
    struct Input {
        QVector<Node> nodes;
        QVector<Bar> bars;
        QVector<ColumnLoad> columnLoads;
        QVector<EndForce> endForces;
        QVector<Structura::App::ProjectData::NodeConnection> connections;
        Options options;
    };

    struct Row {
        QString clause;          ///< "NBR 8800:2024, 4.13.7"
        QString description;
        QString location;        ///< "Pilar 3 no nível z = 6,00 m"
        double demandKn {0.0};
        double resistanceKn {0.0};   ///< < 0 = nao determinada
        double eta {0.0};            ///< demand / resistance (< 0 = nao determinada)
        bool ok {true};
        bool advisory {false};       ///< recomendacao ("preferencialmente"): nao aprovado gera aviso, nao reprovacao
        QString note;
    };

    static QVector<Row> run(const Input &input);

    /// Forca de tracao minima de calculo das contencoes horizontais (4.13.7): max(1 % da forca do pilar ; 75 kN), ou 45 kN nos pilares de extremidade sem laje (N).
    static double minimumTieForce(double columnForceN, bool edgeWithoutSlab);
};

} // namespace Structura::App::Services
