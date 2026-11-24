#include <QtTest/QtTest>

#include "shell/ShellMesh.h"
#include "shell/ShellMeshQuality.h"
#include "shell/ShellModel.h"

#include <cmath>

using namespace Structura::Shell;

namespace {

constexpr double kPi = 3.14159265358979323846;

Mesh singleQuad(const std::array<Vec3, 4> &p, double thickness = 0.01)
{
    Mesh mesh;
    mesh.nodes = {p[0], p[1], p[2], p[3]};
    MeshQuad q;
    q.n = {{0, 1, 2, 3}};
    q.thickness = thickness;
    mesh.quads.push_back(q);
    return mesh;
}

QualityLevel levelOf(const std::array<Vec3, 4> &p, double thickness = 0.01)
{
    return analyzeMeshQuality(singleQuad(p, thickness), Model {}).quads.front().level;
}

} // namespace

class TestShellMeshQuality : public QObject
{
    Q_OBJECT
private slots:
    void regularQuadIsAcceptable()
    {
        const Mesh mesh = singleQuad({{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}});
        const MeshQualityReport r = analyzeMeshQuality(mesh, Model {});
        QCOMPARE(r.acceptable, 1);
        const QuadQuality &q = r.quads.front();
        QVERIFY(std::abs(q.minAngle - 90.0) < 1e-9 && std::abs(q.maxAngle - 90.0) < 1e-9);
        QVERIFY(std::abs(q.aspect - 1.0) < 1e-12);
        QVERIFY(std::abs(q.minJacobian - 1.0) < 1e-12);
        QVERIFY(q.warp < 1e-15);
        QVERIFY(!r.hasErrors());
    }

