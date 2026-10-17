#pragma once

#include "drawing/DrawingModel.h"

#include <QPointF>
#include <QString>

class QPainter;

namespace Structura::Drawing {

/// Desenha uma prancha com QPainter (pre-visualizacao e PDF). `pxPerMm` converte milimetros do papel em unidades do painter;
/// `origin` e o canto superior esquerdo da folha. Linhas sao desenhadas com espessura minima de `minPenPx`.
void paintSheet(QPainter &painter, const Sheet &sheet, double pxPerMm, QPointF origin = {}, double minPenPx = 1.0);

/// DXF ASCII (R12) da prancha em milimetros do papel, uma camada por camada da prancha.
QString sheetToDxf(const Sheet &sheet);

} // namespace Structura::Drawing
