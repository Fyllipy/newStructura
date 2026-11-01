#pragma once

#include "app/ProjectDataTypes.h"

#include <QString>
#include <QVector>

#include <array>
#include <vector>

namespace Structura::App::Services {

/**
 * @brief Modelo de fabricacao de uma ligacao: chapas com contorno e furos, parafusos, soldas e furacao nos perfis ligados.
 *
 * E derivado dos solidos da ligacao (`ConnectionSolids`), entao qualquer tipologia do catalogo passa a ter pecas marcadas, lista de corte e lista de
 * parafusos sem desenho proprio. Coordenadas em mm no referencial da ligacao (s, t, u). Os furos das chapas e dos perfis vem dos parafusos cujo eixo
 * atravessa a peca; o diametro e o do furo padrao (NBR 8800, Tabela 14: d + 1,5 mm ate 24 mm, d + 3 mm acima).
 */
struct FabHole {
    double a {0.0}, b {0.0};   ///< centro no plano da peca (eixos `planeAxes`), mm
    double diameter {0.0};
};

struct FabPart {
    enum class Kind { Plate, MemberPlate };   ///< MemberPlate: mesa/alma de perfil ligado (so a furacao interessa)

    QString mark;                       ///< "C1", "C2"... (so chapas); vazio nos perfis
    QString name;                       ///< ex.: "PL 9,5 x 200 x 215"
    Kind kind {Kind::Plate};
    QString material;                   ///< ex.: "fy 250 MPa" (da ligacao) ou vazio
    double thickness {0.0};             ///< mm
    int thicknessAxis {2};              ///< 0 = s, 1 = t, 2 = u
    std::array<int, 2> planeAxes {0, 1};
    double midOffset {0.0};             ///< coordenada do plano medio ao longo de `thicknessAxis`
    std::vector<std::array<double, 2>> outline;   ///< contorno no plano, ja relativo ao canto minimo (0,0)
    std::array<double, 2> origin {0.0, 0.0};      ///< canto minimo no referencial da ligacao (eixos `planeAxes`)
    double width {0.0}, height {0.0};   ///< caixa envolvente no plano (mm)
    bool rectangular {true};
    std::vector<FabHole> holes;         ///< relativos ao canto minimo
    double grossArea {0.0};             ///< mm2 (contorno)
    double massKg {0.0};                ///< massa liquida (descontados os furos)
    int quantity {1};
    bool weldedToMember {false};        ///< chapa soldada ao perfil ligado (nao ao apoio): faz parte do conjunto da peca
};

/// Recorte de mesa do perfil ligado na extremidade (cope), em mm.
struct FabCope {
    bool top {true};                    ///< mesa do lado de t positivo
    double length {0.0};                ///< ao longo do perfil, desde a extremidade de corte
    double depth {0.0};                 ///< profundidade (mesa + folga)
};

struct FabBolt {
    QString mark;                       ///< "B1"...
    double diameter {0.0};
    double gripLength {0.0};            ///< mm (cabeca ate porca, sem arruelas)
    double length {0.0};                ///< comprimento sob a cabeca sugerido (grip + porca + rosca), multiplo de 5 mm
    QString grade;
    int quantity {0};
    std::array<double, 3> position {};   ///< um dos parafusos do grupo (referencial da ligacao, mm)
    int axis {2};                        ///< eixo do parafuso (0 = s, 1 = t, 2 = u)
    std::vector<std::array<double, 3>> positions;   ///< todos os parafusos do grupo (centro, referencial da ligacao, mm)
};

struct FabWeld {
    QString mark;                       ///< "S1"...
    double leg {0.0};                   ///< perna do filete (mm)
    double length {0.0};                ///< comprimento de cada cordao (mm)
    int quantity {0};
    double totalLength {0.0};           ///< mm
    std::array<double, 3> position {};  ///< centro de um dos cordoes do grupo (referencial da ligacao, mm)
};

struct FabAssembly {
    QVector<FabPart> plateInstances;    ///< cada chapa fisica (quantidade 1) com a marca do seu grupo, na ordem dos solidos
    QVector<FabPart> parts;             ///< chapas marcadas (quantidade agrupada) seguidas das pecas de perfil
    QVector<FabBolt> bolts;
    QVector<FabWeld> welds;
    QVector<FabCope> copes;             ///< recortes das mesas do perfil ligado
    double endAngleDeg {0.0};           ///< corte inclinado da extremidade (angulo entre o eixo e a normal da face do apoio)
    double plateMassKg {0.0};
    double boltMassKg {0.0};

    [[nodiscard]] int plateCount() const;
};

/// Lista de pecas de todas as ligacoes do projeto (chapas iguais somadas entre ligacoes).
struct FabBom {
    struct PlateRow {
        QString name;          ///< "PL e x l x c"
        QString material;
        double thickness {0.0}, width {0.0}, height {0.0};
        int holes {0};
        int quantity {0};
        double unitMassKg {0.0};
        double totalMassKg {0.0};
        QVector<int> connections;   ///< ids das ligacoes que usam a chapa
    };
    struct BoltRow {
        double diameter {0.0};
        double length {0.0};
        QString grade;
        int quantity {0};
        double totalMassKg {0.0};
    };
    struct WeldRow {
        double leg {0.0};
        double totalLength {0.0};   ///< mm
        int beads {0};
    };
    QVector<PlateRow> plates;
    QVector<BoltRow> bolts;
    QVector<WeldRow> welds;
    double plateMassKg {0.0};
    double boltMassKg {0.0};
    int connectionCount {0};
};

class FabricationModel
{
public:
    static FabAssembly fromConnection(const Structura::App::ProjectData::NodeConnection &connection);

    static FabBom bom(const QVector<Structura::App::ProjectData::NodeConnection> &connections);

    /// Centro da chapa no referencial da ligacao (s, t, u), mm.
    static std::array<double, 3> centre(const FabPart &part);

    /// Diametro do furo padrao (mm) para o parafuso de diametro `bolt`.
    static double holeDiameter(double bolt);
    /// Massa aproximada (kg) de um parafuso com porca e duas arruelas.
    static double boltMass(double diameter, double length);
};

} // namespace Structura::App::Services
