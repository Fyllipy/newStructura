#pragma once

#include "report/MathParser.h"

#include <QColor>
#include <QImage>

namespace Structura::Report {

/// Desenha uma equacao (arvore LaTeX) em uma imagem com fundo transparente: fracoes, expoentes/indices,
/// raizes e parenteses que acompanham a altura do conteudo.
/// `pixelSize` e o tamanho da fonte em pixels logicos; `devicePixelRatio` multiplica a resolucao.
QImage renderMath(const MathPtr &tree, double pixelSize = 18.0, const QColor &color = QColor(30, 35, 43), qreal devicePixelRatio = 2.0);

/// Atalho: `renderMath(parseLatex(latex), ...)`.
QImage renderLatex(const QString &latex, double pixelSize = 18.0, const QColor &color = QColor(30, 35, 43), qreal devicePixelRatio = 2.0);

} // namespace Structura::Report
