#pragma once

#include "drawing/DrawingModel.h"

#include <string>
#include <vector>

namespace Structura::Drawing {

/// Retangulo envolvente de uma entidade (texto: largura estimada pelo numero de caracteres).
[[nodiscard]] Rect entityBounds(const Entity &entity);

/// Retangulo envolvente de todas as entidades de `sheet` (invalido, x0 > x1, se vazia).
[[nodiscard]] Rect sheetBounds(const Sheet &sheet);

/// Retangulo envolvente de um grupo; `valid` = false se o grupo nao tem entidades.
[[nodiscard]] Rect groupBounds(const Sheet &sheet, int group, bool *valid = nullptr);

/// Grupo (id > 0) cuja entidade esta mais perto de `p` e a menos de `tol` mm; 0 se nenhum.
[[nodiscard]] int hitGroup(const Sheet &sheet, Point p, double tol);

/// Id do grupo com a chave `key`, ou 0.
[[nodiscard]] int findGroup(const Sheet &sheet, const std::string &key);

void translateGroup(Sheet &sheet, int group, double dx, double dy);

/// Remove do desenho todas as entidades do grupo (o grupo continua listado, vazio).
void eraseGroup(Sheet &sheet, int group);

/// Registra `bounds` como area ocupada.
inline void occupy(Sheet &sheet, const Rect &bounds) { sheet.occupied.push_back(bounds); }

/// Soma das areas de sobreposicao de `bounds` (com `margin`) contra as areas ocupadas da prancha.
[[nodiscard]] double overlapWithOccupied(const Sheet &sheet, const Rect &bounds, double margin = 0.6);

/// Move para `target` todas as entidades de `scratch` (marcando-as com o grupo `key`) e registra a area ocupada.
void commitGroup(Sheet &target, const Sheet &scratch, const std::string &key);

} // namespace Structura::Drawing
