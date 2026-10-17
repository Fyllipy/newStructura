#pragma once

#include "app/SolverBridge.h"

#include <QString>

#include <array>
#include <utility>
#include <vector>

namespace Structura::App::Services {

/**
 * @brief Analise dinamica sobre os modos de vibracao ja calculados (kN, m, s).
 *
 * - espectro de resposta com combinacao modal SRSS/CQC (cada modo vira um caso estatico de forcas equivalentes,
 *   resolvido pelo solver; os resultados por modo, com sinal, sao combinados);
 * - vibracao de pisos pelo caminhar de pessoas (ABNT NBR 8800:2024, Anexo I, I.2.4 a I.4) usando a frequencia e a massa
 *   modal do modelo de elementos finitos;
 * - resposta no tempo por superposicao modal (Newmark, aceleracao media) para cargas harmonicas, pulsos e tabelas.
 *
 * As massas sao concentradas nos nos (massa em t = kN s2/m): cargas gravitacionais / g e, se pedido, a massa dos
 * materiais (metade em cada extremidade da barra). Inercia rotacional e desprezada.
 */
class DynamicsService
{
public:
    static constexpr double kGravity = 9.80665;

    /// Modelo + modos calculados (a ordem dos nos de `model.nodes` e a das formas modais).
    struct Basis {
        const Structura::App::SolverBridge::ModelData *model {nullptr};
        const Structura::App::SolverBridge::Result *modal {nullptr};
        std::vector<double> mass;   ///< massa translacional por no (t), mesma ordem de model->nodes
    };

    /// Massas nodais (t) das cargas gravitacionais do modelo e, se pedido, da massa dos materiais.
    [[nodiscard]] static std::vector<double> nodalMasses(const Structura::App::SolverBridge::ModelData &model, bool includeMaterialMass);

    // ── Modos ────────────────────────────────────────────────────────────
    struct ModeInfo {
        int index {0};
        double omega {0.0};    ///< rad/s
        double frequency {0.0};   ///< Hz
        double period {0.0};   ///< s
        std::array<double, 3> gamma {{0, 0, 0}};      ///< fator de participacao por direcao (X, Y, Z)
        std::array<double, 3> effectiveMass {{0, 0, 0}};   ///< t
        std::array<double, 3> effectiveRatio {{0, 0, 0}};  ///< fracao da massa total
        double generalizedMass {0.0};   ///< t, forma como calculada pelo solver
    };
    [[nodiscard]] static std::vector<ModeInfo> modes(const Basis &basis);
    [[nodiscard]] static double totalMass(const Basis &basis);

    // ── Espectro ─────────────────────────────────────────────────────────
    /// Espectro de pseudo-aceleracao Sa(T)/g por pontos (interpolacao linear; constante fora do intervalo).
    struct Spectrum {
        std::vector<std::pair<double, double>> points;   ///< (T em s, Sa/g)
        [[nodiscard]] double sa(double period) const;
        /// Espectro de quatro trechos (patamar, 1/T, 1/T2): `plateau` = Sa/g maximo, `sa1` = Sa/g em T = 1 s, `tl` = periodo longo.
        [[nodiscard]] static Spectrum parametric(double plateau, double sa1, double tl, double t0Factor = 0.2);
        /// Interpreta linhas "T Sa" (separadas por espaco, ; ou tab); devolve falso se faltar um par valido.
        [[nodiscard]] static bool parse(const QString &text, Spectrum &out);
    };

    struct SpectrumOptions {
        Spectrum spectrum;
        double reduction {1.0};      ///< R (coeficiente de modificacao de resposta)
        double importance {1.0};     ///< I
        double damping {0.05};
        bool cqc {true};             ///< false = SRSS
        std::array<bool, 3> directions {{true, false, false}};
        bool thirtyPercent {false};  ///< combinacao direcional 100% + 30% (senao SRSS)
        int modeCount {0};           ///< 0 = todos os modos calculados
        std::function<void(int, int)> progress;   ///< (modos resolvidos, total), opcional
    };

