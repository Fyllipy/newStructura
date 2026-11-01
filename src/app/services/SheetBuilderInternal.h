#pragma once

#include "app/services/FabricationModel.h"
#include "app/services/MemberFabrication.h"
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

/// Prancha de fabricacao da ligacao: cada chapa marcada com contorno, furos e cotas encadeadas, mais tabelas de parafusos, soldas e furacao nos perfis.
/// Devolve a escala da ultima chapa desenhada.
double drawFabricationSheet(Structura::Drawing::Sheet &sheet, const SheetBuilder::ConnectionDetail &detail, double x0, double y0, double x1, double y1);

/// Lista de pecas de todas as ligacoes (chapas por espessura e dimensao, parafusos por diametro/classe/comprimento, solda por perna) com massas.
void drawConnectionBomSheet(Structura::Drawing::Sheet &sheet, const std::vector<SheetBuilder::ConnectionDetail> &details, double x0, double y0, double x1, double y1);

/// Desenho de fabricacao de uma peca de perfil marcada (comprimento de corte, furacao, secao e tabelas).
void drawMemberFabricationSheet(Structura::Drawing::Sheet &sheet, const FabMember &member, double x0, double y0, double x1, double y1);

/// Plano de corte de perfis (barras de estoque) e chapas (chapas padrao), com padroes de corte e aproveitamento.
void drawCutListSheet(Structura::Drawing::Sheet &sheet, const SheetBuilder::Input &input, const SheetBuilder::Options &options, double x0, double y0, double x1, double y1);

/// Quadro de conjuntos: pecas de perfil marcadas, com corte, furacao, recortes, chapas soldadas ao perfil e massas.
void drawAssemblyScheduleSheet(Structura::Drawing::Sheet &sheet, const SheetBuilder::Input &input, double x0, double y0, double x1, double y1);

/// Chapa com contorno, furos e cotas encadeadas dentro da celula [cx0, cx0 + cw] x [cyTop - ch, cyTop]; devolve a escala (denominador).
double drawPlatePanel(Structura::Drawing::Sheet &sheet, const FabPart &part, double cx0, double cyTop, double cw, double ch);
/// Tabelas de chapas, parafusos e soldas da ligacao a partir de `yTop` (para baixo) na largura `width`; devolve a altura usada.
double drawFabricationTables(Structura::Drawing::Sheet &sheet, const FabAssembly &fab, double x0, double yTop, double width, double maxHeight);

/// Cota alinhada entre p1 e p2, deslocada `offset` mm do lado esquerdo do sentido p1->p2 (negativo = direito).
void dimension(Structura::Drawing::Sheet &sheet, int layer, Structura::Drawing::Point p1, Structura::Drawing::Point p2, double offset, const std::string &text,
               double textHeight = 2.2);

} // namespace Structura::App::Services::internal
