#pragma once

#include <QColor>
#include <QString>

namespace Structura::UI::Theme {

enum class Mode { Light, Dark };

/// Le a opcao gravada (Arquivo > Opcoes > Opcoes visuais), instala o filtro que converte as folhas de estilo dos widgets e aplica o tema.
/// Chamar logo depois de criar o QApplication, antes das janelas.
void initialize();

/// Troca o tema em uso (janelas ja abertas incluidas) e grava a escolha.
void apply(Mode mode, bool persist = true);

[[nodiscard]] Mode current();
[[nodiscard]] bool isDark();

/// Cor adaptada ao tema: no modo claro devolve a mesma cor; no escuro inverte a luminosidade (cinzas e fundos claros viram escuros,
/// textos escuros viram claros) e mantem o matiz; cores saturadas (destaques, estados) ficam vivas.
[[nodiscard]] QColor color(const QColor &light);
[[nodiscard]] QColor color(const QString &lightHex);

/// Converte as cores de uma folha de estilo (#rgb, #rrggbb, rgb(), rgba()) para o modo escuro.
[[nodiscard]] QString darkStyleSheet(const QString &lightStyleSheet);

} // namespace Structura::UI::Theme
