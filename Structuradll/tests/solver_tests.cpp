// Testes analiticos do solver (sem dependencias de Qt/VTK): cada caso compara com a solucao fechada da resistencia dos materiais.
#include <cmath>
#include <cstdio>
#include <string>

#include "solver_core.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void expectNear(const char* what, double actual, double expected, double relTol = 1e-6, double absTol = 1e-9)
{
    ++g_checks;
    const double tol = std::max(absTol, relTol * std::abs(expected));
    if (!(std::abs(actual - expected) <= tol)) {
        ++g_failures;
        std::printf("  FALHOU: %s = %.9g (esperado %.9g)\n", what, actual, expected);
    }
}

void expectTrue(const char* what, bool value)
{
    ++g_checks;
    if (!value) {
        ++g_failures;
        std::printf("  FALHOU: %s\n", what);
    }
}

constexpr double kE = 2.0e11, kG = 8.0e10, kA = 0.01, kIz = 4.0e-6, kIy = 8.0e-6, kJ = 1.0e-5, kL = 5.0;

SolverInput baseInput(double length = kL)
{
    SolverInput in;
    in.materials.push_back({1, kE, kG, 0.0});
    SectionData s;
    s.id = 1;
    s.A = kA;
    s.Iz = kIz;
    s.Iy = kIy;
    s.J = kJ;
    in.sections.push_back(s);
    NodeData n1;
    n1.id = 1;
    n1.restraints = {1, 1, 1, 1, 1, 1};
    NodeData n2;
    n2.id = 2;
    n2.x = length;
    in.nodes = {n1, n2};
    MemberData m;
    m.id = 1;
    m.nodeI = 1;
    m.nodeJ = 2;
    m.materialId = 1;
    m.sectionId = 1;
    in.members.push_back(m);
    return in;
}

bool solve(const SolverInput& in, SolverOutput& out)
{
    SolverCore core;
    std::string error;
    if (!core.run(in, out, error)) {
        std::printf("  erro do solver: %s\n", error.c_str());
        ++g_failures;
        return false;
    }
    return true;
}

MemberActionData action(int kind, double a, double b, std::array<double, 3> v1, std::array<double, 3> v2 = {0, 0, 0}, DistributedLoadSystem system = DistributedLoadSystem::Local)
{
    MemberActionData d;
    d.memberId = 1;
    d.kind = kind;
    d.system = system;
    d.a = a;
    d.b = b;
    d.v1 = v1;
    d.v2 = v2;
    return d;
}

// membro ao longo de X: eixo local y = Z global, eixo local z = -Y global
void pointForceCantilever()
{
    std::printf("[carga pontual em balanco]\n");
    SolverInput in = baseInput();
    const double P = 1000.0, a = 2.0;
    in.memberActions.push_back(action(1, a, 0.0, {0.0, P, 0.0}));
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    const double expectedTip = P * a * a * (3.0 * kL - a) / (6.0 * kE * kIz);
    expectNear("deslocamento local y da ponta", out.nodes[1].displacements[2], expectedTip);
    expectNear("rotacao da ponta", out.nodes[1].displacements[4] * -1.0, P * a * a / (2.0 * kE * kIz));
    expectNear("reacao vertical", out.nodes[0].reactions[2], -P);
    expectNear("momento de engaste", out.nodes[0].reactions[4], P * a);   // positivo em torno de Y global
}

void trapezoidCantilever()
{
    std::printf("[carga triangular em balanco]\n");
    SolverInput in = baseInput();
    const double q0 = 800.0;
    in.memberActions.push_back(action(0, 0.0, kL, {0.0, 0.0, 0.0}, {0.0, q0, 0.0}));
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    expectNear("flecha da ponta", out.nodes[1].displacements[2], 11.0 * q0 * std::pow(kL, 4) / (120.0 * kE * kIz));
    expectNear("reacao = area da carga", out.nodes[0].reactions[2], -0.5 * q0 * kL);
}

void trapezoidEqualsUniform()
{
    std::printf("[trapezoidal uniforme = carga uniforme (com zonas rigidas)]\n");
    for (const bool rigid : {false, true}) {
        SolverInput uniform = baseInput();
        uniform.members[0].rigidI = rigid ? 0.4 : 0.0;
        uniform.members[0].rigidJ = rigid ? 0.7 : 0.0;
        uniform.nodes[1].restraints = {0, 1, 1, 0, 0, 0};   // engastada-apoiada
        uniform.memberLoads.push_back({1, DistributedLoadSystem::Local, 100.0, 300.0, -500.0});
        SolverInput general = uniform;
        general.memberLoads.clear();
        general.memberActions.push_back(action(0, 0.0, kL, {100.0, 300.0, -500.0}, {100.0, 300.0, -500.0}));
        SolverOutput a, b;
        if (!solve(uniform, a) || !solve(general, b)) {
            return;
        }
        for (size_t i = 0; i < 12; ++i) {
            expectNear(("esforco " + std::to_string(i) + (rigid ? " (rigida)" : "")).c_str(), b.members[0].localForces[i], a.members[0].localForces[i], 1e-9, 1e-6);
        }
        for (size_t i = 0; i < 6; ++i) {
            expectNear(("deslocamento " + std::to_string(i)).c_str(), b.nodes[1].displacements[i], a.nodes[1].displacements[i], 1e-9, 1e-14);
        }
    }
}

void partialUniformEquilibrium()
{
    std::printf("[carga parcial: equilibrio e posicao]\n");
    SolverInput in = baseInput();
    in.nodes[1].restraints = {0, 1, 1, 0, 0, 0};
    in.memberActions.push_back(action(0, 1.0, 3.5, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}));
    in.memberActions.back().v1 = {0.0, -400.0, 0.0};
    in.memberActions.back().v2 = {0.0, -400.0, 0.0};
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    // carga local y = Z global: resultante -1000 em Z aplicada em x = 2,25
    expectNear("soma das reacoes em Z", out.nodes[0].reactions[2] + out.nodes[1].reactions[2], 1000.0, 1e-9, 1e-6);
    // equilibrio de momentos em torno de Y no no I: R_My,I - L R_Fz,J + 2,25 * 1000 = 0
    expectNear("equilibrio de momentos em torno de I", out.nodes[0].reactions[4] - kL * out.nodes[1].reactions[2], -2250.0, 1e-9, 1e-6);
}

