#include "ProfilePropertiesDialog.h"

#include "app/services/DesignService.h"
#include "design/DesignSection.h"
#include "design/GrossSection.h"
#include "viz/structure/SectionShapes.h"

#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

namespace {

QString num(double v, int decimals = 2)
{
    return QLocale().toString(v, 'f', decimals);
}

QString sci(double v)
{
    if (v == 0.0) {
        return QStringLiteral("0");
    }
    return QLocale().toString(v, 'g', 6);
}

} // namespace

ProfilePropertiesDialog::ProfilePropertiesDialog(const SectionInfo &section, QWidget *parent)
    : QDialog(parent)
    , m_table(new QTableWidget(this))
{
    setWindowTitle(tr("Propriedades do perfil - %1").arg(section.name));
    resize(760, 720);

    auto *title = new QLabel(tr("<b style='font-size:15px;color:#0d7fb9;'>%1</b>").arg(section.name.toHtmlEscaped()), this);
    auto *hint = new QLabel(tr("Eixos: x é o eixo horizontal da seção (flexão no plano da altura) e y o vertical; valores em cm, salvo indicação. "
                               "As propriedades da seção bruta são obtidas por integração do contorno desenhado."),
                            this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#5a6574;"));

    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Propriedade"), tr("Valor"), tr("Unidade"), tr("Observação")});
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);

    auto *copy = new QPushButton(tr("Copiar tabela"), this);
    auto *close = new QPushButton(tr("Fechar"), this);
    close->setDefault(true);
    connect(copy, &QPushButton::clicked, this, &ProfilePropertiesDialog::copyToClipboard);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch(1);
    buttons->addWidget(copy);
    buttons->addWidget(close);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addWidget(m_table, 1);
    layout->addLayout(buttons);

    fill(section);
}

void ProfilePropertiesDialog::addGroup(const QString &title)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *item = new QTableWidgetItem(title);
    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);
    item->setBackground(QColor(QStringLiteral("#dbe9f3")));
    item->setForeground(QColor(QStringLiteral("#0b4f73")));
    m_table->setItem(row, 0, item);
    for (int c = 1; c < 4; ++c) {
        auto *filler = new QTableWidgetItem;
        filler->setBackground(QColor(QStringLiteral("#dbe9f3")));
        m_table->setItem(row, c, filler);
    }
    m_table->setSpan(row, 0, 1, 4);
}

void ProfilePropertiesDialog::addRow(const QString &name, const QString &value, const QString &unit, const QString &note)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(name));
    auto *valueItem = new QTableWidgetItem(value);
    valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_table->setItem(row, 1, valueItem);
    m_table->setItem(row, 2, new QTableWidgetItem(unit));
    m_table->setItem(row, 3, new QTableWidgetItem(note));
}