    struct ModalLine {
        int direction {0};
        int mode {0};
        double period {0.0};
        double sa {0.0};          ///< m/s2 (ja com I/R)
        double gamma {0.0};
        double baseShear {0.0};   ///< kN
    };
    struct SpectrumResult {
        std::vector<ModalLine> lines;
        std::array<double, 3> baseShear {{0, 0, 0}};         ///< combinado por direcao (kN)
        std::array<double, 3> massCovered {{0, 0, 0}};       ///< fracao de massa efetiva dos modos usados
        std::vector<std::array<double, 6>> displacements;    ///< por no (m, rad), combinado com direcoes
        std::vector<Structura::App::SolverBridge::MemberResult> members;   ///< esforcos locais combinados (valores absolutos)
        std::vector<std::array<double, 6>> reactions;        ///< por no (kN, kN m), combinado
        QString warning;
    };
    [[nodiscard]] static SpectrumResult responseSpectrum(const Basis &basis, const SpectrumOptions &options);

    /// Correlacao modal (Der Kiureghian, amortecimento igual nos dois modos).
    [[nodiscard]] static double cqcCorrelation(double omegaA, double omegaB, double damping);

    // ── Vibracao de pisos (NBR 8800:2024, Anexo I) ───────────────────────
    struct FloorOccupancy {
        QString name;
        double peakLimit {0.0};     ///< m/s2
        double responseFactor {0.0};
        double dampingBase {0.0};   ///< componente de amortecimento tipico da ocupacao (Tabela I.2)
    };
    [[nodiscard]] static std::vector<FloorOccupancy> floorOccupancies();

    struct FloorInput {
        int mode {0};
        double damping {0.03};         ///< beta (soma dos componentes da Tabela I.2)
        double personWeight {750.0};   ///< P0 (N)
        int occupancy {0};
        bool useRms {false};
        double fMin {3.0};             ///< Hz (I.1.6)
    };
    struct FloorResult {
        bool ok {false};
        QString message;
        double frequency {0.0};        ///< Hz
        double modalMass {0.0};        ///< kg
        int controlNode {0};           ///< id do no de maior deslocamento vertical
        double zeta {0.0};             ///< coeficiente dinamico
        double weighting {1.0};        ///< W
        double peak {0.0};             ///< m/s2
        double rms {0.0};              ///< m/s2
        double responseFactor {0.0};   ///< R
        double limitPeak {0.0};
        double limitFactor {0.0};
        bool frequencyOk {false};
        bool accelerationOk {false};
        double verticalShare {0.0};    ///< fracao da energia do modo na direcao vertical
    };
    /// Indice do primeiro modo predominantemente vertical (>= 50% da energia em Z); senao o de maior fracao vertical.
    [[nodiscard]] static int firstVerticalMode(const Basis &basis);
    [[nodiscard]] static FloorResult floorVibration(const Basis &basis, const FloorInput &input);

    // ── Resposta no tempo ────────────────────────────────────────────────
    struct TimeInput {
        enum class Kind { Harmonic, HalfSine, Table };
        Kind kind {Kind::Harmonic};
        double amplitude {1.0};     ///< kN
        double frequency {2.0};     ///< Hz (harmonico) ou 1/duracao do pulso (meio seno, calculado de `pulse`)
        double pulse {0.2};         ///< s (duracao do pulso de meio seno)
        std::vector<std::pair<double, double>> table;   ///< (t, F em kN), interpolada linearmente
        double duration {10.0};
        double damping {0.02};
        int loadNode {0};           ///< id do no carregado
        int loadDirection {2};      ///< 0 X, 1 Y, 2 Z
        int responseNode {0};
        int responseDirection {2};
        int modeCount {0};
    };
    struct TimeResult {
        bool ok {false};
        QString message;
        std::vector<double> t, force, displacement, velocity, acceleration;
        double peakDisplacement {0.0}, peakAcceleration {0.0}, rmsAcceleration {0.0};
        double staticDisplacement {0.0};   ///< deslocamento estatico sob a amplitude (referencia p/ fator dinamico)
    };
    [[nodiscard]] static TimeResult timeHistory(const Basis &basis, const TimeInput &input);
};

} // namespace Structura::App::Services