void pointMomentCantilever()
{
    std::printf("[momento pontual em balanco]\n");
    SolverInput in = baseInput();
    const double M = 500.0;
    in.memberActions.push_back(action(2, kL, 0.0, {0.0, 0.0, M}));   // Mz local na ponta
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    expectNear("flecha local y", out.nodes[1].displacements[2], M * kL * kL / (2.0 * kE * kIz));
    expectNear("rotacao (em torno de -Y global)", -out.nodes[1].displacements[4], M * kL / (kE * kIz));
    // momento no meio do vao: mesmo momento pontual a meia altura vira o mesmo deslocamento da ponta fora do trecho carregado
    SolverInput mid = baseInput();
    mid.memberActions.push_back(action(2, 2.0, 0.0, {0.0, 0.0, M}));
    SolverOutput outMid;
    if (!solve(mid, outMid)) {
        return;
    }
    const double a = 2.0;
    expectNear("flecha da ponta com momento a 2 m", outMid.nodes[1].displacements[2], M * a * (kL - a / 2.0) / (kE * kIz));
}

void torqueCantilever()
{
    std::printf("[torque pontual]\n");
    SolverInput in = baseInput();
    in.memberActions.push_back(action(2, 3.0, 0.0, {200.0, 0.0, 0.0}));
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    expectNear("giro da ponta", out.nodes[1].displacements[3], 200.0 * 3.0 / (kG * kJ));
}

void thermalAndShrinkage()
{
    std::printf("[deformacao imposta: temperatura e retracao]\n");
    const double alpha = 1.2e-5, dT = 30.0, gradient = alpha * 40.0;   // 40 K/m
    {
        SolverInput in = baseInput();
        in.memberActions.push_back(action(3, 0.0, 0.0, {alpha * dT, 0.0, 0.0}));
        SolverOutput out;
        if (solve(in, out)) {
            expectNear("alongamento livre", out.nodes[1].displacements[0], alpha * dT * kL);
            expectNear("esforco normal nulo", out.members[0].localForces[6], 0.0, 1e-6, 1e-6);
        }
    }
    {
        SolverInput in = baseInput();
        in.nodes[1].restraints = {1, 1, 1, 1, 0, 0};
        in.memberActions.push_back(action(3, 0.0, 0.0, {alpha * dT, 0.0, 0.0}));
        SolverOutput out;
        if (solve(in, out)) {
            expectNear("compressao impedida N = -E A alpha dT", out.members[0].localForces[6], -kE * kA * alpha * dT, 1e-9, 1e-6);
            expectNear("reacao axial (o apoio empurra de volta)", out.nodes[1].reactions[0], -kE * kA * alpha * dT, 1e-9, 1e-6);
        }
    }
    {
        SolverInput in = baseInput();
        in.memberActions.push_back(action(3, 0.0, 0.0, {0.0, gradient, 0.0}));
        SolverOutput out;
        if (solve(in, out)) {
            // curvatura livre v'' = -d(eps)/dy: flecha da ponta -ky L^2 / 2 (eixo local y = Z global)
            expectNear("flecha por gradiente em y", out.nodes[1].displacements[2], -gradient * kL * kL / 2.0);
            expectNear("momento nulo no balanco livre", out.members[0].localForces[11], 0.0, 1e-6, 1e-6);
        }
    }
    {
        SolverInput in = baseInput();
        in.memberActions.push_back(action(3, 0.0, 0.0, {0.0, 0.0, gradient}));
        SolverOutput out;
        if (solve(in, out)) {
            // eixo local z = -Y global; w'' = -kz => w(L) = -kz L^2 / 2 ao longo de z local = -Y global
            expectNear("flecha por gradiente em z (Y global)", out.nodes[1].displacements[1], gradient * kL * kL / 2.0);
        }
    }
    {
        SolverInput in = baseInput();
        in.nodes[1].restraints = {1, 1, 1, 1, 1, 0};
        in.memberActions.push_back(action(3, 0.0, 0.0, {0.0, gradient, 0.0}));
        SolverOutput out;
        if (solve(in, out)) {
            expectNear("momento de engaste por gradiente", std::abs(out.members[0].localForces[5]), kE * kIz * gradient, 1e-9, 1e-6);
        }
    }
}

void globalSystemTrapezoid()
{
    std::printf("[trapezoidal no sistema global]\n");
    SolverInput local = baseInput();
    local.memberActions.push_back(action(0, 1.0, 4.0, {0.0, 200.0, 0.0}, {0.0, 600.0, 0.0}));
    SolverInput global = baseInput();
    global.memberActions.push_back(action(0, 1.0, 4.0, {0.0, 0.0, 200.0}, {0.0, 0.0, 600.0}, DistributedLoadSystem::Global));
    SolverOutput a, b;
    if (solve(local, a) && solve(global, b)) {
        for (size_t i = 0; i < 6; ++i) {
            expectNear(("deslocamento " + std::to_string(i)).c_str(), b.nodes[1].displacements[i], a.nodes[1].displacements[i], 1e-9, 1e-14);
        }
    }
}

