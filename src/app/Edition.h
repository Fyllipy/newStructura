#pragma once

#include "app/ProductInfo.h"

/**
 * Ponto unico de consulta das capacidades de cada edicao (Comercial x Demonstrativa).
 *
 * A edicao e escolhida na configuracao do CMake (STRUCTURA_EDITION, ver cmake/ProductInfo.cmake). Quem precisa saber o que a
 * edicao oferece consulta estas constantes; nao se espalha `#ifdef STRUCTURA_EDITION_*` pelo codigo. Os modulos exclusivos da edicao
 * Comercial (dimensionamento, ligacoes, detalhamento) nem sao compilados na Demonstrativa: estas constantes servem apenas para
 * decidir, no codigo compartilhado, o que mostrar e o que o formato de arquivo deve apenas preservar.
 */
namespace Structura::App::Edition {

inline constexpr bool kDemo = Structura::Product::kIsDemo;
inline constexpr bool kCommercial = !kDemo;

/// Dimensionamento de barras e cascas, verificacao de perfis, pilares/lajes mistas, ligacoes e memorias de calculo.
inline constexpr bool kHasDesign = kCommercial;
/// Detalhamento e saidas de fabricacao: pranchas, lista de corte, DSTV e IFC.
inline constexpr bool kHasFabrication = kCommercial;
/// Flambagem elastica, analise modal e analise dinamica.
inline constexpr bool kHasStabilityAndDynamics = kCommercial;
/// Limite de nos do modelo de elementos finitos de cascas; 0 = sem limite.
inline constexpr int kMaxShellNodes = kDemo ? 100 : 0;

} // namespace Structura::App::Edition
