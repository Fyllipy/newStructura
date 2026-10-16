#pragma once

#include "viz/structure/SectionShapes.h"

#include <QPixmap>
#include <QWidget>

#include <optional>

namespace Structura::UI {

/// Desenha o contorno de uma secao (com o centroide marcado), em escala ajustada ao widget.
class SectionPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SectionPreviewWidget(QWidget *parent = nullptr);

    /// Gera o contorno; devolve false (e mostra `error`) se as dimensoes forem invalidas.
    bool setSection(const QString &family,
                    const Structura::Visualization::SectionShapes::Parameters &parameters,
                    bool mirrored,
                    QString *error = nullptr);
    void clear();
    void setPlaceholder(const QString &text);

    /// Miniatura do contorno (para listas e botoes).
    static QPixmap thumbnail(const QString &family,
                             const Structura::Visualization::SectionShapes::Parameters &parameters,
                             const QSize &size,
                             bool mirrored = false);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::optional<Structura::Visualization::SectionShape> m_shape;
    QString m_placeholder;
};

} // namespace Structura::UI