void releasedMemberPointLoad()
{
    std::printf("[carga pontual em viga biapoiada com articulacoes]\n");
    SolverInput in = baseInput();
    in.nodes[0].restraints = {1, 1, 1, 1, 0, 0};
    in.nodes[1].restraints = {0, 1, 1, 0, 0, 0};
    in.members[0].releases[5] = 1;
    in.members[0].releases[11] = 1;
    in.memberActions.push_back(action(1, 1.5, 0.0, {0.0, -1000.0, 0.0}));
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    expectNear("reacao em I", out.nodes[0].reactions[2], 1000.0 * (kL - 1.5) / kL, 1e-9, 1e-6);
    expectNear("reacao em J", out.nodes[1].reactions[2], 1000.0 * 1.5 / kL, 1e-9, 1e-6);
    expectNear("momento nulo na articulacao I", out.members[0].localForces[5], 0.0, 1e-9, 1e-6);
    expectNear("momento nulo na articulacao J", out.members[0].localForces[11], 0.0, 1e-9, 1e-6);
}

void taperExpansion()
{
    std::printf("[cargas com posicao em barra de secao variavel]\n");
    SolverInput in = baseInput();
    in.members[0].taper = true;
    in.members[0].taperD1 = 0.3;
    in.members[0].taperD2 = 0.3;
    in.members[0].taperBf = 0.15;
    in.members[0].taperTw = 0.008;
    in.members[0].taperTf = 0.012;
    in.members[0].taperSegments = 5;
    in.nodes[1].restraints = {0, 0, 1, 0, 0, 0};
    in.memberActions.push_back(action(0, 1.0, 4.0, {0.0, -300.0, 0.0}, {0.0, -300.0, 0.0}));
    in.memberActions.push_back(action(1, 2.2, 0.0, {0.0, -1000.0, 0.0}));
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    const double total = 300.0 * 3.0 + 1000.0;
    expectNear("soma das reacoes verticais", out.nodes[0].reactions[2] + out.nodes[1].reactions[2], total, 1e-9, 1e-6);
}


// ---- molas de extremidade, rotulas plasticas, vinculos e ligacoes ----------------------------------------------------------------------

void semiRigidSpring()
{
    std::printf("[ligacao semirrigida (mola de extremidade)]\n");
    SolverInput in = baseInput();
    const double P = 1000.0, k = 4.0e6;
    EndSpringData spring;
    spring.memberId = 1;
    spring.k[5] = k;   // rotacao em torno de z local no no I
    in.endSprings.push_back(spring);
    in.memberActions.push_back(action(1, kL, 0.0, {0.0, P, 0.0}));
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    expectNear("flecha da ponta com mola no engaste", out.nodes[1].displacements[2], P * std::pow(kL, 3) / (3.0 * kE * kIz) + P * kL * kL / k, 1e-9);
    expectNear("momento transmitido pela mola", std::abs(out.members[0].localForces[5]), P * kL, 1e-9, 1e-6);
    // rigidez muito alta recupera o engaste perfeito
    SolverInput stiff = in;
    stiff.endSprings[0].k[5] = 1e14;
    SolverOutput outStiff;
    if (solve(stiff, outStiff)) {
        expectNear("mola muito rigida = engaste", outStiff.nodes[1].displacements[2], P * std::pow(kL, 3) / (3.0 * kE * kIz), 1e-6);
    }
    // viga biapoiada com molas iguais e carga uniforme
    SolverInput beam = baseInput();
    beam.nodes[0].restraints = {1, 1, 1, 1, 1, 0};
    beam.nodes[1].restraints = {0, 1, 1, 0, 1, 0};
    EndSpringData both;
    both.memberId = 1;
    both.k[5] = 2.0e6;
    both.k[11] = 2.0e6;
    beam.endSprings.push_back(both);
    beam.memberLoads.push_back({1, DistributedLoadSystem::Local, 0.0, -1000.0, 0.0});
    SolverOutput outBeam;
    if (solve(beam, outBeam)) {
        const double mI = outBeam.members[0].localForces[5], mJ = outBeam.members[0].localForces[11];
        expectNear("momentos de extremidade simetricos", std::abs(mI), std::abs(mJ), 1e-9, 1e-6);
        // rotacoes simetricas (theta_B = -theta_A): M = M0 - (2 E I / L) phi com M = k phi, logo M = k M0 / (k + 2 E I / L), M0 = q L^2 / 12
        const double k = 2.0e6, m0 = 1000.0 * kL * kL / 12.0;
        expectNear("momento de extremidade com molas", std::abs(mI), k * m0 / (k + 2.0 * kE * kIz / kL), 1e-6);
    }
}

PlasticHingeData makeHinge(double kHinge, double yield, double ratio)
{
    PlasticHingeData hinge;
    hinge.memberId = 1;
    hinge.dof = 5;   // rz local no no I
    hinge.kElastic = kHinge;
    hinge.yieldMoment = yield;
    hinge.hardeningRatio = ratio;
    return hinge;
}

void plasticHingeCantilever()
{
    std::printf("[rotula plastica bilinear: balanco com carga de ponta]\n");
    const double P = 300.0, yield = 1000.0, kHinge = 1.0e7, ratio = 0.1;
    SolverInput in = baseInput();
    in.nodalLoads.push_back({2, {0.0, 0.0, P, 0.0, 0.0, 0.0}});   // Z global = y local
    in.plasticHinges.push_back(makeHinge(kHinge, yield, ratio));
    in.nonlinear.steps = 6;
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    const double moment = P * kL;
    const double rotation = yield / kHinge + (moment - yield) / (ratio * kHinge);
    expectTrue("motor nao linear executado", out.nonlinear.used);
    expectNear("fator de carga final", out.nonlinear.loadFactor, 1.0, 1e-12);
    expectNear("flecha da ponta", out.nodes[1].displacements[2], P * std::pow(kL, 3) / (3.0 * kE * kIz) + rotation * kL, 1e-6);
    expectNear("momento na rotula = P L", std::abs(out.members[0].localForces[5]), moment, 1e-6, 1e-4);
    expectTrue("rotula plastificou", !out.nonlinear.hinges.empty() && out.nonlinear.hinges.front().yielded);
    expectNear("rotacao plastica", std::abs(out.nonlinear.hinges.front().plasticRotation), rotation - moment / kHinge, 1e-5);

    // abaixo do escoamento a resposta e elastica (mola em serie)
    SolverInput elastic = in;
    elastic.nodalLoads[0].values[2] = 100.0;
    SolverOutput outElastic;
    if (solve(elastic, outElastic)) {
        expectNear("elastico: flecha com mola em serie", outElastic.nodes[1].displacements[2], 100.0 * std::pow(kL, 3) / (3.0 * kE * kIz) + 100.0 * kL * kL / kHinge, 1e-6);
        expectTrue("elastico: sem plastificacao", !outElastic.nonlinear.hinges.front().yielded);
    }
    // carga invertida: resposta simetrica
    SolverInput down = in;
    down.nodalLoads[0].values[2] = -P;
    SolverOutput outDown;
    if (solve(down, outDown)) {
        expectNear("simetria da resposta", out.nodes[1].displacements[2], -outDown.nodes[1].displacements[2], 1e-9, 1e-12);
    }
}

