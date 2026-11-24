#pragma once

#include <array>
#include <string>
#include <vector>

namespace Structura::App::Services {

/**
 * Gerador de escadas metálicas: reta, em L, em U e em espiral (helicoidal).
 *
 * A escada é formada por longarinas (1 central, 2 laterais ou 3), degraus (barras transversais entre as longarinas,
 * ou balanços quando só há a longarina central) e, nas escadas em L e em U, por um patamar (vigas de borda e vigas
 * secundárias). A geometria sai em coordenadas globais (m); o chamador só cria nós, barras, apoios e cargas.
 * Sem dependência de Qt nem de interface, para ser testado isoladamente.
 */
class StairGenerator
{
public:
    enum class Type { Straight, LShaped, UShaped, Spiral };
    enum class Layout { Two, Three, Center };
    enum class Role { Stringer, Tread, LandingBeam, LandingTread };
    enum class SupportKind { None, Pinned, Fixed };

    /// Tipos de carregamento que o usuário pode associar a um caso de carga.
    enum class LoadKind {
        TreadArea,      ///< sobrecarga de piso nos degraus (kN/m2, sobre a projeção horizontal)
        LandingArea,    ///< sobrecarga de piso no patamar (kN/m2)
        StringerLine,   ///< carga linear em todas as longarinas (kN/m, por metro de barra)
        HandrailLine,   ///< carga linear nas longarinas laterais, ex.: peso do guarda-corpo (kN/m)
    };

    struct Load {
        LoadKind kind {LoadKind::TreadArea};
        double value {0.0};         ///< positivo = para baixo (gravidade)
        std::string loadCase;       ///< nome do caso de carga ao qual o carregamento pertence
    };

    struct Params {
        Type type {Type::Straight};
        Layout layout {Layout::Two};

        double baseZ {0.0};         ///< cota do nível de partida (m)
        double topZ {3.0};          ///< cota do nível de chegada (m)
        double riser {0.18};        ///< altura alvo do degrau (espelho), m; o número de degraus é arredondado e a altura recalculada
        double tread {0.28};        ///< profundidade do piso do degrau, m
        double width {1.0};         ///< largura útil da escada (eixo a eixo das longarinas laterais), m

        double x0 {0.0}, y0 {0.0};  ///< ponto inicial (início da escada; centro do eixo na espiral), m
        double azimuthDeg {0.0};    ///< direção de subida no plano (0 = +X, 90 = +Y)
        bool turnLeft {true};       ///< sentido da curva (L, U) ou do giro (espiral): anti-horário
        bool endByPoint {false};    ///< escada reta: o ponto final (x1, y1) define direção e profundidade do piso
        double x1 {0.0}, y1 {0.0};

        int firstFlightSteps {0};   ///< L e U: degraus do primeiro lance (0 = metade)
        double landingLength {0.0}; ///< L e U: profundidade do patamar na direção do primeiro lance (0 = igual à largura)
        double gap {0.10};          ///< U: folga entre os dois lances, m

        double outerRadius {1.20};  ///< espiral: raio externo, m
        double innerRadius {0.15};  ///< espiral: raio interno (coluna central), m
        double totalAngleDeg {0.0}; ///< espiral: ângulo total (0 = calculado pela profundidade do piso)

        SupportKind baseSupport {SupportKind::Pinned};
        SupportKind topSupport {SupportKind::Pinned};

        std::vector<Load> loads;
    };

    struct Node {
        double x {0.0}, y {0.0}, z {0.0};
    };

    struct Bar {
        int a {0}, b {0};
        Role role {Role::Stringer};
        bool lateral {false};       ///< longarina lateral (recebe guarda-corpo)
        double areaWidth {0.0};     ///< largura de influência para cargas de área (m); 0 = não recebe
    };

    struct Result {
        std::vector<Node> nodes;
        std::vector<Bar> bars;
        std::vector<int> baseNodes;   ///< nós das longarinas no início
        std::vector<int> topNodes;    ///< nós das longarinas no fim
        int risers {0};
        double riser {0.0};           ///< altura real do degrau
        double tread {0.0};           ///< profundidade real do piso (espiral: no raio médio)
        int firstFlight {0};          ///< degraus do primeiro lance (L e U)
        std::vector<std::string> warnings;
    };

    /// Carga de barra gerada a partir de um carregamento: `qz` em N/m no sistema global (negativo = para baixo).
    struct MemberLoadSpec {
        int bar {0};
        std::string loadCase;
        double qz {0.0};
    };

    [[nodiscard]] static Result generate(const Params &params);
    /// Converte os carregamentos em cargas por barra, somando as que caem na mesma barra e no mesmo caso.
    [[nodiscard]] static std::vector<MemberLoadSpec> memberLoads(const Params &params, const Result &result);

    [[nodiscard]] static const char *typeName(Type type);
    [[nodiscard]] static const char *layoutName(Layout layout);
    [[nodiscard]] static const char *loadKindName(LoadKind kind);
    [[nodiscard]] static const char *supportName(SupportKind kind);
};

} // namespace Structura::App::Services
