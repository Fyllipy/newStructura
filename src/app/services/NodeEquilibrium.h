#pragma once

#include <array>
#include <vector>

namespace Structura::App::Services::NodeEquilibrium {

/// Esforcos de extremidade de uma barra no no (uma combinacao): eixos locais no sistema global e o vetor de 12 componentes
/// (N, Vy, Vz, Mt, My, Mz do no i e do no j; N e N.mm) na convencao do avaliador de esforcos internos (esforco na face positiva da secao).
struct MemberEnd {
    bool atEnd {false};   ///< o no e a extremidade j da barra
    std::array<double, 3> x {}, y {}, z {};
    std::array<double, 12> f {};
};

struct PanelResult {
    bool valid {false};
    double panelShear {0.0};     ///< |FSd| no painel de alma do pilar (N)
    double columnAxial {0.0};    ///< maior |N| dos trechos de pilar no no (N)
    double beamFlangeForce {0.0};   ///< |M|/z das vigas no plano, apenas informativo (N)
};

/// Resultante (Fx, Fy, Fz, Mx, My, Mz; N e N.mm, globais) das forcas e momentos que as barras exercem sobre o no. Nula em um no sem cargas aplicadas
/// (confere a convencao de sinais dos esforcos de extremidade).
std::array<double, 6> nodeResultant(const std::vector<MemberEnd> &members);

/// Cortante no painel de alma do pilar de uma ligacao rigida viga-pilar pelo equilibrio do no (NBR 8800:2024, 5.7.7.1: a forca transmitida
/// pelas mesas da viga somada algebricamente ao cortante ja atuante no pilar).
///
/// `members`: todas as barras que chegam ao no, `beamIndex`: a viga da ligacao, `hostIndex`: o pilar de apoio (sua direcao a partir do no define o eixo c).
/// Barras colineares com o pilar sao trechos de pilar (acima/abaixo); as demais sao vigas, cujos momentos (em torno de n = s x c) viram o binario das
/// mesas separadas de `lever` = d - tf (mm). A forca resultante sobre cada meia-alma do painel (mesas + cortante do pilar do mesmo lado) e o cortante do painel.
PanelResult panelShear(const std::vector<MemberEnd> &members, int beamIndex, int hostIndex, double lever);

} // namespace Structura::App::Services::NodeEquilibrium