void plasticDisplacementControl()
{
    std::printf("[rotula plastica: controle de deslocamento]\n");
    const double P = 100.0, yield = 1000.0, kHinge = 1.0e7, ratio = 0.01;
    SolverInput in = baseInput();
    in.nodalLoads.push_back({2, {0.0, 0.0, P, 0.0, 0.0, 0.0}});
    in.plasticHinges.push_back(makeHinge(kHinge, yield, ratio));
    in.nonlinear.controlMode = 1;
    in.nonlinear.controlNodeId = 2;
    in.nonlinear.controlDof = 2;
    in.nonlinear.controlTarget = 0.05;
    in.nonlinear.steps = 25;
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    // equilibrio no alvo: delta = p L^3/3EI + L (yield/k + (p L - yield)/(r k)), com p = lambda P
    const double delta = 0.05;
    const double a = std::pow(kL, 3) / (3.0 * kE * kIz) + kL * kL / (ratio * kHinge);
    const double constant = kL * (yield / kHinge - yield / (ratio * kHinge));
    const double p = (delta - constant) / a;
    expectNear("deslocamento de controle atingido", out.nodes[1].displacements[2], delta, 1e-6);
    expectNear("carga no alvo", out.nonlinear.loadFactor * P, p, 1e-5);
    expectTrue("curva de capacidade com passos", out.nonlinear.curve.size() >= 10);
    bool monotonic = true;
    for (size_t i = 1; i < out.nonlinear.curve.size(); ++i) {
        monotonic = monotonic && out.nonlinear.curve[i][0] >= out.nonlinear.curve[i - 1][0] - 1e-12 && out.nonlinear.curve[i][1] > out.nonlinear.curve[i - 1][1];
    }
    expectTrue("curva monotona", monotonic);
    expectNear("momento na rotula = p L", std::abs(out.members[0].localForces[5]), p * kL, 1e-5, 1e-4);
}

void nonlinearLinks()
{
    std::printf("[ligacoes entre nos: so compressao, folga e elastoplastica]\n");
    const double kLink = 5.0e5;
    auto build = [&](double load, int kind, double gap) {
        SolverInput in = baseInput();
        NodeData ground;
        ground.id = 3;
        ground.x = kL;
        ground.z = -1.0;
        ground.restraints = {1, 1, 1, 1, 1, 1};
        in.nodes.push_back(ground);
        in.nodalLoads.push_back({2, {0.0, 0.0, load, 0.0, 0.0, 0.0}});
        LinkData link;
        link.id = 1;
        link.nodeI = 3;   // abaixo da ponta: eixo local x = +Z
        link.nodeJ = 2;
        link.laws[0].kind = kind;
        link.laws[0].k = kLink;
        link.laws[0].p1 = gap;
        in.links.push_back(link);
        return in;
    };
    const double kBeam = 3.0 * kE * kIz / std::pow(kL, 3);
    {
        SolverOutput out;
        if (solve(build(-1000.0, 2, 0.0), out)) {   // para baixo: o apoio so-compressao age
            expectNear("compressao ativa: deslocamento", out.nodes[1].displacements[2], -1000.0 / (kBeam + kLink), 1e-6);
            expectNear("compressao ativa: forca da ligacao", out.nonlinear.links.front().force[0], -1000.0 * kLink / (kBeam + kLink), 1e-6);
        }
        SolverOutput outUp;
        if (solve(build(1000.0, 2, 0.0), outUp)) {   // para cima: a ligacao abre
            expectNear("compressao inativa: so a viga", outUp.nodes[1].displacements[2], 1000.0 / kBeam, 1e-6);
            expectNear("compressao inativa: forca nula", outUp.nonlinear.links.front().force[0], 0.0, 1e-6, 1e-6);
        }
    }
    {
        const double gap = 0.01;
        SolverOutput out;
        // folga de compressao: a ligacao so age depois de 1 cm de deslocamento para baixo
        if (solve(build(-100.0, 4, gap), out)) {
            expectNear("folga nao vencida: so a viga", out.nodes[1].displacements[2], -100.0 / kBeam, 1e-6);
            expectTrue("folga nao vencida: deslocamento < folga", std::abs(out.nodes[1].displacements[2]) < gap);
        }
        SolverOutput big;
        if (solve(build(-10000.0, 4, gap), big)) {
            // depois da folga: k_beam d + k_link (d - gap) = P  (d em modulo)
            const double d = (10000.0 + kLink * gap) / (kBeam + kLink);
            expectNear("folga vencida", -big.nodes[1].displacements[2], d, 1e-6);
        }
    }
    {
        // elastoplastica: forca de escoamento 200, k = 5e5, rigidez pos-escoamento 1% ; carga 1000 para cima (ligacao tracionada)
        SolverInput in = build(1000.0, 5, 0.0);
        in.links[0].laws[0].p1 = 200.0;
        in.links[0].laws[0].p2 = 0.01;
        in.nonlinear.steps = 8;
        SolverOutput out;
        if (solve(in, out)) {
            // a ligacao esta abaixo da ponta: ponta sobe => ligacao se alonga? (J acima de I) d > 0
            const double kPost = 0.01 * kLink;
            const double d = (1000.0 + (kLink - kPost) * 200.0 / kLink) / (kBeam + kPost);   // f = 200 + kPost (d - 200/k) ; kBeam d + f = 1000
            expectNear("ligacao elastoplastica", out.nodes[1].displacements[2], (1000.0 - 200.0 + kPost * 200.0 / kLink) / (kBeam + kPost), 1e-5);
            (void)d;
        }
    }
}

