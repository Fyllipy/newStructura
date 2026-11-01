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
    // Centro de cisalhamento em relacao ao centroide (m), nos eixos locais y e z: acopla flexao e torcao quando a barra tem empenamento (secoes nao duplamente simetricas)
    double scY = 0.0;
    double scZ = 0.0;
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

// Carga geral de barra com posicao (distancias sobre o eixo, a partir do no I). kind: 0 = trapezoidal/parcial (v1, v2 = qx,qy,qz em a e em b),
// 1 = forca pontual em a (v1 = Fx,Fy,Fz), 2 = momento pontual em a (v1 = Mx,My,Mz), 3 = deformacao imposta em toda a barra, nos eixos locais
// (v1 = deformacao axial livre, d(eps)/dy, d(eps)/dz em 1/comprimento): temperatura = alfa * dT, retracao = eps_cs.
struct MemberActionData {
    int memberId = 0;
    int kind = 0;
    DistributedLoadSystem system = DistributedLoadSystem::Local;
    double a = 0.0;
    double b = 0.0;
    std::array<double, 3> v1{0.0, 0.0, 0.0};
    std::array<double, 3> v2{0.0, 0.0, 0.0};
};

// --- vinculos e ligacoes entre nos ---------------------------------------------------------------------------------------------------
// Diafragma rigido: os nos escravos acompanham o no mestre como corpo rigido no plano normal ao eixo global `normalAxis` (0 = X, 1 = Y, 2 = Z).
struct DiaphragmData {
    int masterNodeId = 0;
    std::vector<int> slaveNodeIds;
    int normalAxis = 2;
};

// Ligacao rigida entre dois nos. kind: 0 = corpo rigido de 6 graus de liberdade (translacao do escravo = a do mestre + theta x r);
// 1 = tres translacoes iguais; 2 = tres rotacoes iguais; 3 = um unico grau de liberdade igual (`dof`, 0..5).
struct RigidLinkData {
    int masterNodeId = 0;
    int slaveNodeId = 0;
    int kind = 0;
    int dof = 0;
};

// Apoio inclinado: restringe deslocamentos (bits 0..2 da mascara) e rotacoes (bits 3..5) segundo as direcoes `axes` (e1, e2, e3 em linhas, eixos globais).
struct InclinedSupportData {
    int nodeId = 0;
    std::array<double, 9> axes{1, 0, 0, 0, 1, 0, 0, 0, 1};
    int mask = 0;
};

// Vinculo linear geral: sum coef_j * u(no_j, dof_j) = 0
struct LinearConstraintData {
    std::vector<int> nodeIds;
    std::vector<int> dofs;
    std::vector<double> coefs;
    bool reportReaction = false;
};

