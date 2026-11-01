#pragma once

#include "app/services/FabricationModel.h"
#include "app/services/SheetBuilder.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Peca de perfil fabricada: comprimento real de corte, furacao e chapas ligadas em cada extremidade, com marca unica.
 *
 * Parte do comprimento entre nos, desconta os recuos ate a face dos apoios (ligacoes) e o afastamento de cada extremidade em relacao a face
 * (folga/setback da ligacao) e transporta os furos de mesa e alma dos perfis ligados (FabricationModel) para coordenadas da propria peca: x medido a partir da
 * extremidade de corte (I ou J), y a partir da linha de centro da alma (+ = lado de t positivo da ligacao).
 * Pecas com o mesmo perfil, comprimento de corte e furacao dividem a marca (P pilar, V viga, D diagonal, R trelica, T tirante).
 */
struct FabMember {
    struct Hole {
        char end {'I'};         ///< extremidade de referencia do x
        QString face;           ///< "alma" ou "mesa"
        double x {0.0};         ///< mm desde a extremidade de corte
        double y {0.0};         ///< mm (alma: desde a linha de centro; mesa: posicao na largura desde o eixo)
        double diameter {0.0};
        int connectionId {0};
    };
    struct Attached {
        QString plateMark;      ///< marca da chapa na ligacao (ex.: L3-C1)
        QString name;           ///< ex.: PL 9,5 x 100 x 215
        char end {'I'};
        int connectionId {0};
        int quantity {1};
        bool welded {false};    ///< soldada ao perfil (faz parte do conjunto); senao, soldada ao apoio
        double massKg {0.0};    ///< massa por chapa
    };
    struct Cope {
        char end {'I'};
        bool top {true};
        double length {0.0};
        double depth {0.0};
    };

    QString mark;
    QString section;
    QVector<int> bars;          ///< barras (ids externos) que compartilham a marca
    double nodeLength {0.0};    ///< mm, entre nos
    double faceLength {0.0};    ///< mm, entre faces dos apoios
    double cutLength {0.0};     ///< mm, comprimento real de corte
    double gapI {0.0}, gapJ {0.0};   ///< afastamento das extremidades de corte em relacao a face do apoio
    double massPerMeter {0.0};
    double unitMassKg {0.0};    ///< so o perfil (as chapas da ligacao estao na lista de pecas das ligacoes)
    QVector<Hole> holes;
    QVector<Cope> copes;        ///< recortes das mesas nas extremidades
    double endAngleI {0.0}, endAngleJ {0.0};   ///< corte inclinado das extremidades (graus)
    double assemblyMassKg {0.0};   ///< perfil + chapas soldadas ao perfil (conjunto)
    QVector<Attached> attached;   ///< chapas das ligacoes das extremidades (informativo: soldadas ao apoio ou ao perfil conforme a tipologia)
    SheetBuilder::Bar profile;  ///< perfil (secao) da primeira barra do grupo
    int quantity {0};
};

class MemberFabrication
{
public:
    /// Agrupa as barras do projeto em pecas marcadas. `Input::connections` fornece as ligacoes de extremidade (`NodeConnection::memberId/atEnd`).
    static QVector<FabMember> build(const SheetBuilder::Input &input);
};

} // namespace Structura::App::Services