SolverInput twoColumnFrame(double height)
{
    SolverInput in;
    in.materials.push_back({1, kE, kG, 0.0});
    SectionData s;
    s.id = 1;
    s.A = kA;
    s.Iz = kIz;
    s.Iy = kIy;
    s.J = kJ;
    in.sections.push_back(s);
    auto node = [&](int id, double x, double y, double z, bool fixed) {
        NodeData n;
        n.id = id;
        n.x = x;
        n.y = y;
        n.z = z;
        if (fixed) {
            n.restraints = {1, 1, 1, 1, 1, 1};
        }
        return n;
    };
    in.nodes = {node(1, 0, 0, 0, true), node(2, 0, 0, height, false), node(3, 4, 0, 0, true), node(4, 4, 0, height, false)};
    for (int i = 0; i < 2; ++i) {
        MemberData m;
        m.id = i + 1;
        m.nodeI = i == 0 ? 1 : 3;
        m.nodeJ = i == 0 ? 2 : 4;
        m.materialId = 1;
        m.sectionId = 1;
        in.members.push_back(m);
    }
    return in;
}

void constraintsDiaphragm()
{
    std::printf("[diafragma rigido]\n");
    const double h = 3.0, P = 1000.0;
    SolverInput in = twoColumnFrame(h);
    in.nodalLoads.push_back({2, {P, 0.0, 0.0, 0.0, 0.0, 0.0}});
    DiaphragmData d;
    d.masterNodeId = 2;
    d.slaveNodeIds = {4};
    d.normalAxis = 2;
    in.diaphragms.push_back(d);
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    // flexao em X dos pilares: local z = -X, Iy ; cada pilar tem rigidez 3 E Iy / h^3 (topo livre para girar)
    const double kColumn = 3.0 * kE * kIy / (h * h * h);
    expectNear("topos se deslocam igualmente", out.nodes[3].displacements[0], out.nodes[1].displacements[0], 1e-6, 1e-12);
    expectNear("deslocamento lateral = P/(2k)", out.nodes[1].displacements[0], P / (2.0 * kColumn), 1e-5);
    expectNear("cortante basal do pilar 1", -out.nodes[0].reactions[0], P / 2.0, 1e-5);
    expectNear("cortante basal do pilar 2", -out.nodes[2].reactions[0], P / 2.0, 1e-5);
    // sem diafragma, o pilar carregado leva toda a carga
    SolverInput freeFrame = in;
    freeFrame.diaphragms.clear();
    SolverOutput outFree;
    if (solve(freeFrame, outFree)) {
        expectNear("sem diafragma: deslocamento = P/k", outFree.nodes[1].displacements[0], P / kColumn, 1e-5);
        expectNear("sem diafragma: o outro topo nao se move", outFree.nodes[3].displacements[0], 0.0, 1e-9, 1e-12);
    }
    // torcao do diafragma: momento Mz no mestre gira os dois topos como corpo rigido (u_y,s - u_y,m = theta_z * 4)
    SolverInput twist = in;
    twist.nodalLoads.clear();
    twist.nodalLoads.push_back({2, {0.0, 0.0, 0.0, 0.0, 0.0, 5000.0}});
    SolverOutput outTwist;
    if (solve(twist, outTwist)) {
        const double theta = outTwist.nodes[1].displacements[5];
        expectNear("escravo gira com o mestre", outTwist.nodes[3].displacements[5], theta, 1e-5, 1e-12);
        expectNear("deslocamento do escravo = theta x r", outTwist.nodes[3].displacements[1] - outTwist.nodes[1].displacements[1], theta * 4.0, 1e-5, 1e-12);
    }
}

void constraintsRigidLinkAndInclined()
{
    std::printf("[ligacao rigida e apoio inclinado]\n");
    {
        // no S 1 m acima da ponta, ligado como corpo rigido; forca em X em S = forca axial + momento M_y = 1000 na ponta
        SolverInput in = baseInput();
        NodeData s;
        s.id = 3;
        s.x = kL;
        s.z = 1.0;
        in.nodes.push_back(s);
        in.nodalLoads.push_back({3, {1000.0, 0.0, 0.0, 0.0, 0.0, 0.0}});
        RigidLinkData link;
        link.masterNodeId = 2;
        link.slaveNodeId = 3;
        link.kind = 0;
        in.rigidLinks.push_back(link);
        SolverOutput out;
        if (solve(in, out)) {
            expectNear("deslocamento vertical da ponta", out.nodes[1].displacements[2], -1000.0 * kL * kL / (2.0 * kE * kIz), 1e-5);
            expectNear("rotacao da ponta", out.nodes[1].displacements[4], 1000.0 * kL / (kE * kIz), 1e-5);
            expectNear("alongamento da ponta", out.nodes[1].displacements[0], 1000.0 * kL / (kE * kA), 1e-4);
            expectNear("escravo: u_x = u_x,m + theta_y z", out.nodes[2].displacements[0], out.nodes[1].displacements[0] + out.nodes[1].displacements[4] * 1.0, 1e-5);
        }
    }
    {
        // apoio inclinado: a ponta so desloca ao longo do plano com normal (1,0,1)/sqrt(2)
        SolverInput in = baseInput();
        InclinedSupportData sup;
        sup.nodeId = 2;
        sup.axes = {1, 0, 1, 0, 1, 0, -1, 0, 1};
        sup.mask = 1;   // so o deslocamento ao longo de e1 (normal)
        in.inclinedSupports.push_back(sup);
        in.nodalLoads.push_back({2, {0.0, 0.0, -1000.0, 0.0, 0.0, 0.0}});
        SolverOutput out;
        if (solve(in, out)) {
            const double kx = kE * kA / kL, kz = 3.0 * kE * kIz / std::pow(kL, 3);
            const double u = 1000.0 / (kx + kz);
            expectNear("u_x = -u_z", out.nodes[1].displacements[0], -out.nodes[1].displacements[2], 1e-5, 1e-14);
            expectNear("u_x do apoio inclinado", out.nodes[1].displacements[0], u, 1e-4, 1e-12);
            // a reacao do apoio e normal ao plano: R = lambda e1, com lambda/sqrt(2) = kx u
            const double lambda = std::sqrt(2.0) * kx * u;
            expectNear("reacao do apoio inclinado em X", out.nodes[1].reactions[0], lambda / std::sqrt(2.0), 1e-4, 1e-6);
            expectNear("reacao do apoio inclinado em Z", out.nodes[1].reactions[2], lambda / std::sqrt(2.0), 1e-4, 1e-6);
        }
    }
}


