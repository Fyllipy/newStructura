#pragma once

#include "app/SolverBridge.h"

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Comprimentos efetivos de flambagem (coeficientes K) das barras comprimidas a partir dos modos de flambagem elastica.
 *
 * Para cada modo k (fator critico lambda_k) e cada barra comprimida i (esforco axial N_i < 0), a energia de flexao da barra em cada plano
 * e e_i,k = E I integral (v'')^2 dx, com v o deslocamento transversal do modo (deslocamento da corda mais a flecha pelas rotacoes de extremidade, em relacao ao eixo
 * indeformado: inclui o efeito P-delta do deslocamento de andar). A participacao da barra no modo e e_i,k / max_j e_j,k; a barra "flamba" no primeiro modo
 * (menor lambda) em que a participacao atinge o limite escolhido. Como as cargas do modelo sao multiplicadas por lambda, N_cr,i = lambda_k |N_i| e
 * K = pi sqrt(E I) / (L sqrt(N_cr,i)), isto e, N_cr = pi^2 E I / (K L)^2.
 *
 * O resultado separa os dois planos: X = flexao em torno do eixo de maior inercia (plano da altura) e Y = em torno do de menor inercia.
 */
class EffectiveLengthService
{
public:
    struct Options {
        double minParticipation {0.10};   ///< participacao minima da barra (ou do grupo de trechos colineares) no modo: energia de flexao em relacao ao maior valor de barra no modo
        double minAxialRatio {0.05};      ///< |N| minimo da barra em relacao ao maior |N| comprimido (descarta barras quase sem carga)
        double kMin {0.30};               ///< valores de K abaixo disto ou acima de kMax sao marcados como suspeitos
        double kMax {6.00};
    };

    struct PlaneResult {
        bool found {false};
        int mode {0};            ///< 1-based
        double lambda {0.0};
        double participation {0.0};
        double ncr {0.0};        ///< unidade de forca do modelo
        double k {1.0};          ///< em relacao ao comprimento do grupo de barras colineares
        double effectiveLength {0.0};   ///< K x comprimento do grupo
        bool suspicious {false};
    };

    struct MemberResult {
        int memberId {0};
        double length {0.0};     ///< comprimento da barra (unidade de comprimento do modelo)
        /// Barras colineares ligadas em nos que so as unem (uma coluna dividida em trechos) formam um grupo: K e o comprimento efetivo valem para o grupo todo.
        int groupSize {1};
        double groupLength {0.0};
        double axial {0.0};      ///< esforco axial de primeira ordem (compressao < 0)
        PlaneResult x;           ///< plano da altura (flexao em torno do eixo forte)
        PlaneResult y;
    };

    struct Output {
        QVector<MemberResult> members;
        double firstLambda {0.0};
        /// Amplificacao nominal dos efeitos de primeira ordem 1 / (1 - 1/lambda1) (apenas se lambda1 > 1).
        double amplification {0.0};
        /// 0 = pequena deslocabilidade (amplificacao <= 1,10), 1 = media (<= 1,40), 2 = grande (NBR 8800, 4.10.4); -1 = sem modo de flambagem.
        int swayClass {-1};
        QStringList notes;
    };

    /// `model` e `result` vem da analise de flambagem (BucklingService::run): forcas axiais de primeira ordem e formas modais.
    [[nodiscard]] static Output compute(const Structura::App::SolverBridge::ModelData &model, const Structura::App::SolverBridge::Result &result, const Options &options);
};

} // namespace Structura::App::Services
