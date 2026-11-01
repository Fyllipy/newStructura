#pragma once

#include "drawing/DrawingModel.h"

#include <string>

namespace Structura::Drawing {

/// Simbolos normalizados de detalhamento (milimetros do papel). Cada funcao desenha na camada indicada.
struct WeldSymbol {
    enum class Type { Fillet, GrooveFull, GroovePartial };

    Point arrowTip;            ///< ponto da junta apontado pela seta
    Point tail;                ///< fim da linha de referencia (cauda) onde o simbolo e escrito
    Type type {Type::Fillet};
    double size {6.0};         ///< perna do filete ou garganta efetiva (mm); 0 = nao escreve
    double length {0.0};       ///< comprimento do cordao (mm); 0 = nao escreve (cordao continuo)
    bool arrowSide {true};     ///< simbolo no lado da seta (abaixo da linha de referencia); false = outro lado (acima)
    bool bothSides {false};    ///< repete o simbolo nos dois lados
    bool field {false};        ///< solda de campo (bandeira)
    bool allAround {false};    ///< solda em todo o contorno (circulo)
    std::string note;          ///< texto na cauda (ex.: processo, especificacao)
};

/// Seta de ponta cheia de `from` para `to`.
void arrowHead(Sheet &sheet, Point from, Point to, int layer, double size = 2.5);

/// Solda: seta, linha de referencia horizontal de `tail` (para o lado da seta), simbolo (filete: triangulo; chanfro: V/bisel), dimensao, bandeira e circulo.
void drawWeldSymbol(Sheet &sheet, const WeldSymbol &weld, int layer, int textLayer);

/// Parafuso/furo visto de frente: circulo do furo, cruz de eixo e, opcionalmente, chamada "n PARAF. Ø.." na ponta de uma linha de chamada.
void drawBoltHole(Sheet &sheet, Point center, double holeDiameterPaper, int layer, int centreLayer);

/// Parafuso visto pelo eixo (planta/elevacao): hexagono da cabeca (entre faces 1,5 d), circulo da haste e cruz de eixo; `d` e o diametro no papel.
void drawBoltEndSymbol(Sheet &sheet, Point center, double d, int layer, int centreLayer);

/// Parafuso cortado ou visto de lado: eixo tracejado com 2 mm de ponta sobre `a`-`b` (extremidades do parafuso no papel).
void drawBoltAxisLine(Sheet &sheet, Point a, Point b, int centreLayer);

/// Chamada de marca: circulo com o texto, ligado por uma linha ao ponto apontado.
void drawMarkCallout(Sheet &sheet, Point target, Point label, const std::string &mark, int layer, int textLayer, double radius = 4.0);

/// Linha de corte com setas e letras (ex.: "A"), de `a` para `b`; as setas apontam para o lado `side` (+1 esquerda do sentido a->b, -1 direita).
void drawSectionCut(Sheet &sheet, Point a, Point b, const std::string &letter, double side, int layer, int textLayer);

} // namespace Structura::Drawing
