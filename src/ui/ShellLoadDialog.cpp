#include "ui/ShellLoadDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <algorithm>

namespace Structura::UI {

namespace {
enum TypeIndex { NodalForce = 0, LineLoad = 1, SurfaceLoad = 2 };
enum ModeIndex { GlobalPerArea = 0, ProjectedPerArea = 1, NormalPressure = 2 };
}

ShellLoadDialog::ShellLoadDialog(const QStringList &caseNames, int nodes, int edges, int elements, QWidget *parent)
    : QDialog(parent)
    , m_nodes(nodes)
    , m_edges(edges)
    , m_elements(elements)
{
    setWindowTitle(tr("Carga nos nós de casca selecionados"));
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Seleção atual: %1 nó(s), %2 aresta(s) entre nós selecionados, %3 elemento(s) com os quatro nós selecionados.").arg(nodes).arg(edges).arg(elements)));

    auto *form = new QFormLayout;
    m_case = new QComboBox;
    m_case->setEditable(true);
    m_case->addItems(caseNames);
    form->addRow(tr("Caso de carga"), m_case);
    m_type = new QComboBox;
    m_type->addItems({tr("Força em cada nó"), tr("Carga linear sobre as arestas"), tr("Carga de superfície sobre os elementos")});
    form->addRow(tr("Tipo"), m_type);
    m_mode = new QComboBox;
    m_mode->addItems({tr("Força por área, direção global"), tr("Força por área projetada em planta, direção global"), tr("Pressão normal (+ no sentido da normal da casca)")});
    form->addRow(tr("Modo"), m_mode);
    const QStringList labels = {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
    for (int i = 0; i < 3; ++i) {
        m_value[static_cast<std::size_t>(i)] = new QDoubleSpinBox;
        m_value[static_cast<std::size_t>(i)]->setRange(-1e9, 1e9);
        m_value[static_cast<std::size_t>(i)]->setDecimals(4);
        m_value[static_cast<std::size_t>(i)]->setSingleStep(1.0);
        form->addRow(labels[i], m_value[static_cast<std::size_t>(i)]);
    }
    m_value[2]->setValue(-1.0);
    m_unit = new QLabel;
    form->addRow(tr("Unidade"), m_unit);
    layout->addLayout(form);
    m_hint = new QLabel;
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);
    auto *note = new QLabel(tr("A carga fica gravada por coordenadas (acompanha a malha quando ela é refeita). Resultados já calculados deixam de valer: reanalise depois de aplicar."));
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // tipos sem alvo ficam desabilitados
    auto *model = qobject_cast<QStandardItemModel *>(m_type->model());
    auto enable = [&](int index, bool on) {
        if (model && model->item(index)) {
            model->item(index)->setEnabled(on);
        }
    };
    enable(NodalForce, nodes > 0);
    enable(LineLoad, edges > 0);
    enable(SurfaceLoad, elements > 0);
    m_type->setCurrentIndex(nodes > 0 ? NodalForce : 0);
    if (edges > 0 && nodes > 0 && elements == 0 && edges >= nodes - 1) {
        m_type->setCurrentIndex(LineLoad);   // selecao em linha: a carga linear e a escolha provavel
    }
    if (elements > 0) {
        m_type->setCurrentIndex(SurfaceLoad);
    }
    connect(m_type, &QComboBox::currentIndexChanged, this, [this]() { updateFields(); });
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this]() { updateFields(); });
    updateFields();
}

void ShellLoadDialog::updateFields()
{
    const int type = m_type->currentIndex();
    const bool surface = type == SurfaceLoad;
    m_mode->setEnabled(surface);
    const bool normal = surface && m_mode->currentIndex() == NormalPressure;
    m_value[1]->setEnabled(!normal);
    m_value[2]->setEnabled(!normal);
    m_value[0]->setPrefix(normal ? tr("p = ") : QString());
    switch (type) {
    case NodalForce:
        m_unit->setText(tr("kN por nó"));
        m_hint->setText(tr("A mesma força é aplicada em cada um dos %1 nós. A força total é %1 vezes o valor informado.").arg(m_nodes));
        break;
    case LineLoad:
        m_unit->setText(tr("kN/m"));
        m_hint->setText(tr("Carga por comprimento; cada aresta recebe q·L/2 nos seus dois nós (força total = q × comprimento total das %1 aresta(s)).").arg(m_edges));
        break;
    default:
        m_unit->setText(normal ? tr("kPa") : tr("kN/m²"));
        m_hint->setText(tr("Aplicada nos %1 elemento(s); força total = valor × área dos elementos (na projeção em planta, × |n·z|).").arg(m_elements));
        break;
    }
}

Structura::Shell::Load ShellLoadDialog::load() const
{
    Structura::Shell::Load l;
    l.caseName = m_case->currentText().trimmed().toStdString();
    const int type = m_type->currentIndex();
    l.type = type == NodalForce ? Structura::Shell::Load::NodalPoints : (type == LineLoad ? Structura::Shell::Load::LinePoints : Structura::Shell::Load::RegionPoints);
    l.v = {m_value[0]->value() * 1000.0, m_value[1]->value() * 1000.0, m_value[2]->value() * 1000.0, 0.0};   // kN -> N
    if (type == SurfaceLoad) {
        const int mode = m_mode->currentIndex();
        l.v[3] = static_cast<double>(mode);
        if (mode == NormalPressure) {
            l.v = {m_value[0]->value() * 1000.0, 0.0, 0.0, 2.0};   // kPa -> Pa
        }
    }
    return l;
}

} // namespace Structura::UI
