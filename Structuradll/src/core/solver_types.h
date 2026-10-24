#pragma once

#include <array>
#include <utility>
#include <string>
#include <vector>

enum class DistributedLoadSystem {
    Local = 0,
    Global = 1
};

struct MaterialData {
    int id = 0;
    double E = 0.0;
    double G = 0.0;
    double density = 0.0;
};

struct SectionData {
    int id = 0;
    double A = 0.0;
    double Iz = 0.0;
    double Iy = 0.0;
    double J = 0.0;
    // Areas efetivas de cisalhamento (m2) para a deformacao por cortante: Ay = cortante na direcao local y, Az = na direcao z (0 = rigida ao cortante)
    double Ay = 0.0;
    double Az = 0.0;
    // Constante de empenamento (m6) das secoes abertas de paredes finas: > 0 permite a torcao com empenamento (7o grau de liberdade) nas barras que a pedirem
    double Cw = 0.0;
};

struct NodeData {
    int id = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::array<int, 6> restraints{0, 0, 0, 0, 0, 0};
};

struct MemberData {
    int id = 0;
    int nodeI = 0;
    int nodeJ = 0;
    int materialId = 0;
    int sectionId = 0;
    // Section placement: rotation about the member x-axis (degrees) and rigid offsets
    // (m) of the member axis relative to node I / node J, in the rotated local y/z axes.
    double sectionRotationDeg = 0.0;
    double offsetY1 = 0.0;
    double offsetZ1 = 0.0;
    double offsetY2 = 0.0;
    double offsetZ2 = 0.0;
    // End releases: ux uy uz rx ry rz at node I then node J (non-zero = released)
    std::array<int, 12> releases{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    // Zonas rigidas nas extremidades (m, medidas sobre o eixo da barra a partir de cada no): so o trecho restante e flexivel
    double rigidI = 0.0;
    double rigidJ = 0.0;
    // Torcao com empenamento (Vlasov): 0 = so Saint-Venant; 1 = ativa (exige Cw > 0 na secao). Condicao de empenamento em cada extremidade:
    // 0 = automatica (impedido em no totalmente engastado, livre nos demais; barras colineares no no compartilham o empenamento), 1 = livre, 2 = impedido
    int warping = 0;
    int warpingStart = 0;
    int warpingEnd = 0;
    // Barra de secao I de alma variavel (NBR 8800, Anexo G): alturas nas extremidades I e J, mesas e alma constantes. O solver subdivide a barra em `segments`
    // trechos prismaticos (nos internos), com as propriedades da altura media de cada trecho, e devolve os esforcos das extremidades da barra original.
    bool taper = false;
    double taperD1 = 0.0;
    double taperD2 = 0.0;
    double taperBf = 0.0;
    double taperTw = 0.0;
    double taperTf = 0.0;
    int taperSegments = 8;
};

struct NodalLoadData {
    int nodeId = 0;
    std::array<double, 6> values{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
};

struct MemberLoadData {
    int memberId = 0;
    DistributedLoadSystem system = DistributedLoadSystem::Local;
    double qx = 0.0;
    double qy = 0.0;
    double qz = 0.0;
};

struct MemberMomentData {
    int memberId = 0;
    DistributedLoadSystem system = DistributedLoadSystem::Local;
    double mx = 0.0;
    double my = 0.0;
    double mz = 0.0;
};

// --- cascas e chapas acopladas as barras (elemento Q4 de Femdll/src/shell_q4) -----------------------------------------------------
struct ShellElementData {
    int id = 0;
    std::array<int, 4> nodeIds{0, 0, 0, 0};   // anti-horario visto do lado da normal positiva
    double E = 0.0;
    double nu = 0.3;
    double density = 0.0;                      // massa especifica (so modal)
    double alpha = 1.2e-5;                     // dilatacao termica (1/K), usada nas cargas de temperatura
    double thickness = 0.0;
};

// Carga por elemento de casca: kind 0 = pressao nodal v[0..3] (positiva no sentido da normal), 1 = forca de corpo global v[0..2],
// 2 = temperatura (v[0] uniforme, v[1] gradiente).
struct ShellLoadData {
    int shellId = 0;
    int kind = 0;
    std::array<double, 4> v{0.0, 0.0, 0.0, 0.0};
};

// No escravo preso a nos mestres por interpolacao (nos pendentes de malhas nao conformes, nos de barras sobre cascas).
struct TieData {
    int slaveId = 0;
    std::vector<int> masterIds;
    std::vector<double> weights;
    int dofMask = 63;   // bits 0..5 = ux uy uz rx ry rz ligados (7 = so translacoes: apoio articulado de casca em barra)
};

// Mola nodal (apoio elastico); compressionOnly: k[0] = rigidez normal, k[1] = tangencial; normal aponta do apoio para a estrutura.
struct SpringData {
    int nodeId = 0;
    std::array<double, 6> k{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    bool compressionOnly = false;
    std::array<double, 3> normal{0.0, 0.0, 1.0};
};

struct ShellResultData {
    int id = 0;
    std::array<double, 40> values{};   // 5 pontos (4 cantos + centro) x (nxx nyy nxy mxx myy mxy qx qy), eixos locais do elemento
    std::array<double, 9> axes{};      // e1, e2, e3 (global)
};

struct SolverInput {
    std::vector<MaterialData> materials;
    std::vector<SectionData> sections;
    std::vector<NodeData> nodes;
    std::vector<MemberData> members;
    std::vector<NodalLoadData> nodalLoads;
    std::vector<MemberLoadData> memberLoads;
    std::vector<MemberMomentData> memberMoments;
    std::vector<ShellElementData> shells;
    std::vector<ShellLoadData> shellLoads;
    std::vector<TieData> ties;
    std::vector<SpringData> springs;
    // Second-order (geometric non-linear) analysis by iterating K + Kg(N)
    bool secondOrder = false;
    // Tension-only members (ties/cables): deactivated while shortening, reactivated when elongating
    std::vector<int> tensionOnlyMembers;
    // Linear (elastic) buckling: number of modes to extract (0 = none); uses the first-order axial forces
    int bucklingModes = 0;
    // Modal analysis (free vibration): number of modes and additional lumped nodal masses (translational)
    int modalModes = 0;
    std::vector<std::pair<int, double>> extraNodalMasses;   // node id, mass
    int maxIterations = 30;
    double tolerance = 1e-7;
    // Deformacao por cortante (Timoshenko) nas barras cuja secao tem areas de cisalhamento
    bool shearDeformation = false;
    // Segunda ordem pelas funcoes de estabilidade (Livesley) em vez da rigidez geometrica consistente (P-delta e P-delta local exatos na barra)
    bool stabilityFunctions = false;
    // Segunda ordem: 0 = iteracao direta com incremento de carga automatico se falhar; n > 1 = sempre em n passos de carga
    int loadSteps = 0;
};

struct NodeResult {
    int id = 0;
    std::array<double, 6> displacements{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 6> reactions{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
};

struct MemberResult {
    int id = 0;
    int nodeI = 0;
    int nodeJ = 0;
    std::array<double, 12> localForces{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 3> auxiliaryK{0.0, 0.0, 0.0};
    // Torcao com empenamento: taxa de torcao theta' nas extremidades (rad/m) e forcas generalizadas conjugadas (N.m2: bimomento aplicado a barra, B = -E Cw theta'')
    bool hasWarping = false;
    std::array<double, 2> warpingRate{0.0, 0.0};
    std::array<double, 2> warpingForce{0.0, 0.0};
};

struct SolverOutput {
    // Avisos do solver (condicionamento, graus de liberdade sem rigidez, tirantes que nao estabilizam, segunda ordem resolvida por incrementos...)
    std::vector<std::string> warnings;
    std::vector<NodeResult> nodes;
    std::vector<MemberResult> members;
    std::vector<ShellResultData> shells;
    // Critical load factors (ascending) and mode shapes: modes[m] has 6 values per node (input order)
    std::vector<double> bucklingFactors;
    std::vector<std::vector<double>> bucklingModes;
    // Modal results: circular frequencies (rad/s, ascending), mode shapes (6 per node) and effective modal mass fractions (X,Y,Z)
    std::vector<double> modalOmega;
    std::vector<std::vector<double>> modalShapes;
    std::vector<std::array<double, 3>> modalMassRatio;
};