// Molas de extremidade das barras (semirrigidez): rigidez em cada um dos 12 graus de liberdade (< 0 = sem mola; 0 = articulacao)
struct EndSpringData {
    int memberId = 0;
    std::array<double, 12> k{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
};

// Rotula plastica bilinear em uma rotacao de extremidade da barra (dof 3..5 em I, 9..11 em J)
struct PlasticHingeData {
    int memberId = 0;
    int dof = 4;
    double kElastic = 0.0;
    double yieldMoment = 0.0;
    double hardeningRatio = 0.0;
    double axialYield = 0.0;   // 0 = sem interacao com o esforco normal
    double exponent = 2.0;
};

// Lei de forca-deformacao de um grau de liberdade de uma ligacao entre dois nos. kind: 0 = linear (k); 1 = so tracao; 2 = so compressao;
// 3 = folga de tracao (age quando a deformacao excede p1); 4 = folga de compressao (age quando a deformacao e menor que -p1);
// 5 = elastoplastica bilinear simetrica (p1 = forca de escoamento, p2 = razao de enrijecimento); 6 = multilinear (pontos deformacao-forca em `table`,
// ordenados, extrapolada com a ultima inclinacao); 7 = rigida (k muito alta)
struct LinkLawData {
    int kind = 0;
    double k = 0.0;
    double p1 = 0.0;
    double p2 = 0.0;
    std::vector<std::pair<double, double>> table;
};

// Ligacao entre dois nos (mola/elemento de interface) com eixos locais x, y, z (linhas de `axes`, globais; vetor nulo = direcao I->J e eixos derivados)
struct LinkData {
    int id = 0;
    int nodeI = 0;
    int nodeJ = 0;
    std::array<double, 9> axes{0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<LinkLawData, 6> laws;   // ux uy uz rx ry rz relativos, nos eixos locais
};

// Controle da analise nao linear incremental (rotulas plasticas, ligacoes nao lineares)
struct NonlinearOptions {
    bool force = false;            // executar o motor incremental mesmo sem elementos nao lineares
    int steps = 10;
    int maxIterations = 40;
    double tolerance = 1e-6;       // relativa
    int criteria = 5;              // bit 0 = forcas, bit 1 = deslocamentos, bit 2 = energia
    int maxCutbacks = 8;
    int controlMode = 0;           // 0 = controle de carga; 1 = controle de deslocamento
    int controlNodeId = 0;
    int controlDof = 0;
    double controlTarget = 0.0;    // deslocamento alvo (controle de deslocamento)
    double loadFactorMax = 1.0;    // controle de carga: fator de carga final
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
    // Material ortotropico (ver fem::Material): E2, nu12, G12, multiplicadores de flexao nas direcoes 1 e 2 e o eixo 1 (global)
    bool orthotropic = false;
    double E2 = 0.0, nu12 = 0.3, G12 = 0.0, bend1 = 1.0, bend2 = 1.0;
    std::array<double, 3> axis{1.0, 0.0, 0.0};
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
    std::vector<MemberActionData> memberActions;
    std::vector<DiaphragmData> diaphragms;
    std::vector<RigidLinkData> rigidLinks;
    std::vector<InclinedSupportData> inclinedSupports;
    std::vector<LinearConstraintData> constraints;
    std::vector<EndSpringData> endSprings;
    std::vector<PlasticHingeData> plasticHinges;
    std::vector<LinkData> links;
    NonlinearOptions nonlinear;
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
    // Infraestrutura numerica. linearSolver: 0 = direto (LDL^T esparso, ordenacao AMD), 1 = gradiente conjugado precondicionado (Cholesky incompleta; cai no direto se nao
    // convergir). eigenSolver: 0 = automatico (denso ate eigenDenseLimit graus de liberdade livres, esparso acima), 1 = denso, 2 = esparso (iteracao em subespaco).
    int linearSolver = 0;
    int eigenSolver = 0;
    int eigenDenseLimit = 400;
    double iterativeTolerance = 1e-10;
    int iterativeMaxIterations = 5000;
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

struct HingeResult {
    int memberId = 0;
    int dof = 0;
    double plasticRotation = 0.0;
    double moment = 0.0;
    bool yielded = false;
};

struct LinkResult {
    int id = 0;
    std::array<double, 6> deformation{0, 0, 0, 0, 0, 0};
    std::array<double, 6> force{0, 0, 0, 0, 0, 0};
};

struct NonlinearSummary {
    bool used = false;
    bool converged = true;
    double loadFactor = 1.0;       // fator de carga alcancado
    int steps = 0;
    int iterations = 0;
    std::vector<std::array<double, 2>> curve;   // (fator de carga, deslocamento de controle) a cada passo confirmado
    std::vector<HingeResult> hinges;
    std::vector<LinkResult> links;
};

struct SolverOutput {
    // Avisos do solver (condicionamento, graus de liberdade sem rigidez, tirantes que nao estabilizam, segunda ordem resolvida por incrementos...)
    std::vector<std::string> warnings;
    NonlinearSummary nonlinear;
    std::vector<double> constraintForces;   // forca (penalidade x violacao) de cada linha de vinculo, na ordem de expansao
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
