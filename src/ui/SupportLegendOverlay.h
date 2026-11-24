#pragma once

#include <QString>
#include <QWidget>

#include <vector>

namespace Structura::UI {

/// Legenda dos tipos de apoio de cascas desenhados na vista (cor + nome + quantidade de nos). Filha do widget da vista, transparente ao mouse.
class SupportLegendOverlay : public QWidget
{
    Q_OBJECT

public:
    struct Entry {
        int kind {0};      ///< Structura::Shell::SupportKind
        int count {0};     ///< nos com esse tipo
    };

    explicit SupportLegendOverlay(QWidget *parent);

    void setEntries(const std::vector<Entry> &entries);
    void clearEntries();
    [[nodiscard]] const std::vector<Entry> &entries() const noexcept { return m_entries; }
    void placeInParent();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::vector<Entry> m_entries;
};

} // namespace Structura::UI
