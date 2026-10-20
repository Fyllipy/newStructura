#pragma once

#include "design/Connections.h"

#include <vector>

namespace Structura::Design::Connections {

/// Elemento de apoio de uma ligação de viga ou diagonal (pilar ou viga principal de perfil I/H) e a força que a ligação lhe transmite.
struct SupportInput {
    enum Face { None = 0, Flange = 1, Web = 2 };
    int face {None};          ///< onde a chapa está soldada: face externa da mesa ou face da alma
    double d {0.0};           ///< altura do perfil de apoio (mm)
    double bf {0.0};
    double tf {0.0};
    double tw {0.0};
    double k {0.0};           ///< mesa + raio de concordância com a alma (mm); 0 = 1,5 tf
    double fy {250.0};
    double fu {400.0};
    bool atMemberEnd {true};  ///< a ligação está na extremidade do perfil de apoio (sem continuidade para o outro lado)
    double tension {0.0};     ///< força normal à face que tende a arrancá-la (N)
    double compression {0.0}; ///< força normal à face que a comprime (N)
    double lengthAlong {0.0};      ///< comprimento de atuação ao longo do perfil de apoio, n (mm)
    double lengthTransverse {0.0}; ///< dimensão da área carregada transversal ao perfil (mm), p.ex. espessura da chapa mais soldas
    double unbracedLength {0.0};   ///< ℓ: maior comprimento destravado lateralmente na seção da força (mm); 0 = mesas contidas (5.7.5 dispensada)
    double gammaA1 {1.10};
};

/// Estados-limite locais do perfil de apoio: flexão local da mesa, escoamento local, enrugamento e flambagem lateral da alma
/// (NBR 8800:2024, 5.7.2 a 5.7.5) para a ligação na mesa; flexão transversal da alma (linhas de plastificação) para a ligação na alma.
std::vector<Check> checkSupportMember(const SupportInput &input);

/// Painel de alma do pilar em ligacao rigida viga-pilar (NBR 8800:2024, 5.7.7.1): forca cortante solicitante no painel
/// FSd = F_mesas - V_pilar contra a resistente FRd = VRd (NSd <= 0,4 Np) ou VRd (1,4 - NSd/Np) (NSd > 0,4 Np), com VRd de 5.4.3 para
/// Aw = dc tw. Chapas de reforco de alma (5.7.7.2, dos dois lados) somam-se a espessura da alma; enrijecedores transversais alinhados as mesas da
/// viga entram no kv (5.4.3.1.1).
struct ColumnPanelInput {
    double d {0.0};              ///< altura do pilar dc (mm)
    double bf {0.0};
    double tf {0.0};
    double tw {0.0};
    double radius {0.0};         ///< raio de concordancia mesa-alma (mm)
    double fy {250.0};
    double doubler {0.0};        ///< espessura total das chapas de reforco da alma (mm)
    double stiffenerSpacing {0.0};   ///< a: distancia entre os enrijecedores transversais do painel (mm); 0 = sem enrijecedores (kv = 5,34)
    double axial {0.0};          ///< NSd no pilar (N, modulo)
    double beamForce {0.0};      ///< forca transmitida pelas mesas da viga ao painel (N)
    double columnShear {0.0};    ///< cortante ja atuante no pilar, somado algebricamente (N); 0 = desprezado
    double panelShear {-1.0};    ///< >= 0: FSd do equilibrio do no (mesas da viga + cortantes do pilar, com sinal); substitui beamForce - columnShear
    double gammaA1 {1.10};
};
std::vector<Check> checkColumnPanel(const ColumnPanelInput &input);

} // namespace Structura::Design::Connections
