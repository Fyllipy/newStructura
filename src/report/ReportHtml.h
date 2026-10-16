#pragma once

#include "report/ReportModel.h"

#include <QImage>
#include <QMap>
#include <QString>

namespace Structura::Report {

struct HtmlResult {
    QString html;
    QMap<QString, QImage> images;   ///< recursos referenciados pelo HTML (equacoes e figuras)
};

/// Converte o relatorio em HTML para o QTextBrowser. As equacoes viram imagens renderizadas por `renderMath`.
/// `includeCover`: false omite capa e sumario (visualizacao de uma barra).
HtmlResult toHtml(const ReportDocument &document, double equationPixelSize = 19.0, bool includeCover = true);

} // namespace Structura::Report
