#pragma once

#include <QDialog>
#include <QString>
#include <QUuid>
#include <QVector>

class QComboBox;

namespace Structura::UI {

/// Criterios que limitam a selecao (por janela, "selecionar tudo que atende" etc.).
struct SelectionFilter {
    bool levelEnabled {false};
    double level {0.0};
    QUuid section;  ///< nulo = qualquer secao
    QUuid material; ///< nulo = qualquer material

    [[nodiscard]] bool active() const { return levelEnabled || !section.isNull() || !material.isNull(); }
    [[nodiscard]] bool barsOnly() const { return !section.isNull() || !material.isNull(); }
};

class SelectionFilterDialog : public QDialog
{
    Q_OBJECT

public:
    struct Option {
        QString text;
        QUuid id;
        double value {0.0};
    };

    SelectionFilterDialog(const SelectionFilter &current,
                          const QVector<Option> &levels,
                          const QVector<Option> &sections,
                          const QVector<Option> &materials,
                          QWidget *parent = nullptr);

    [[nodiscard]] SelectionFilter filter() const;

private:
    QComboBox *m_level;
    QComboBox *m_section;
    QComboBox *m_material;
};

} // namespace Structura::UI