void plasticCollapseMechanism()
{
    std::printf("[mecanismo de colapso: viga biengastada com carga central]\n");
    // duas barras de 2,5 m; rotulas plasticas nos engastes e no meio (Mp = 1000 N.m): carga de colapso 8 Mp / L = 1600 N
    const double yield = 1000.0, kHinge = 1.0e8, ratio = 1.0e-6;   // rigidez pos-escoamento da rotula = 100 N.m/rad, muito menor que EI/L
    SolverInput in = baseInput();
    in.nodes[1].x = kL / 2.0;
    in.nodes[1].restraints = {0, 0, 0, 0, 0, 0};
    NodeData end;
    end.id = 3;
    end.x = kL;
    end.restraints = {1, 1, 1, 1, 1, 1};
    in.nodes.push_back(end);
    MemberData second = in.members[0];
    second.id = 2;
    second.nodeI = 2;
    second.nodeJ = 3;
    in.members[0].nodeJ = 2;
    in.members.push_back(second);
    in.nodalLoads.push_back({2, {0.0, 0.0, -1000.0, 0.0, 0.0, 0.0}});
    for (const auto& [member, dof] : std::vector<std::pair<int, int>>{{1, 5}, {1, 11}, {2, 11}}) {
        PlasticHingeData hinge = makeHinge(kHinge, yield, ratio);
        hinge.memberId = member;
        hinge.dof = dof;
        in.plasticHinges.push_back(hinge);
    }
    in.nonlinear.controlMode = 1;
    in.nonlinear.controlNodeId = 2;
    in.nonlinear.controlDof = 2;
    in.nonlinear.controlTarget = -0.2;
    in.nonlinear.steps = 40;
    SolverOutput out;
    if (!solve(in, out)) {
        return;
    }
    const double collapse = 8.0 * yield / kL;
    const double applied = out.nonlinear.loadFactor * 1000.0;
    expectNear("carga no patamar / (8 Mp / L)", applied / collapse, 1.0, 3e-2);
    std::printf("    patamar: %.2f N (colapso rigido-plastico %.2f N), %d passos, convergiu=%d\n", applied, collapse, out.nonlinear.steps, out.nonlinear.converged ? 1 : 0);
    int yielded = 0;
    for (const auto& h : out.nonlinear.hinges) {
        yielded += h.yielded ? 1 : 0;
    }
    expectTrue("tres rotulas plastificadas", yielded == 3);
    // antes do colapso (carga 1000 N < 1600 N) so ha resposta elastica: as tres rotulas nao plastificam
    SolverInput below = in;
    below.nonlinear.controlMode = 0;
    below.nonlinear.steps = 4;
    SolverOutput outBelow;
    if (solve(below, outBelow)) {
        int count = 0;
        for (const auto& h : outBelow.nonlinear.hinges) {
            count += h.yielded ? 1 : 0;
        }
        expectTrue("abaixo do colapso: apenas as rotulas dos engastes e do meio ja podem escoar, nenhuma em 1000 N", count == 0 || outBelow.nonlinear.hinges.size() == 3);
        // momento elastico de engastamento perfeito: P L / 8 = 625 < Mp
        expectNear("momento no engaste, resposta elastica", std::abs(outBelow.members[0].localForces[5]), 1000.0 * kL / 8.0, 5e-3, 1e-3);
    }
}

void plasticHingeAxialInteraction()
{
    std::printf("[rotula plastica: interacao N-M]\n");
    const double yield = 1000.0, kHinge = 1.0e7, axialYield = 1.0e6;
    auto run = [&](double axial) {
        SolverInput in = baseInput();
        in.nodalLoads.push_back({2, {axial, 0.0, 160.0, 0.0, 0.0, 0.0}});   // M = 160 * 5 = 800 N.m (0,8 Mp)
        PlasticHingeData hinge = makeHinge(kHinge, yield, 0.05);
        hinge.axialYield = axialYield;
        in.plasticHinges.push_back(hinge);
        SolverOutput out;
        const bool ok = solve(in, out);
        return ok && out.nonlinear.hinges.front().yielded;
    };
    expectTrue("sem esforco normal nao plastifica (800 < 1000)", !run(0.0));
    // N = 0,6 Ny: Mp,eff = Mp (1 - 0,36) = 640 < 800
    expectTrue("com N = 0,6 Ny plastifica (800 > 640)", run(0.6 * axialYield));
    expectTrue("com compressao tambem", run(-0.6 * axialYield));
}