    void trapezoidAndSkewByAngleTiers()
    {
        // trapezio suave (angulos 81..99): aceitavel
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {1, 0, 0}, {0.85, 1, 0}, {0.15, 1, 0}}})), static_cast<int>(QualityLevel::Acceptable));
        // losango de 45 graus (angulos 45 e 135): ainda aceitavel (limite inclusivo)
        const double s45 = std::tan(45.0 * kPi / 180.0);
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {1, 0, 0}, {1 + s45, 1, 0}, {s45, 1, 0}}})), static_cast<int>(QualityLevel::Acceptable));
        // 30 graus de skew (angulos 60..120 => aceitavel) e 60 graus de skew (30..150 => aviso)
        const double s60 = std::tan(60.0 * kPi / 180.0);
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {1, 0, 0}, {1 + s60, 1, 0}, {s60, 1, 0}}})), static_cast<int>(QualityLevel::Warning));
        // 82 graus de skew (angulos ~8..172, como os elementos do polo): aviso, nao erro (medido: ~4-6 % de erro local, Etapa 3)
        const double s82 = std::tan(82.0 * kPi / 180.0);
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {1, 0, 0}, {1 + s82, 1, 0}, {s82, 1, 0}}})), static_cast<int>(QualityLevel::Warning));
        // 89,7 graus de skew (angulos ~0,3..179,7): praticamente degenerado => erro
        const double s897 = std::tan(89.7 * kPi / 180.0);
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {1, 0, 0}, {1 + s897, 1, 0}, {s897, 1, 0}}})), static_cast<int>(QualityLevel::Error));
    }

    void aspectRatioTiers()
    {
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {20, 0, 0}, {20, 1, 0}, {0, 1, 0}}})), static_cast<int>(QualityLevel::Acceptable));   // 20
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {40, 0, 0}, {40, 1, 0}, {0, 1, 0}}})), static_cast<int>(QualityLevel::Warning));      // 40
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {2000, 0, 0}, {2000, 1, 0}, {0, 1, 0}}})), static_cast<int>(QualityLevel::Error));    // 2000
    }

    void warpTiersUseThicknessAndSize()
    {
        auto warped = [](double delta) {
            return std::array<Vec3, 4> {{{0, 0, delta}, {1, 0, -delta}, {1, 1, delta}, {0, 1, -delta}}};
        };
        QCOMPARE(static_cast<int>(levelOf(warped(1.0e-4))), static_cast<int>(QualityLevel::Acceptable));   // delta/t = 0,01
        QCOMPARE(static_cast<int>(levelOf(warped(1.0e-3))), static_cast<int>(QualityLevel::Warning));      // delta/t = 0,1 (~3 % de erro na flecha, Etapa 3)
        QCOMPARE(static_cast<int>(levelOf(warped(3.0e-3))), static_cast<int>(QualityLevel::Error));        // delta/t = 0,3
        // casca grossa (t = 0,5 m): delta/t pequeno, mas delta/h = 0,03 ainda e reprovado por tamanho
        QCOMPARE(static_cast<int>(levelOf(warped(0.04), 0.5)), static_cast<int>(QualityLevel::Error));
        const QuadQuality q = analyzeMeshQuality(singleQuad(warped(1.0e-3)), Model {}).quads.front();
        QVERIFY(std::abs(q.warp - 1.0e-3) < 1e-9);
        QVERIFY(!q.reason.empty());
    }

    void invertedReentrantAndDegenerateElementsAreErrors()
    {
        // gravata (nos 2 e 3 trocados): jacobiano negativo
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}}})), static_cast<int>(QualityLevel::Error));
        // nao convexo (canto reentrante)
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {1, 0, 0}, {0.2, 0.2, 0}, {0, 1, 0}}})), static_cast<int>(QualityLevel::Error));
        // area nula (4 nos colineares) e nos coincidentes
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}}})), static_cast<int>(QualityLevel::Error));
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}})), static_cast<int>(QualityLevel::Error));
        // quase colapsado em triangulo (dois nos a 1e-6 do outro): angulo minimo ~0
        QCOMPARE(static_cast<int>(levelOf({{{0, 0, 0}, {1, 0, 0}, {1e-6, 1e-6, 0}, {0, 1e-6, 0}}})), static_cast<int>(QualityLevel::Error));
        // um quad invertido num conjunto e contado
        MeshQualityReport r = analyzeMeshQuality(singleQuad({{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}}}), Model {});
        QVERIFY(r.hasErrors());
        QVERIFY(!r.messages.empty());
    }

    void neighbourNormalsOrientationAndFolding()
    {
        auto twoQuads = [](bool flipSecond, double foldDegrees) {
            Mesh mesh;
            const double c = std::cos(foldDegrees * kPi / 180.0), s = std::sin(foldDegrees * kPi / 180.0);
            mesh.nodes = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {1 + c, 0, s}, {1 + c, 1, s}};
            MeshQuad a, b;
            a.n = {{0, 1, 2, 3}};
            b.n = flipSecond ? std::array<int, 4> {{1, 2, 5, 4}} : std::array<int, 4> {{2, 1, 4, 5}};
            a.thickness = b.thickness = 0.01;
            mesh.quads = {a, b};
            return mesh;
        };
        const MeshQualityReport coplanar = analyzeMeshQuality(twoQuads(false, 0.0), Model {});
        QVERIFY(!coplanar.hasErrors());
        QCOMPARE(coplanar.sharpDihedralEdges, 0);
        QVERIFY(coplanar.maxDihedral < 1e-6);
        const MeshQualityReport folded = analyzeMeshQuality(twoQuads(false, 60.0), Model {});
        QVERIFY(!folded.hasErrors());
        QCOMPARE(folded.sharpDihedralEdges, 1);
        QVERIFY(std::abs(folded.maxDihedral - 60.0) < 1e-6);
        const MeshQualityReport flipped = analyzeMeshQuality(twoQuads(true, 0.0), Model {});
        QVERIFY(flipped.hasErrors());
        QCOMPARE(flipped.flippedNormalEdges, 1);
        // objetos diferentes compartilhando a aresta nao se submetem a regra de orientacao (juncao de chapas)
        Mesh joint = twoQuads(true, 0.0);
        joint.quads[1].object = 1;
        QVERIFY(!analyzeMeshQuality(joint, Model {}).hasErrors());
    }

    void generatedMeshesHaveNoErrors()
    {
        const double pi = kPi;
        // chapa regular
        {
            Model m;
            Object o;
            o.id = 1;
            o.name = "Chapa";
            o.kind = Kind::Plate;
            o.polygon = {{0, 0, 0}, {4, 0, 0}, {4, 2, 0}, {0, 2, 0}};
            o.p = {{"t", 0.02}};
            o.meshSize = 0.5;
            m.objects = {o};
            const Mesh mesh = generateMesh(m);
            const MeshQualityReport r = analyzeMeshQuality(mesh, m);
            QVERIFY(!r.hasErrors());
            QCOMPARE(r.warnings, 0);
        }
        // poligono geral (triangulo -> 3 quads por triangulo): sem erro
        {
            Model m;
            Object o;
            o.id = 1;
            o.name = "Triangulo";
            o.kind = Kind::Plate;
            o.polygon = {{0, 0, 0}, {3, 0, 0}, {0, 2, 0}};
            o.p = {{"t", 0.02}};
            o.meshSize = 0.4;
            m.objects = {o};
            const Mesh mesh = generateMesh(m);
            const MeshQualityReport r = analyzeMeshQuality(mesh, m);
            QVERIFY(!r.hasErrors());
        }
        // extrusao de circulo (cilindro) e de polilinha
        {
            Model m;
            Object o;
            o.id = 1;
            o.name = "Cilindro";
            o.kind = Kind::Extrusion;
            o.meshSize = 0.3;
            o.polygon = {{0.0, 0.0, 0.0}};
            o.p = {{"circle", 1.0}, {"r", 1.5}, {"vx", 0.0}, {"vy", 0.0}, {"vz", 4.0}, {"endScale", 1.0}, {"t", 0.01}};
            m.objects = {o};
            const MeshQualityReport r = analyzeMeshQuality(generateMesh(m), m);
            QVERIFY(!r.hasErrors());
            QVERIFY(r.maxAspect < 5.0);
        }
        // revolucao: hemisferio (polos: elementos afunilados, nunca degenerados)
        {
            Model m;
            Object o;
            o.id = 1;
            o.name = "Domo";
            o.kind = Kind::Revolution;
            o.meshSize = 0.25;
            for (int i = 0; i <= 20; ++i) {
                const double a = 0.5 * pi * i / 20.0;
                o.polygon.push_back({2.0 * std::cos(a), 0.0, 2.0 * std::sin(a)});
            }
            o.p = {{"ax", 0.0}, {"ay", 0.0}, {"az", 0.0}, {"dx", 0.0}, {"dy", 0.0}, {"dz", 1.0}, {"angle", 360.0}, {"t", 0.01}};
            m.objects = {o};
            const MeshQualityReport r = analyzeMeshQuality(generateMesh(m), m);
            QVERIFY2(!r.hasErrors(), r.messages.empty() ? "" : r.messages.front().c_str());
            QVERIFY(r.warnings > 0);   // elementos triangulares do polo sao sinalizados (aviso), nao escondidos
            QVERIFY(r.maxAngle > 170.0);
        }
        // tanque cilindrico completo (varias virolas, fundo e domo): superficies curvas e juncoes
        {
            Model m;
            Object o;
            o.id = 1;
            o.name = "Tanque";
            o.kind = Kind::CylTank;
            o.meshSize = 0.5;
            o.p = {{"r", 3.0}, {"h", 4.0}, {"courses", 2.0}, {"tBottom", 0.01}, {"roof", 3.0}, {"roofRise", 1.0}, {"tRoof", 0.006}, {"t1", 0.008}, {"t2", 0.006}};
            m.objects = {o};
            const MeshQualityReport r = analyzeMeshQuality(generateMesh(m), m);
            QVERIFY2(!r.hasErrors(), r.messages.empty() ? "" : r.messages.front().c_str());
        }
    }
};

QTEST_GUILESS_MAIN(TestShellMeshQuality)
#include "TestShellMeshQuality.moc"
