#pragma once

#include "app/services/SheetBuilder.h"
#include "design/ShearConnections.h"

#include <vector>

namespace Structura::App::Services::internal {

/// Pecas (chapas, parafusos, solda) de uma ligacao para a lista de materiais.
std::vector<Structura::Design::Connections::Part> connectionParts(const SheetBuilder::ConnectionDetail &detail);

/// Desenha o detalhe da ligacao (vista e corte, com cotas e notas) na regiao [x0,x1]x[y0,y1] (mm do papel).
/// Devolve a escala (denominador) usada. `scaleDenominator` > 0 forca a escala.
double drawConnectionDetail(Structura::Drawing::Sheet &sheet, const SheetBuilder::ConnectionDetail &detail, double x0, double y0, double x1, double y1,
                            bool notes, double scaleDenominator, bool profileOutline = true);

/// Cota alinhada entre p1 e p2, deslocada `offset` mm do lado esquerdo do sentido p1->p2 (negativo = direito).
void dimension(Structura::Drawing::Sheet &sheet, int layer, Structura::Drawing::Point p1, Structura::Drawing::Point p2, double offset, const std::string &text,
               double textHeight = 2.2);

} // namespace Structura::App::Services::internal
