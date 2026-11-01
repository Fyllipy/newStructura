#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <array>

namespace Structura::App::Services {

/**
 * @brief Vento estatico em edificios de planta retangular (ABNT NBR 6123:2023).
 *
 * Vk = V0 S1 S2 S3 e q = 0,613 Vk^2 (N/m2). S2 = bm Fr (z/10)^p com os parametros das Tabelas 1 e 2 (z < 5 m usa 5 m e, na categoria V, z < 10 m usa 10 m);
 * a classe da edificacao (A, B ou C) vem da maior dimensao da superficie frontal. A forca em cada pavimento vem dos coeficientes de forma externos, Ce, da
 * Tabela 6 (paredes de barlavento e de sotavento), interpolados em a/b entre 3/2 e 2 (nota 2) e escolhidos pela relacao h/b. A excentricidade da forca em relacao ao
 * eixo vertical geometrico e ea = 0,075 a, eb = 0,075 b (0,15 com efeitos de vizinhanca), 6.1.4. Vento so a 0 e 90 graus (as quatro direcoes ortogonais).
 */
class WindStaticService
{
public:
    enum Direction { PlusX = 0, MinusX = 1, PlusY = 2, MinusY = 3 };

    struct Parameters {
        double v0 {35.0};              ///< velocidade basica (m/s)
        double s1 {1.0};               ///< fator topografico
        double s3 {1.0};               ///< fator estatistico
        int category {2};              ///< categoria de rugosidade, 1 a 5
        bool neighbourhood {false};    ///< efeitos de vizinhanca: excentricidade 0,15 em vez de 0,075 (6.1.4)
        bool highTurbulence {false};   ///< vento de alta turbulencia (6.1.3): sotavento com 2/3 do coeficiente da Tabela 6
        double parapet {0.0};          ///< altura de platibanda/ático acima do ultimo pavimento (m), somada a area carregada do topo
    };

    struct Building {
        double xMin {0.0}, xMax {0.0}, yMin {0.0}, yMax {0.0};   ///< retangulo envolvente da planta (m)
        QVector<double> levels;   ///< cotas dos pavimentos em ordem crescente (m); o primeiro e a base (nivel dos apoios)
    };

    struct FloorLoad {
        double z {0.0};          ///< cota (m)
        double tributary {0.0};  ///< altura carregada (m)
        double s2 {0.0};
        double q {0.0};          ///< pressao dinamica (kN/m2)
        double force {0.0};      ///< forca resultante na direcao do vento (kN, positiva no sentido do vento)
    };

    struct Case {
        Direction direction {PlusX};
        int eccentricity {0};            ///< 0 = sem excentricidade; +1 / -1 = resultante deslocada de +e / -e na direcao transversal
        double eccentricityDistance {0.0};   ///< e (m), com sinal aplicado
        char windClass {'B'};
        double alpha {0.0};              ///< 0 ou 90 graus (Tabela 6)
        double windwardCoefficient {0.0};
        double leewardCoefficient {0.0};
        double faceWidth {0.0};          ///< largura da fachada de barlavento (m)
        QVector<FloorLoad> floors;
        QString name;                    ///< ex.: "VENTO +X", "VENTO +X e+"
    };

    struct Result {
        double a {0.0}, b {0.0}, h {0.0};   ///< dimensoes em planta (a >= b) e altura (m)
        QVector<Case> cases;
        QStringList warnings;
    };

    /// `directions`: mascara de bits (1 << Direction); `eccentricity`: 0 = so a resultante centrada; 1 = centrada e com +e/-e; 2 = so com +e e -e.
    [[nodiscard]] static Result compute(const Parameters &parameters, const Building &building, int directions, int eccentricity);

    /// Fator S2 (Tabelas 1 e 2) na cota z para a categoria e a classe.
    [[nodiscard]] static double s2(int category, char windClass, double z);
    /// Coeficientes de forma externos da Tabela 6 de barlavento e de sotavento (paredes C e D para alfa = 0; A e B para alfa = 90).
    static void wallCoefficients(double heightOverB, double aOverB, double alphaDegrees, double &windward, double &leeward);
    /// Fator topografico S1(z) no ponto B de talude ou morro (5.2-b): thetaDegrees = inclinacao media, z = altura sobre o terreno (m), dt = desnivel entre base e topo (m).
    /// Interpola linearmente entre 3 e 6 graus e entre 17 e 45 graus; nunca inferior a 1,0.
    [[nodiscard]] static double s1Topographic(double thetaDegrees, double z, double dt);
    /// Linha da Tabela 6 interpolada em a/b (nota 2): alfa = 0 -> {A1B1, A2B2, C, D}; alfa = 90 -> {A, B, C1D1, C2D2}.
    [[nodiscard]] static std::array<double, 4> wallRow(double heightOverB, double aOverB, bool alpha90);
    /// Classe da edificacao (5.3.2) pela maior dimensao da superficie frontal (m).
    [[nodiscard]] static char windClass(double largestFrontalDimension);
};

} // namespace Structura::App::Services
