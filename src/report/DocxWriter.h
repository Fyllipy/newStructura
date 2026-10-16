#pragma once

#include "report/ReportModel.h"

#include <string>
#include <vector>

namespace Structura::Report {

/// Gera um arquivo .docx (Office Open XML) a partir do modelo do relatorio. As equacoes viram equacoes
/// nativas do Word (OMML), editaveis, e os estilos (capa, titulos, tabelas, notas) ficam em styles.xml.
std::vector<unsigned char> writeDocx(const ReportDocument &document);

/// Equacao LaTeX -> OMML (<m:oMath>...</m:oMath>), usado tambem nos testes.
std::string latexToOmml(const std::string &latex);

} // namespace Structura::Report
