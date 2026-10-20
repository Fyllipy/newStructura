#pragma once

#include <QString>

#include <array>
#include <memory>
#include <stdexcept>
#include <vector>

namespace Structura::App {

class SolverBridgeError : public std::runtime_error
{
public:
    explicit SolverBridgeError(const std::string &message)
        : std::runtime_error(message)
    {
    }
};

class SolverBridge
{
public:
    enum class MemberLoadCoordSystem {
        Local = 0,
        Global = 1
    };

    struct MaterialData {
        int id {0};
        double e {0.0};
        double g {0.0};
        /// Massa especifica (unidade de forca*s2/comprimento4) usada so na analise modal.
        double density {0.0};
    };

    struct SectionData {
        int id {0};
        double area {0.0};
        double iz {0.0};
        double iy {0.0};
        double j {0.0};
    };

    struct NodeData {
        int id {0};
        double x {0.0};
        double y {0.0};
        double z {0.0};
        std::array<int, 6> restraints {{0, 0, 0, 0, 0, 0}};
    };

    struct MemberData {
        int id {0};
        int nodeI {0};
        int nodeJ {0};
        int materialId {0};
        int sectionId {0};
        // Posicionamento da secao: giro em torno de x local (graus) e posicao do eixo da barra
        // (centroide) em relacao a cada no, nos eixos locais girados (m).
        double sectionRotationDeg {0.0};
        double offsetY1 {0.0};
        double offsetZ1 {0.0};
        double offsetY2 {0.0};
        double offsetZ2 {0.0};
        /// Liberacoes: ux uy uz rx ry rz no I e depois no J (nao zero = liberado).
        std::array<int, 12> releases {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
        /// Tirante: so trabalha a tracao (a barra e desativada enquanto encurtar).
        bool tensionOnly {false};

        [[nodiscard]] bool hasPlacement() const
        {
            return sectionRotationDeg != 0.0 || offsetY1 != 0.0 || offsetZ1 != 0.0
                || offsetY2 != 0.0 || offsetZ2 != 0.0;
        }
        [[nodiscard]] bool hasReleases() const
        {
            for (const int flag : releases) {
                if (flag != 0) {
                    return true;
                }
            }
            return false;
        }
    };

    struct NodalLoadData {
        int nodeId {0};
        std::array<double, 6> values {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    };

    struct MemberLoadData {
        int memberId {0};
        MemberLoadCoordSystem coordSystem {MemberLoadCoordSystem::Local};
        double qx {0.0};
        double qy {0.0};
        double qz {0.0};
    };

    struct MemberMomentData {
        int memberId {0};
        MemberLoadCoordSystem coordSystem {MemberLoadCoordSystem::Local};
        double mx {0.0};
        double my {0.0};
        double mz {0.0};
    };

    /// Cascas e chapas acopladas as barras (geometria compartilhada pelos casos; unidades do modelo de barras).
    struct ShellNodeData {
        int id {0};
        double x {0.0}, y {0.0}, z {0.0};
        std::array<int, 6> restraints {{0, 0, 0, 0, 0, 0}};
    };
    struct ShellElementData {
        int id {0};
        std::array<int, 4> nodes {{0, 0, 0, 0}};   ///< ids dos nos, anti-horario visto do lado da normal positiva
        double e {0.0}, nu {0.3}, density {0.0}, thickness {0.0};
    };
    struct ShellTieData {
        int slave {0};
        std::vector<int> masters;
        std::vector<double> weights;
        int dofMask {63};   ///< graus de liberdade ligados (bits ux uy uz rx ry rz); 7 = so translacoes (apoio articulado)
    };
    struct ShellSpringData {
        int node {0};
        std::array<double, 6> k {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
        bool compressionOnly {false};
        std::array<double, 3> normal {{0.0, 0.0, 1.0}};
    };
    struct ShellPart {
        std::vector<ShellNodeData> nodes;        ///< nos que nao pertencem ao modelo de barras
        std::vector<ShellElementData> elements;
        std::vector<ShellTieData> ties;
        std::vector<ShellSpringData> springs;
        [[nodiscard]] bool empty() const { return elements.empty(); }
    };
    /// Carga de elemento de casca: kind 0 = pressao nodal (4 valores, positiva no sentido da normal), 1 = forca de corpo global (3),
    /// 2 = temperatura (uniforme, gradiente).
    struct ShellLoadData {
        int shellId {0};
        int kind {0};
        std::array<double, 4> v {{0.0, 0.0, 0.0, 0.0}};
    };

    struct ModelData {
        std::vector<MaterialData> materials;
        std::vector<SectionData> sections;
        std::vector<NodeData> nodes;
        std::vector<MemberData> members;
        std::vector<NodalLoadData> nodalLoads;
        std::vector<MemberLoadData> memberLoads;
        std::vector<MemberMomentData> memberMoments;
        /// Cascas acopladas (nulo = so barras). A geometria e compartilhada entre os casos; as cargas sao por caso.
        std::shared_ptr<const ShellPart> shell;
        std::vector<ShellLoadData> shellLoads;
        /// Analise de segunda ordem (P-delta e P-delta local) por iteracao com rigidez geometrica.
        bool secondOrder {false};
        /// Modos de flambagem elastica linear a extrair (0 = nao calcular); usa os esforcos axiais de primeira ordem.
        int bucklingModes {0};
        /// Modos de vibracao livre a extrair (0 = nao calcular). Massas adicionais concentradas por no (id, massa).
        int modalModes {0};
        std::vector<std::pair<int, double>> extraMasses;
    };

    struct NodeResult {
        int nodeId {0};
        std::array<double, 6> displacements {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
        std::array<double, 6> reactions {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    };

    struct MemberResult {
        int memberId {0};
        std::array<double, 12> localForces {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
        std::array<double, 3> auxK {{0.0, 0.0, 0.0}};
    };

    struct ShellElementResult {
        int shellId {0};
        std::array<double, 40> values {};   ///< 5 pontos (4 cantos e centro) x (nxx nyy nxy mxx myy mxy qx qy), eixos locais do elemento
        std::array<double, 9> axes {};      ///< e1, e2, e3
    };

    struct Result {
        std::vector<NodeResult> nodeResults;
        std::vector<MemberResult> memberResults;
        /// Resultados das cascas acopladas: nos proprios (fora de `nodeResults`) e elementos.
        std::vector<NodeResult> shellNodeResults;
        std::vector<ShellElementResult> shellResults;
        /// Fatores criticos de carga (crescentes) e formas modais (6 valores por no, na ordem de ModelData::nodes).
        std::vector<double> bucklingFactors;
        std::vector<std::vector<double>> bucklingModes;
        /// Modal: frequencias circulares (rad/s), fracao de massa efetiva (X,Y,Z) e formas modais (6 por no).
        std::vector<double> modalOmega;
        std::vector<std::array<double, 3>> modalMassRatio;
        std::vector<std::vector<double>> modalShapes;
    };

    explicit SolverBridge(QString libraryPath = QString());

    [[nodiscard]] Result solve(const ModelData &model) const;

private:
    QString m_libraryPath;
};

} // namespace Structura::App