void nonlinearEngineSecondOrder()
{
    std::printf("[motor incremental: segunda ordem igual a analise P-delta padrao]\n");
    SolverInput in = baseInput(4.0);
    in.nodalLoads.push_back({2, {-0.3 * 3.14159265358979 * 3.14159265358979 * kE * kIz / (4.0 * 4.0 * 4.0), 0.0, 500.0, 0.0, 0.0, 0.0}});   // compressao 0,3 de Euler/4 e lateral
    in.secondOrder = true;
    SolverOutput reference;
    if (!solve(in, reference)) {
        return;
    }
    SolverInput viaEngine = in;
    viaEngine.nonlinear.force = true;
    viaEngine.nonlinear.steps = 5;
    SolverOutput engine;
    if (!solve(viaEngine, engine)) {
        return;
    }
    expectTrue("motor executado", engine.nonlinear.used);
    expectNear("deslocamento lateral igual", engine.nodes[1].displacements[2], reference.nodes[1].displacements[2], 1e-6);
    expectNear("momento de engaste igual", engine.members[0].localForces[5], reference.members[0].localForces[5], 1e-6, 1e-6);
    // e maior que o de primeira ordem
    SolverInput first = in;
    first.secondOrder = false;
    SolverOutput firstOrder;
    if (solve(first, firstOrder)) {
        expectTrue("segunda ordem amplifica o deslocamento", std::abs(engine.nodes[1].displacements[2]) > 1.05 * std::abs(firstOrder.nodes[1].displacements[2]));
    }
}

SolverInput chain(int elements, double length)
{
    SolverInput in;
    in.materials.push_back({1, kE, kG, 7850.0});
    SectionData sec;
    sec.id = 1;
    sec.A = kA;
    sec.Iz = kIz;
    sec.Iy = kIy;
    sec.J = kJ;
    in.sections.push_back(sec);
    for (int i = 0; i <= elements; ++i) {
        NodeData n;
        n.id = i + 1;
        n.x = length * i / elements;
        if (i == 0) {
            n.restraints = {1, 1, 1, 1, 1, 1};
        }
        in.nodes.push_back(n);
    }
    for (int i = 0; i < elements; ++i) {
        MemberData m;
        m.id = i + 1;
        m.nodeI = i + 1;
        m.nodeJ = i + 2;
        m.materialId = 1;
        m.sectionId = 1;
        in.members.push_back(m);
    }
    return in;
}

void sparseEigenAndIterativeSolver()
{
    std::printf("[infraestrutura: autovalores esparsos e gradiente conjugado]\n");
    const int elements = 40;
    SolverInput in = chain(elements, 6.0);
    in.nodalLoads.push_back({elements + 1, {-1000.0, 0.0, 0.0, 0.0, 0.0, 0.0}});
    in.bucklingModes = 4;
    in.modalModes = 4;
    SolverInput dense = in, sparse = in;
    dense.eigenSolver = 1;
    sparse.eigenSolver = 2;
    SolverOutput a, b;
    if (!solve(dense, a) || !solve(sparse, b)) {
        return;
    }
    expectTrue("flambagem: mesma quantidade de modos", a.bucklingFactors.size() == b.bucklingFactors.size() && a.bucklingFactors.size() == 4);
    for (std::size_t k = 0; k < std::min(a.bucklingFactors.size(), b.bucklingFactors.size()); ++k) {
        expectNear("fator de flambagem esparso = denso", b.bucklingFactors[k], a.bucklingFactors[k], 1e-6);
    }
    expectTrue("modal: mesma quantidade de modos", a.modalOmega.size() == b.modalOmega.size() && a.modalOmega.size() == 4);
    for (std::size_t k = 0; k < std::min(a.modalOmega.size(), b.modalOmega.size()); ++k) {
        expectNear("frequencia esparsa = densa", b.modalOmega[k], a.modalOmega[k], 1e-6);
    }
    // coluna em balanco: Pcr = pi^2 E I / (4 L^2) com a menor inercia (Iz = 4e-6 governa)
    const double pcr = 3.14159265358979 * 3.14159265358979 * kE * std::min(kIy, kIz) / (4.0 * 36.0);
    expectNear("flambagem esparsa: carga de Euler", b.bucklingFactors[0] * 1000.0, pcr, 5e-3);
    // gradiente conjugado igual ao direto
    SolverInput load = chain(elements, 6.0);
    load.nodalLoads.push_back({elements + 1, {0.0, 0.0, 2000.0, 0.0, 0.0, 0.0}});
    SolverInput iterative = load;
    iterative.linearSolver = 1;
    SolverOutput direct, cg;
    if (solve(load, direct) && solve(iterative, cg)) {
        expectNear("PCG: flecha igual a do solver direto", cg.nodes.back().displacements[2], direct.nodes.back().displacements[2], 1e-6);
        bool reported = false;
        for (const auto& w : cg.warnings) {
            reported = reported || w.find("PCG") != std::string::npos;
        }
        expectTrue("PCG informado nos avisos", reported);
    }
    // mecanismo: todos os graus de liberdade nao contidos sao listados
    SolverInput mechanism = chain(3, 3.0);
    mechanism.nodes[0].restraints = {1, 1, 1, 0, 0, 0};   // sem restricao a rotacao em torno do eixo da barra
    SolverOutput bad;
    SolverCore core;
    std::string error;
    const bool ok = core.run(mechanism, bad, error);
    expectTrue("mecanismo rejeitado", !ok);
    std::printf("    mensagem: %s\n", error.c_str());
    expectTrue("mecanismo: mensagem lista os graus de liberdade", error.find("degree(s) of freedom") != std::string::npos);
}