void ProfilePropertiesDialog::fill(const SectionInfo &section)
{
    using namespace Structura::Design;
    using Structura::Visualization::SectionShapes;

    // ---- identificacao e dimensoes
    addGroup(tr("Identificação"));
    addRow(tr("Nome"), section.name, QString());
    addRow(tr("Família"), section.family.isEmpty() ? tr("(sem geometria)") : section.family, QString());
    if (section.mirrored) {
        addRow(tr("Espelhado"), tr("sim"), QString());
    }
    if (!section.geometry.isEmpty()) {
        addGroup(tr("Dimensões do perfil"));
        const auto *family = SectionShapes::findFamily(section.family);
        for (auto it = section.geometry.constBegin(); it != section.geometry.constEnd(); ++it) {
            QString label = it.key();
            if (family) {
                for (const auto &parameter : family->parameters) {
                    if (parameter.key == it.key()) {
                        label = parameter.label;
                        break;
                    }
                }
            }
            const bool angle = it.key().contains(QLatin1String("angle"));
            addRow(label, num(it.value(), 2), angle ? tr("°") : tr("mm"));
        }
    }

    // ---- secao bruta (integracao do contorno)
    GrossSection g;
    std::vector<Loop> loops;
    std::size_t outerCount = 1;
    if (!section.family.isEmpty()) {
        if (const auto shape = SectionShapes::build(section.family, section.geometry, section.mirrored)) {
            for (const auto &loop : shape->loops) {
                loops.emplace_back(loop.begin(), loop.end());
            }
            outerCount = shape->outerCount;
            g = computeGrossSection(loops, outerCount);
        }
    }
    const DesignSection ds = Structura::App::Services::DesignService::makeSection(section, QStringLiteral("AUTO"));

    if (g.valid) {
        addGroup(tr("Propriedades geométricas (seção bruta)"));
        addRow(tr("Área A"), num(g.area / 100.0, 3), tr("cm²"));
        addRow(tr("Largura total"), num(g.aMax - g.aMin, 2), tr("mm"));
        addRow(tr("Altura total"), num(g.bMax - g.bMin, 2), tr("mm"));
        addRow(tr("Centroide a partir do canto inferior-esquerdo"), tr("(%1 ; %2)").arg(num(-g.aMin, 2), num(-g.bMin, 2)), tr("mm"),
               tr("distâncias do centroide às fibras extremas"));
        addRow(tr("Inércia Ix (eixo x)"), num(g.Ix / 1e4, 2), tr("cm⁴"));
        addRow(tr("Inércia Iy (eixo y)"), num(g.Iy / 1e4, 2), tr("cm⁴"));
        if (std::abs(g.Ixy) > 1e-6 * std::max(g.Ix, g.Iy)) {
            addRow(tr("Produto de inércia Ixy"), num(g.Ixy / 1e4, 2), tr("cm⁴"), tr("seção sem eixo de simetria alinhado aos eixos do desenho"));
        }
        addRow(tr("Raio de giração rx"), num(g.rx / 10.0, 3), tr("cm"));
        addRow(tr("Raio de giração ry"), num(g.ry / 10.0, 3), tr("cm"));
        addRow(tr("Módulo elástico Wx (fibra superior)"), num(g.WxTop / 1e3, 2), tr("cm³"));
        addRow(tr("Módulo elástico Wx (fibra inferior)"), num(g.WxBottom / 1e3, 2), tr("cm³"));
        addRow(tr("Módulo elástico Wy (lado direito)"), num(g.WyRight / 1e3, 2), tr("cm³"));
        addRow(tr("Módulo elástico Wy (lado esquerdo)"), num(g.WyLeft / 1e3, 2), tr("cm³"));
        addRow(tr("Módulo plástico Zx"), num(g.Zx / 1e3, 2), tr("cm³"));
        addRow(tr("Módulo plástico Zy"), num(g.Zy / 1e3, 2), tr("cm³"));
        if (g.Wx() > 0.0 && g.Wy() > 0.0) {
            addRow(tr("Fator de forma Zx/Wx"), num(g.Zx / g.Wx(), 3), QString());
            addRow(tr("Fator de forma Zy/Wy"), num(g.Zy / g.Wy(), 3), QString());
        }
        addRow(tr("Linha neutra plástica (eixo x), b"), num(g.pnaX, 2), tr("mm"), tr("em relação ao centroide"));
        addRow(tr("Linha neutra plástica (eixo y), a"), num(g.pnaY, 2), tr("mm"), tr("em relação ao centroide"));
        // perimetro externo e area de pintura
        double perimeter = 0.0;
        for (std::size_t i = 0; i < loops.size(); ++i) {
            const auto &loop = loops[i];
            for (std::size_t k = 0; k < loop.size(); ++k) {
                const auto &p = loop[k], &q = loop[(k + 1) % loop.size()];
                perimeter += std::hypot(q[0] - p[0], q[1] - p[1]);
            }
        }
        addRow(tr("Perímetro (todos os contornos)"), num(perimeter, 1), tr("mm"), tr("área de pintura ≈ perímetro × comprimento"));
        addRow(tr("Massa linear (aço, 7850 kg/m³)"), num(g.area * 1e-6 * 7850.0, 2), tr("kg/m"));
        if (g.Ix > 0.0 && g.Iy > 0.0) {
            addRow(tr("Relação Ix/Iy"), num(g.Ix / g.Iy, 3), QString());
        }
    }

    // ---- torcao e empenamento
    if (ds.supported() || section.j > 0.0) {
        addGroup(tr("Torção e empenamento"));
        addRow(tr("Constante de torção J (usada na análise)"), num(section.j * 1e8, 3), tr("cm⁴"), tr("Saint-Venant"));
        if (ds.supported()) {
            if (ds.J > 0.0) {
                addRow(tr("Constante de torção J (dimensionamento)"), num(ds.J / 1e4, 3), tr("cm⁴"));
            }
            if (ds.Cw > 0.0) {
                addRow(tr("Constante de empenamento Cw (fórmulas da norma)"), sci(ds.Cw / 1e6), tr("cm⁶"));
            }
            if (ds.sectorial.valid) {
                addRow(tr("Constante de empenamento Cw (coordenada setorial)"), sci(ds.sectorial.Cw / 1e6), tr("cm⁶"), tr("linha média da seção, usada no empenamento do solver"));
                addRow(tr("Centro de cisalhamento (ys ; zs)"), tr("(%1 ; %2)").arg(num(ds.sectorial.ys / 10.0, 3), num(ds.sectorial.zs / 10.0, 3)), tr("cm"), tr("em relação ao centroide"));
                addRow(tr("Coordenada setorial máxima ωmax"), num(ds.sectorial.omegaMax / 100.0, 2), tr("cm²"));
            }
            if (ds.x0 != 0.0 || ds.y0 != 0.0) {
                addRow(tr("Centro de cisalhamento (x0 ; y0), norma"), tr("(%1 ; %2)").arg(num(ds.x0 / 10.0, 3), num(ds.y0 / 10.0, 3)), tr("cm"));
                const double r0sq = ds.g.rx * ds.g.rx + ds.g.ry * ds.g.ry + ds.x0 * ds.x0 + ds.y0 * ds.y0;
                addRow(tr("Raio de giração polar r0"), num(std::sqrt(r0sq) / 10.0, 3), tr("cm"), tr("r0² = rx² + ry² + x0² + y0²"));
            } else if (g.valid) {
                addRow(tr("Raio de giração polar r0"), num(std::sqrt(g.rx * g.rx + g.ry * g.ry) / 10.0, 3), tr("cm"), tr("seção com centro de cisalhamento no centroide"));
            }
        }
    }

    // ---- dimensionamento
    if (ds.supported()) {
        addGroup(tr("Parâmetros de dimensionamento"));
        addRow(tr("Tipo de seção"), QString::fromStdString(kindName(ds.kind)), QString());
        addRow(tr("Norma aplicável"), ds.standard == Standard::NBR8800 ? QStringLiteral("NBR 8800:2024") : QStringLiteral("NBR 14762:2010"), QString());
        if (std::abs(ds.principalAngleDeg) > 1e-9) {
            addRow(tr("Giro até os eixos principais"), num(ds.principalAngleDeg, 2), tr("°"));
            addRow(tr("Inércia principal maior I1"), num(ds.I1 / 1e4, 2), tr("cm⁴"));
            addRow(tr("Inércia principal menor I2"), num(ds.I2 / 1e4, 2), tr("cm⁴"));
        }
        if (ds.rMin > 0.0) {
            addRow(tr("Raio de giração mínimo do perfil componente"), num(ds.rMin / 10.0, 3), tr("cm"), tr("barra composta / cantoneira"));
        }
        if (ds.composite()) {
            addRow(tr("Área de um perfil componente"), num(ds.componentArea / 100.0, 3), tr("cm²"));
            addRow(tr("Afastamento entre os perfis"), num(ds.gapE, 2), tr("mm"));
            addRow(tr("Espaçamento ℓ entre as ligações"), tr("na janela de propriedades da barra"), QString(), tr("NBR 8800:2024, 5.3.6"));
        }
        for (const auto &element : ds.elements) {
            const double ratio = element.t > 0.0 ? element.b / element.t : 0.0;
            addRow(tr("Elemento %1 (b/t)").arg(QString::fromStdString(element.name)), num(ratio, 2), QString(),
                   tr("b = %1 mm, t = %2 mm, grupo %3, %4 un.").arg(num(element.b, 1), num(element.t, 2)).arg(element.group).arg(element.count));
        }
        if (ds.h > 0.0 && ds.tw > 0.0) {
            addRow(tr("Esbeltez da alma h/tw"), num(ds.h / ds.tw, 2), QString(), tr("h = %1 mm").arg(num(ds.h, 1)));
        }
        if (ds.bf > 0.0 && ds.tf > 0.0) {
            addRow(tr("Esbeltez da mesa bf/(2 tf)"), num(ds.bf / (2.0 * ds.tf), 2), QString());
        }
        addRow(tr("Simetria"), tr("%1 · %2")
                                   .arg(ds.symmetricAboutX ? tr("em relação ao eixo x") : tr("sem simetria em x"),
                                        ds.symmetricAboutY ? tr("em relação ao eixo y") : tr("sem simetria em y")),
               QString());
    }

    // ---- valores usados pelo solver
    addGroup(tr("Valores informados ao solver (análise)"));
    addRow(tr("Área A"), num(section.area * 1e4, 3), tr("cm²"));
    addRow(tr("Inércia Iz (flexão no plano da altura)"), num(section.iz * 1e8, 2), tr("cm⁴"));
    addRow(tr("Inércia Iy (flexão no plano da largura)"), num(section.iy * 1e8, 2), tr("cm⁴"));
    addRow(tr("Constante de torção J"), num(section.j * 1e8, 3), tr("cm⁴"));
    if (g.valid && section.area > 0.0) {
        const double diff = std::abs(section.area * 1e6 - g.area) / g.area;
        if (diff > 0.01) {
            addRow(tr("Diferença A (solver) × A (contorno)"), num(diff * 100.0, 2), tr("%"), tr("valor do catálogo ou informado pelo usuário prevalece na análise"));
        }
    }
    m_table->resizeRowsToContents();
}

void ProfilePropertiesDialog::copyToClipboard() const
{
    QStringList lines;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            const auto *item = m_table->item(r, c);
            cells << (item ? item->text() : QString());
        }
        lines << cells.join(QLatin1Char('\t'));
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

} // namespace Structura::UI
