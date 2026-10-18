#pragma once

#include "app/SolverBridge.h"
#include "shell/ShellJob.h"
#include "shell/ShellMesh.h"
#include "shell/ShellResults.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Structura::App::Services {

/// Acoplamento das cascas e chapas ao modelo de barras: as cascas entram na mesma analise (mesma matriz de rigidez) do solver de barras.
///
///  * nos da malha que coincidem com nos de barras sao os mesmos nos (continuidade total);
///  * nos de barras sobre o plano de um elemento de casca (borda ou face) ficam presos a ele por interpolacao;
///  * nos pendentes de malhas nao conformes ficam presos por interpolacao (Mesh::ties);
///  * apoios, molas e cargas das cascas (ShellModel) entram nos casos de carga pelo nome.
///
/// Unidades: o modelo de barras usa metros e a unidade de forca do projeto (N ou kN, `forceUnit` em N); as cascas ficam em SI.
class ShellCouplingService
{
public:
    /// Geometria acoplada (compartilhada por todos os casos de uma analise).
    struct Mapping {
        Structura::Shell::Mesh mesh;
        std::vector<int> nodeIdOfMeshNode;                       ///< no da malha -> id do no no solver
        std::shared_ptr<const SolverBridge::ShellPart> part;
        std::vector<std::pair<int, std::array<int, 6>>> barRestraints;   ///< apoios das cascas sobre nos de barras (id do no, graus restritos)
        std::vector<int> tiedBarNodes;                           ///< nos de barras presos a elementos de casca (nao coincidem com no da malha)
        int mergedNodes {0};                                     ///< nos da malha que coincidem com nos de barras
        int interiorShellNodes {0};                              ///< nos da malha sobre o eixo de uma barra sem ser extremidade (barra nao dividida)
        std::vector<std::string> warnings;
        double forceUnit {1.0};
    };

    /// Constroi (com cache) o acoplamento da geometria das cascas com os nos e barras de `model` (so geometria: nos, barras).
    [[nodiscard]] static std::shared_ptr<const Mapping> mapping(const Structura::Shell::Model &shell, const SolverBridge::ModelData &model, double forceUnit);

    /// Acrescenta ao modelo de um caso de carga as cascas, ligacoes, apoios e as cargas das cascas daquele caso. Devolve o mapeamento usado
    /// (nulo se nao ha cascas visiveis).
    static std::shared_ptr<const Mapping> attach(SolverBridge::ModelData &model, const Structura::Shell::Model &shell, const std::string &loadCase, double forceUnit);

    /// Resultados das cascas de um caso (ou combinacao) no formato do modulo de cascas, em SI.
    [[nodiscard]] static Structura::Shell::CaseResult caseResult(const Mapping &mapping, const SolverBridge::Result &result, const std::string &name);

    /// Posicoes de corte (parametro t em (0,1)) de cada barra de `model` nos nos de casca que caem sobre o seu eixo.
    struct BarCuts {
        int memberId {0};
        std::vector<double> parameters;                 ///< fracao do comprimento (0..1), crescente
        std::vector<std::array<double, 3>> points;      ///< posicao do no da malha correspondente
    };
    [[nodiscard]] static std::vector<BarCuts> barCuts(const Structura::Shell::Mesh &mesh, const SolverBridge::ModelData &model);
};

} // namespace Structura::App::Services
