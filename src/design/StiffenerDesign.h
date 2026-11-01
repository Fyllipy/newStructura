#pragma once

#include "design/Connections.h"

#include <string>
#include <vector>

namespace Structura::Design::Stiffeners {

/// Enrijecedor transversal de alma de viga ou pilar I/H (NBR 8800:2024, 5.7.9 e 5.4.3.1.3): verificacao e proposta de dimensoes.
struct StiffenerInput {
    // perfil
    double d {0.0};          ///< altura total (mm)
    double h {0.0};          ///< altura livre da alma entre as mesas (mm)
    double tw {0.0};
    double bf {0.0};         ///< largura da mesa que recebe a forca
    double tf {0.0};
    double fy {250.0};
    double fu {400.0};
    double E {200000.0};
    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
    // enrijecedor
    int sides {2};           ///< 1 = chapa de um lado da alma; 2 = par (um de cada lado)
    double b {0.0};          ///< largura de cada chapa (mm)
    double t {0.0};          ///< espessura (mm)
    double cut {25.0};       ///< recorte de canto junto a solda mesa-alma (mm)
    bool fitted {true};      ///< extremidade ajustada (usinada) em contato com a mesa carregada; falso: soldada
    bool interior {false};   ///< secao interna da barra (faixa de alma de 25 tw); falso: extremidade (12 tw), 5.7.9.4
    double weldLeg {0.0};    ///< perna do filete enrijecedor-alma (mm); 0 = nao verifica
    double fw {485.0};       ///< resistencia do metal da solda (MPa)
    // acao
    double force {0.0};      ///< forca localizada solicitante de calculo F_Sd (N, modulo)
    bool tension {false};    ///< a forca traciona a alma (5.7.9.1); falso: comprime (5.7.9.2/5.7.9.3)
    double bearing {100.0};  ///< n: comprimento de atuacao da forca na mesa (mm), para escoamento local e enrugamento
    double unbracedLength {0.0};   ///< l: comprimento destravado lateral para 5.7.5 (mm); 0 = mesas contidas
    bool atEnd {true};       ///< a forca atua a menos de d da extremidade da barra (apoio): usa as expressoes de extremidade de 5.7.3 e 5.7.4
};

struct StiffenerCheck {
    std::string id;
    std::string title;
    std::string clause;
    double demand {0.0};
    double resistance {0.0};
    std::string unit;
    double ratio {0.0};
    std::vector<std::string> lines;
};

struct StiffenerResult {
    bool required {false};            ///< a alma sozinha nao resiste: o enrijecedor e necessario (5.7.9)
    double webResistance {0.0};       ///< menor resistencia local da alma sem enrijecedor (N), 5.7.3, 5.7.4 e 5.7.5; 0 = nao se aplica
    std::string webGoverning;
    std::vector<StiffenerCheck> checks;
    double maxRatio {0.0};
};

/// Menor resistencia local da alma sem enrijecedor para a forca de `input` (compressao: 5.7.3, 5.7.4, 5.7.5; tracao: 5.7.2, 5.7.3).
double webLocalResistance(const StiffenerInput &input, std::string *governing = nullptr);

/// Verifica o enrijecedor: geometria (5.4.3.1.3-b, 5.7.9.5), flambagem como barra comprimida (5.7.9.4), esmagamento de extremidade ajustada (6.6.2),
/// tracao (5.7.9.1) e solda enrijecedor-alma pela diferenca entre a forca solicitante e a resistente da alma.
StiffenerResult checkStiffener(const StiffenerInput &input);

/// Enrijecedor intermediario de cortante (5.4.3.1.3): b/t, inercia minima a tw^3 j e distancia entre os pontos de solda; `a` = distancia entre enrijecedores (mm).
std::vector<StiffenerCheck> checkShearStiffener(const StiffenerInput &input, double a);

/// Menor chapa (espessuras comerciais, largura em passos de 5 mm) que atende todas as verificacoes de `input`; b e t em zero se nenhuma serve.
struct StiffenerProposal {
    double b {0.0};
    double t {0.0};
    bool found {false};
};
StiffenerProposal proposeStiffener(StiffenerInput input);

/// Enrijecedor transversal de alma de perfil formado a frio (NBR 14762:2010, 9.5): `web` = alma do perfil (h = altura da viga, t = espessura).
struct ColdStiffenerInput {
    double h {0.0};          ///< altura da viga (mm), comprimento efetivo de flambagem do enrijecedor
    double t {0.0};          ///< espessura da alma (mm)
    double fy {345.0};
    double E {200000.0};
    double gamma {1.10};     ///< 9.5.1
    int sides {2};           ///< 1 = chapa de um lado da alma; 2 = par
    double b {0.0};          ///< largura de cada chapa (mm)
    double ts {0.0};         ///< espessura do enrijecedor (mm)
    bool interior {false};   ///< secao intermediaria (faixa de 18 t); falso: extremidade/apoio (faixa de 10 t)
    double force {0.0};      ///< forca localizada de calculo (N)
    bool tension {false};    ///< enrijecedor tracionado (9.5.1-a)
};

/// 9.5.1: `b/ts <= 0,42 sqrt(E/fy)` (borda livre) e Ns,Rd = A fy/gamma (tracao) ou chi A fy/gamma (compressao), com a faixa de alma de 10 t ou 18 t e KL = h.
StiffenerResult checkColdStiffener(const ColdStiffenerInput &input);

/// 9.5.2: enrijecedor de cortante; `a` = distancia entre enrijecedores. a/h <= 3 e <= (260/(h/t))^2; Is >= max(5 h t^3 (h/a - 0,7 a/h) ; t (h/50)^4).
std::vector<StiffenerCheck> checkColdShearStiffener(const ColdStiffenerInput &input, double a);

/// Inercia da secao do enrijecedor (somente as chapas) em relacao ao eixo no plano medio da alma (mm4).
double plateInertiaAboutWebPlane(int sides, double b, double t, double tw);

} // namespace Structura::Design::Stiffeners
