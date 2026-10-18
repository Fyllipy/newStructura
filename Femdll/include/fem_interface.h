#pragma once

#include <cstddef>

#if defined(_WIN32)
#if defined(FEM_DLL_EXPORTS)
#define FEM_API __declspec(dllexport)
#else
#define FEM_API __declspec(dllimport)
#endif
#else
#define FEM_API
#endif

/// MEF de cascas/chapas (quadrilatero Q4: membrana com modos incompativeis e penalidade de rotacao no plano, flexao de Mindlin com MITC4)
/// e barras espaciais de Euler-Bernoulli. Unidades consistentes escolhidas pelo chamador (p.ex. N, m, Pa).
/// Identificadores (ids) de nos, cascas e barras sao os do chamador; as consultas usam a ordem de insercao.
extern "C" {

FEM_API void* fem_create();
FEM_API void fem_destroy(void* handle);
/// Versao do formato de resultados (incrementa quando o layout muda).
FEM_API int fem_version();

// --- modelo -----------------------------------------------------------------------------------------------------------
FEM_API int fem_add_material(void* h, int id, double E, double nu, double density, double alpha);
/// restraint6: 1 = restrito (ux uy uz rx ry rz); pode ser nulo (= livre).
FEM_API int fem_add_node(void* h, int id, double x, double y, double z, const int* restraint6);
/// Nos em ordem anti-horaria vista do lado da normal positiva. Espessura na unidade de comprimento do modelo.
FEM_API int fem_add_shell(void* h, int id, int n1, int n2, int n3, int n4, int materialId, double thickness);
FEM_API int fem_add_beam(void* h, int id, int ni, int nj, int materialId, double A, double Iy, double Iz, double J, double refX, double refY, double refZ);
/// Mola elastica. compressionOnly != 0: k6[0] = rigidez normal, k6[1] = rigidez tangencial; `normal` aponta do apoio para a estrutura.
FEM_API int fem_add_spring(void* h, int node, const double* k6, int compressionOnly, double nx, double ny, double nz);

/// Liga o no `slave` aos nos mestres (count <= 8) com os pesos dados: u_slave = soma(w_i u_mestre_i) nos 6 graus de liberdade.
FEM_API int fem_add_tie(void* h, int slave, int count, const int* masters, const double* weights);

// --- casos de carga -----------------------------------------------------------------------------------------------------
/// Cria um caso de carga e devolve seu indice (>= 0).
FEM_API int fem_add_case(void* h);
FEM_API int fem_load_node(void* h, int caseIndex, int node, const double* f6);
/// Pressao nodal (4 valores), positiva no sentido da normal do elemento.
FEM_API int fem_load_shell_pressure(void* h, int caseIndex, int shell, const double* p4);
/// Forca por unidade de volume global (p.ex. peso proprio = (0, 0, -peso especifico)).
FEM_API int fem_load_shell_body(void* h, int caseIndex, int shell, double bx, double by, double bz);
FEM_API int fem_load_shell_thermal(void* h, int caseIndex, int shell, double uniform, double gradient);
FEM_API int fem_load_beam_uniform(void* h, int caseIndex, int beam, double qx, double qy, double qz);

// --- solucao e resultados -------------------------------------------------------------------------------------------------
/// 0 = sucesso. `error` recebe a mensagem em caso de falha.
FEM_API int fem_solve(void* h, int errorSize, char* error);
FEM_API int fem_spring_iterations(void* h);
FEM_API double fem_max_warp(void* h);
/// 6 valores por no (ordem de insercao).
FEM_API int fem_get_displacements(void* h, int caseIndex, double* out);
FEM_API int fem_get_reactions(void* h, int caseIndex, double* out);
/// 5 pontos (4 cantos + centro) x 8 valores (nxx nyy nxy mxx myy mxy qx qy) = 40 valores por casca, no sistema local do elemento;
/// `axes9` (opcional) recebe e1, e2, e3 (linhas) de cada casca (9 valores).
FEM_API int fem_get_shell_results(void* h, int caseIndex, double* out40, double* axes9);
/// Esforcos nas extremidades da barra (12 valores, eixos locais, forcas sobre o elemento).
FEM_API int fem_get_beam_forces(void* h, int caseIndex, double* out12);

}