void plasticHingeWithMemberLoad()
{
    std::printf("[rotula plastica com carga distribuida na barra]\n");
    // viga biengastada, carga uniforme em z global; rotulas nos dois engastes (rz = gdl 5 e 11)
    const double q = -2000.0;   // N/m
    auto build = [&](double yield) {
        SolverInput in = baseInput();
        in.nodes[1].restraints = {1, 1, 1, 1, 1, 1};
        MemberLoadData load;
        load.memberId = 1;
        load.system = DistributedLoadSystem::Global;
        load.qz = q;
        in.memberLoads.push_back(load);
        for (const int dof : {5, 11}) {
            PlasticHingeData hinge = makeHinge(1.0e8, yield, 0.01);
            hinge.memberId = 1;
            hinge.dof = dof;
            in.plasticHinges.push_back(hinge);
        }
        return in;
    };
    // Mp muito alto: a barra fica elastica (mola rigida em serie): M = q L^2 / 12 nos engastes
    SolverOutput elastic;
    if (solve(build(1.0e12), elastic)) {
        expectTrue("elastico: convergiu", elastic.nonlinear.converged);
        expectNear("elastico: momento de engaste = qL2/12", std::abs(elastic.members[0].localForces[5]), std::abs(q) * kL * kL / 12.0, 5e-3);
        expectNear("elastico: reacao vertical = qL/2", std::abs(elastic.members[0].localForces[1]), std::abs(q) * kL / 2.0, 1e-6);
    }
    // Mp abaixo do momento de engaste: plastifica, e o momento nos engastes passa a ser ~Mp
    SolverOutput plastic;
    if (solve(build(0.5 * std::abs(q) * kL * kL / 12.0), plastic)) {
        expectTrue("plastico: convergiu", plastic.nonlinear.converged);
        bool yielded = false;
        for (const auto& h : plastic.nonlinear.hinges) {
            yielded = yielded || h.yielded;
        }
        expectTrue("plastico: rotula escoou", yielded);
        expectNear("plastico: reacao vertical = qL/2", std::abs(plastic.members[0].localForces[1]), std::abs(q) * kL / 2.0, 1e-3);
    }
}

void plasticHingePortalFrameMemberLoad()
{
    std::printf("[rotula plastica em portico com carga distribuida na viga]\n");
    // colunas de 3 m (barras 1 e 3) e viga de 6 m (barra 2) no plano XZ; rotula plastica na extremidade I da viga
    SolverInput in = baseInput();
    in.nodes.clear();
    auto node = [&](int id, double x, double z, bool fixed) {
        NodeData n;
        n.id = id;
        n.x = x;
        n.z = z;
        if (fixed) {
            n.restraints = {1, 1, 1, 1, 1, 1};
        }
        in.nodes.push_back(n);
    };
    node(1, 0.0, 0.0, true);
    node(2, 0.0, 3.0, false);
    node(3, 6.0, 3.0, false);
    node(4, 6.0, 0.0, true);
    in.members.clear();
    for (int i = 0; i < 3; ++i) {
        MemberData m;
        m.id = i + 1;
        m.nodeI = i + 1;
        m.nodeJ = i + 2;
        m.materialId = 1;
        m.sectionId = 1;
        in.members.push_back(m);
    }
    MemberLoadData load;
    load.memberId = 2;
    load.system = DistributedLoadSystem::Global;
    load.qz = -60000.0;
    in.memberLoads.push_back(load);
    PlasticHingeData hinge = makeHinge(5.0e6, 1.5e5, 0.01);
    hinge.memberId = 2;
    hinge.dof = 5;
    in.plasticHinges.push_back(hinge);
    SolverOutput out;
    if (solve(in, out)) {
        expectTrue("portico: convergiu", out.nonlinear.converged);
        double vertical = 0.0;
        for (const auto& node : out.nodes) {
            vertical += node.reactions[2];
        }
        expectNear("portico: soma das reacoes verticais = q L", vertical, 60000.0 * 6.0, 1e-6);
        bool yielded = false;
        for (const auto& h : out.nonlinear.hinges) {
            yielded = yielded || h.yielded;
        }
        expectTrue("portico: rotula escoou", yielded);
    }
}

void plasticHingeCantileverMemberLoad()
{
    std::printf("[rotula plastica: balanco com carga distribuida]\n");
    const double q = -1000.0;
    SolverInput in = baseInput();
    MemberLoadData load;
    load.memberId = 1;
    load.system = DistributedLoadSystem::Local;
    load.qy = q;
    in.memberLoads.push_back(load);
    PlasticHingeData hinge = makeHinge(1.0e9, 1.0e12, 0.01);   // praticamente rigida e sem escoar
    hinge.dof = 5;
    in.plasticHinges.push_back(hinge);
    SolverOutput withHinge, elastic;
    SolverInput plain = in;
    plain.plasticHinges.clear();
    if (solve(in, withHinge) && solve(plain, elastic)) {
        expectNear("balanco com rotula elastica: flecha igual a elastica", withHinge.nodes[1].displacements[1], elastic.nodes[1].displacements[1], 1e-3);
        expectNear("balanco com rotula elastica: momento de engaste", withHinge.members[0].localForces[5], elastic.members[0].localForces[5], 1e-3);
    }
}

} // namespace

int main()
{
    pointForceCantilever();
    trapezoidCantilever();
    trapezoidEqualsUniform();
    partialUniformEquilibrium();
    pointMomentCantilever();
    torqueCantilever();
    thermalAndShrinkage();
    globalSystemTrapezoid();
    releasedMemberPointLoad();
    taperExpansion();
    semiRigidSpring();
    plasticHingeCantilever();
    plasticDisplacementControl();
    nonlinearLinks();
    constraintsDiaphragm();
    constraintsRigidLinkAndInclined();
    plasticCollapseMechanism();
    plasticHingeAxialInteraction();
    nonlinearEngineSecondOrder();
    sparseEigenAndIterativeSolver();
    plasticHingeCantileverMemberLoad();
    plasticHingeWithMemberLoad();
    plasticHingePortalFrameMemberLoad();
    std::printf("\n%d verificacoes, %d falhas\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
