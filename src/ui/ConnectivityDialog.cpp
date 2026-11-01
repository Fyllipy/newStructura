#include "ConnectivityDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

namespace PD = Structura::App::ProjectData;

namespace {

const char *kDofNames[6] = {"ux", "uy", "uz", "rx", "ry", "rz"};

QString nodeList(const QVector<int> &nodes, int limit = 8)
{
    QStringList parts;
    for (int i = 0; i < nodes.size() && i < limit; ++i) {
        parts << QString::number(nodes[i]);
    }
    return parts.join(QStringLiteral(", ")) + (nodes.size() > limit ? QStringLiteral(" … (%1)").arg(nodes.size()) : QString());
}

QString lawKindName(int kind)
{
    static const char *names[] = {"linear", "só tração", "só compressão", "folga de tração", "folga de compressão", "elastoplástica", "multilinear", "rígida"};
    return QString::fromUtf8(names[std::clamp(kind, 0, 7)]);
}

} // namespace

ConnectivityDialog::ConnectivityDialog(const PD::ProjectExtras &extras, const QVector<int> &selectedNodes, const QSet<int> &existingNodes, QWidget *parent)
    : QDialog(parent)
    , m_extras(extras)
    , m_selected(selectedNodes)
    , m_existing(existingNodes)
{
    setWindowTitle(tr("Vínculos e ligações entre nós"));
    resize(720, 460);
    auto *root = new QVBoxLayout(this);
    auto *note = new QLabel(tr("Selecione os nós na viewport antes de abrir este diálogo: cada botão \"Adicionar\" usa a seleção (%1 nó(s) selecionado(s): %2).")
                                .arg(m_selected.size()).arg(m_selected.isEmpty() ? tr("nenhum") : nodeList(m_selected)));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #5a6472;"));
    root->addWidget(note);

    m_tabs = new QTabWidget(this);
    struct Tab {
        QListWidget **list;
        QString title;
        QString hint;
        void (ConnectivityDialog::*add)();
    };
    const Tab tabs[] = {
        {&m_diaphragms, tr("Diafragmas rígidos"), tr("Os nós escravos se movem com o mestre como corpo rígido no plano do pavimento (lajes rígidas)."), &ConnectivityDialog::addDiaphragm},
        {&m_rigidLinks, tr("Ligações rígidas"), tr("Corpo rígido de 6 graus de liberdade, translações iguais, rotações iguais ou um único grau de liberdade entre dois nós."), &ConnectivityDialog::addRigidLink},
        {&m_inclined, tr("Apoios inclinados"), tr("Restringe o deslocamento ou a rotação do nó segundo direções quaisquer (rolete em plano inclinado)."), &ConnectivityDialog::addInclinedSupport},
        {&m_links, tr("Ligações (molas)"), tr("Mola ou elemento de interface entre dois nós, com lei de força-deformação por grau de liberdade (linear, só compressão, folga, elastoplástica, multilinear)."), &ConnectivityDialog::addLink},
        {&m_constraints, tr("Vínculos lineares"), tr("Relação linear genérica Σ coef × u = 0 entre graus de liberdade da seleção."), &ConnectivityDialog::addConstraint},
    };
    for (const Tab &tab : tabs) {
        auto *page = new QWidget(this);
        auto *layout = new QVBoxLayout(page);
        auto *hint = new QLabel(tab.hint, page);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: #5a6472;"));
        layout->addWidget(hint);
        *tab.list = new QListWidget(page);
        (*tab.list)->setSelectionMode(QAbstractItemView::ExtendedSelection);
        layout->addWidget(*tab.list, 1);
        auto *buttons = new QHBoxLayout;
        auto *add = new QPushButton(tr("Adicionar com a seleção…"), page);
        auto *remove = new QPushButton(tr("Remover selecionados"), page);
        connect(add, &QPushButton::clicked, this, tab.add);
        connect(remove, &QPushButton::clicked, this, &ConnectivityDialog::removeSelected);
        buttons->addWidget(add);
        buttons->addWidget(remove);
        buttons->addStretch(1);
        layout->addLayout(buttons);
        m_tabs->addTab(page, tab.title);
    }
    root->addWidget(m_tabs, 1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
    refresh();
}

bool ConnectivityDialog::requireSelection(int minimum, const QString &what)
{
    if (m_selected.size() < minimum) {
        QMessageBox::information(this, windowTitle(), tr("%1: selecione ao menos %2 nó(s) na viewport e reabra o diálogo.").arg(what).arg(minimum));
        return false;
    }
    return true;
}

void ConnectivityDialog::refresh()
{
    m_diaphragms->clear();
    for (const auto &d : m_extras.diaphragms) {
        const char axis = "XYZ"[std::clamp(d.normalAxis, 0, 2)];
        m_diaphragms->addItem(tr("%1 — mestre %2, normal %3, escravos: %4").arg(d.name.isEmpty() ? tr("(sem nome)") : d.name).arg(d.masterNode).arg(QLatin1Char(axis)).arg(nodeList(d.slaveNodes)));
    }
    m_rigidLinks->clear();
    for (const auto &l : m_extras.rigidLinks) {
        static const char *kinds[] = {"corpo rígido (6 gdl)", "3 translações iguais", "3 rotações iguais", "1 gdl igual"};
        m_rigidLinks->addItem(tr("Mestre %1 → escravo %2: %3%4").arg(l.masterNode).arg(l.slaveNode).arg(QString::fromUtf8(kinds[std::clamp(l.kind, 0, 3)]))
                                  .arg(l.kind == 3 ? QStringLiteral(" (%1)").arg(QLatin1String(kDofNames[std::clamp(l.dof, 0, 5)])) : QString()));
    }
    m_inclined->clear();
    for (const auto &s : m_extras.inclinedSupports) {
        QStringList restrained;
        for (int k = 0; k < 6; ++k) {
            if ((s.mask >> k) & 1) {
                restrained << QStringLiteral("%1e%2").arg(k < 3 ? QStringLiteral("u") : QStringLiteral("r")).arg(k % 3 + 1);
            }
        }
        m_inclined->addItem(tr("Nó %1: restringe %2; e1 = (%3; %4; %5)").arg(s.nodeId).arg(restrained.join(QLatin1Char(' ')))
                                .arg(s.axes[0], 0, 'f', 3).arg(s.axes[1], 0, 'f', 3).arg(s.axes[2], 0, 'f', 3));
    }
    m_links->clear();
    for (const auto &link : m_extras.links) {
        QStringList laws;
        for (int d = 0; d < 6; ++d) {
            const auto &law = link.laws[static_cast<std::size_t>(d)];
            if (law.k != 0.0 || law.kind == 6) {
                laws << QStringLiteral("%1: %2 (k = %3)").arg(QLatin1String(kDofNames[d]), lawKindName(law.kind)).arg(law.k, 0, 'g', 5);
            }
        }
        m_links->addItem(tr("Ligação %1 %2: nós %3 → %4; %5").arg(link.id).arg(link.name).arg(link.nodeI).arg(link.nodeJ).arg(laws.join(QStringLiteral("; "))));
    }
    m_constraints->clear();
    for (const auto &c : m_extras.constraints) {
        QStringList terms;
        for (int i = 0; i < c.nodes.size(); ++i) {
            terms << QStringLiteral("%1·%2(nó %3)").arg(c.coefs.value(i), 0, 'g', 5).arg(QLatin1String(kDofNames[std::clamp(c.dofs.value(i), 0, 5)])).arg(c.nodes[i]);
        }
        m_constraints->addItem(terms.join(QStringLiteral(" + ")) + QStringLiteral(" = 0") + (c.reportReaction ? tr("  [reação]") : QString()));
    }
}

void ConnectivityDialog::removeSelected()
{
    auto eraseRows = [](QListWidget *list, auto &vector) {
        QList<int> rows;
        for (const auto *item : list->selectedItems()) {
            rows.append(list->row(item));
        }
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (const int row : rows) {
            vector.removeAt(row);
        }
    };
    switch (m_tabs->currentIndex()) {
    case 0: eraseRows(m_diaphragms, m_extras.diaphragms); break;
    case 1: eraseRows(m_rigidLinks, m_extras.rigidLinks); break;
    case 2: eraseRows(m_inclined, m_extras.inclinedSupports); break;
    case 3: eraseRows(m_links, m_extras.links); break;
    default: eraseRows(m_constraints, m_extras.constraints); break;
    }
    refresh();
}

void ConnectivityDialog::addDiaphragm()
{
    if (!requireSelection(2, tr("Diafragma rígido"))) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Novo diafragma rígido"));
    auto *form = new QFormLayout(&dialog);
    auto *name = new QLineEdit(tr("Pavimento %1").arg(m_extras.diaphragms.size() + 1), &dialog);
    auto *master = new QComboBox(&dialog);
    for (const int node : m_selected) {
        master->addItem(tr("Nó %1").arg(node), node);
    }
    auto *axis = new QComboBox(&dialog);
    axis->addItem(tr("Z (laje horizontal)"), 2);
    axis->addItem(tr("Y (plano XZ)"), 1);
    axis->addItem(tr("X (plano YZ)"), 0);
    form->addRow(tr("Nome"), name);
    form->addRow(tr("Nó mestre"), master);
    form->addRow(tr("Eixo normal ao plano"), axis);
    form->addRow(new QLabel(tr("Escravos: os outros %1 nó(s) da seleção.").arg(m_selected.size() - 1), &dialog));
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(box);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    PD::RigidDiaphragm d;
    d.name = name->text().trimmed();
    d.masterNode = master->currentData().toInt();
    d.normalAxis = axis->currentData().toInt();
    for (const int node : m_selected) {
        if (node != d.masterNode) {
            d.slaveNodes.append(node);
        }
    }
    m_extras.diaphragms.append(d);
    refresh();
    m_tabs->setCurrentIndex(0);
}

void ConnectivityDialog::addRigidLink()
{
    if (!requireSelection(2, tr("Ligação rígida"))) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Nova ligação rígida"));
    auto *form = new QFormLayout(&dialog);
    auto *master = new QComboBox(&dialog);
    for (const int node : m_selected) {
        master->addItem(tr("Nó %1").arg(node), node);
    }
    auto *kind = new QComboBox(&dialog);
    kind->addItem(tr("Corpo rígido (6 graus de liberdade, com braço de alavanca)"), 0);
    kind->addItem(tr("Três translações iguais"), 1);
    kind->addItem(tr("Três rotações iguais"), 2);
    kind->addItem(tr("Um grau de liberdade igual"), 3);
    auto *dof = new QComboBox(&dialog);
    for (int i = 0; i < 6; ++i) {
        dof->addItem(QLatin1String(kDofNames[i]), i);
    }
    form->addRow(tr("Nó mestre"), master);
    form->addRow(tr("Tipo"), kind);
    form->addRow(tr("Grau de liberdade (tipo 4)"), dof);
    form->addRow(new QLabel(tr("Um escravo por nó restante da seleção (%1).").arg(m_selected.size() - 1), &dialog));
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(box);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    for (const int node : m_selected) {
        if (node != master->currentData().toInt()) {
            m_extras.rigidLinks.append({master->currentData().toInt(), node, kind->currentData().toInt(), dof->currentData().toInt()});
        }
    }
    refresh();
    m_tabs->setCurrentIndex(1);
}

void ConnectivityDialog::addInclinedSupport()
{
    if (!requireSelection(1, tr("Apoio inclinado"))) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Novo apoio inclinado"));
    auto *form = new QFormLayout(&dialog);
    auto spin = [&](double value) {
        auto *s = new QDoubleSpinBox(&dialog);
        s->setRange(-1e6, 1e6);
        s->setDecimals(4);
        s->setValue(value);
        return s;
    };
    auto *nx = spin(0.0), *ny = spin(0.0), *nz = spin(1.0);
    form->addRow(tr("Direção e1 — componente X"), nx);
    form->addRow(tr("Direção e1 — componente Y"), ny);
    form->addRow(tr("Direção e1 — componente Z"), nz);
    auto *auxiliary = new QComboBox(&dialog);
    auxiliary->addItem(tr("e2 no plano horizontal (e1 × Z)"), 0);
    auxiliary->addItem(tr("e2 pelo eixo global X"), 1);
    form->addRow(tr("Direção auxiliar"), auxiliary);
    QCheckBox *restrain[6];
    const QString names[6] = {tr("deslocamento em e1"), tr("deslocamento em e2"), tr("deslocamento em e3"), tr("rotação em torno de e1"), tr("rotação em torno de e2"), tr("rotação em torno de e3")};
    for (int i = 0; i < 6; ++i) {
        restrain[i] = new QCheckBox(names[i], &dialog);
        restrain[i]->setChecked(i == 0);
        form->addRow(QString(), restrain[i]);
    }
    form->addRow(new QLabel(tr("Aplica-se a %1 nó(s) da seleção.").arg(m_selected.size()), &dialog));
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(box);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    double e1[3] = {nx->value(), ny->value(), nz->value()};
    const double length = std::sqrt(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
    if (length < 1e-9) {
        QMessageBox::warning(this, windowTitle(), tr("A direção e1 não pode ser nula."));
        return;
    }
    for (double &c : e1) {
        c /= length;
    }
    // e2 = e1 x reference ; e3 = e1 x e2
    double reference[3] = {0, 0, 1};
    if (auxiliary->currentData().toInt() == 1 || std::abs(e1[2]) > 0.999) {
        reference[0] = 1;
        reference[2] = 0;
        if (std::abs(e1[0]) > 0.999) {
            reference[0] = 0;
            reference[1] = 1;
        }
    }
    auto cross = [](const double a[3], const double b[3], double out[3]) {
        out[0] = a[1] * b[2] - a[2] * b[1];
        out[1] = a[2] * b[0] - a[0] * b[2];
        out[2] = a[0] * b[1] - a[1] * b[0];
    };
    double e2[3], e3[3];
    cross(e1, reference, e2);
    const double l2 = std::sqrt(e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2]);
    for (double &c : e2) {
        c /= l2;
    }
    cross(e1, e2, e3);
    int mask = 0;
    for (int i = 0; i < 6; ++i) {
        mask |= restrain[i]->isChecked() ? (1 << i) : 0;
    }
    if (mask == 0) {
        return;
    }
    for (const int node : m_selected) {
        PD::InclinedSupport s;
        s.nodeId = node;
        s.mask = mask;
        s.axes = {{e1[0], e1[1], e1[2], e2[0], e2[1], e2[2], e3[0], e3[1], e3[2]}};
        m_extras.inclinedSupports.append(s);
    }
    refresh();
    m_tabs->setCurrentIndex(2);
}

void ConnectivityDialog::addLink()
{
    if (!requireSelection(2, tr("Ligação"))) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Nova ligação entre nós"));
    auto *root = new QVBoxLayout(&dialog);
    auto *top = new QFormLayout;
    auto *nodeI = new QComboBox(&dialog), *nodeJ = new QComboBox(&dialog);
    for (const int node : m_selected) {
        nodeI->addItem(tr("Nó %1").arg(node), node);
        nodeJ->addItem(tr("Nó %1").arg(node), node);
    }
    nodeJ->setCurrentIndex(std::min(1, static_cast<int>(m_selected.size()) - 1));
    auto *name = new QLineEdit(tr("Mola %1").arg(m_extras.links.size() + 1), &dialog);
    auto *axes = new QComboBox(&dialog);
    axes->addItem(tr("Eixo x na direção I → J"), 0);
    axes->addItem(tr("Eixos globais X, Y, Z"), 1);
    top->addRow(tr("Nome"), name);
    top->addRow(tr("Nó I"), nodeI);
    top->addRow(tr("Nó J"), nodeJ);
    top->addRow(tr("Eixos locais"), axes);
    root->addLayout(top);

    auto *grid = new QGridLayout;
    const QStringList headers = {tr("Grau de liberdade"), tr("Lei"), tr("Rigidez k (kN/m ou kN·m/rad)"), tr("Folga (m,rad) / escoamento (kN)"), tr("Enrijecimento"), tr("Pontos (def:força; …)")};
    for (int c = 0; c < headers.size(); ++c) {
        grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(headers[c]), &dialog), 0, c);
    }
    QComboBox *kinds[6];
    QDoubleSpinBox *k[6], *p1[6], *p2[6];
    QLineEdit *tables[6];
    const QString dofLabels[6] = {tr("ux (força, eixo x)"), tr("uy (força, eixo y)"), tr("uz (força, eixo z)"), tr("rx (momento, eixo x)"), tr("ry (momento, eixo y)"), tr("rz (momento, eixo z)")};
    for (int d = 0; d < 6; ++d) {
        grid->addWidget(new QLabel(dofLabels[d], &dialog), d + 1, 0);
        kinds[d] = new QComboBox(&dialog);
        for (int kind = 0; kind <= 7; ++kind) {
            kinds[d]->addItem(lawKindName(kind), kind);
        }
        k[d] = new QDoubleSpinBox(&dialog);
        k[d]->setRange(0.0, 1e15);
        k[d]->setDecimals(3);
        k[d]->setValue(0.0);
        p1[d] = new QDoubleSpinBox(&dialog);
        p1[d]->setRange(0.0, 1e15);
        p1[d]->setDecimals(6);
        p2[d] = new QDoubleSpinBox(&dialog);
        p2[d]->setRange(0.0, 1.0);
        p2[d]->setDecimals(4);
        p2[d]->setValue(0.01);
        tables[d] = new QLineEdit(&dialog);
        tables[d]->setPlaceholderText(tr("-0,01:-300; 0:0; 0,01:300"));
        grid->addWidget(kinds[d], d + 1, 1);
        grid->addWidget(k[d], d + 1, 2);
        grid->addWidget(p1[d], d + 1, 3);
        grid->addWidget(p2[d], d + 1, 4);
        grid->addWidget(tables[d], d + 1, 5);
    }
    root->addLayout(grid);
    auto *hint = new QLabel(tr("Rigidez 0 = grau de liberdade livre. Leis: só compressão/tração (apoio unilateral), folga (age após vencer a abertura), elastoplástica (escoamento e "
                               "razão de enrijecimento), multilinear (pontos deformação:força) e rígida (k muito alta). Leis não lineares ativam a análise incremental."), &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #5a6472;"));
    root->addWidget(hint);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(box);
    dialog.resize(980, dialog.sizeHint().height());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (nodeI->currentData().toInt() == nodeJ->currentData().toInt()) {
        QMessageBox::warning(this, windowTitle(), tr("Escolha dois nós diferentes."));
        return;
    }
    PD::LinkElement link;
    int maxId = 0;
    for (const auto &existing : m_extras.links) {
        maxId = std::max(maxId, existing.id);
    }
    link.id = maxId + 1;
    link.name = name->text().trimmed();
    link.nodeI = nodeI->currentData().toInt();
    link.nodeJ = nodeJ->currentData().toInt();
    if (axes->currentData().toInt() == 1) {
        link.axes = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    }
    bool anyLaw = false;
    for (int d = 0; d < 6; ++d) {
        auto &law = link.laws[static_cast<std::size_t>(d)];
        law.kind = kinds[d]->currentData().toInt();
        law.k = k[d]->value();
        law.p1 = p1[d]->value();
        law.p2 = p2[d]->value();
        if (law.kind == 6) {
            for (const QString &pair : tables[d]->text().split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                const QStringList xy = QString(pair).replace(QLatin1Char(','), QLatin1Char('.')).split(QLatin1Char(':'));
                if (xy.size() == 2) {
                    law.table.append({xy[0].trimmed().toDouble(), xy[1].trimmed().toDouble()});
                }
            }
            std::sort(law.table.begin(), law.table.end());
            if (law.table.size() < 2) {
                QMessageBox::warning(this, windowTitle(), tr("A lei multilinear de %1 precisa de ao menos dois pontos (deformação:força).").arg(QLatin1String(kDofNames[d])));
                return;
            }
        }
        anyLaw = anyLaw || law.k != 0.0 || law.kind == 6;
    }
    if (!anyLaw) {
        QMessageBox::information(this, windowTitle(), tr("Nenhum grau de liberdade com rigidez: ligação não criada."));
        return;
    }
    m_extras.links.append(link);
    refresh();
    m_tabs->setCurrentIndex(3);
}

void ConnectivityDialog::addConstraint()
{
    if (!requireSelection(1, tr("Vínculo linear"))) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Novo vínculo linear"));
    auto *root = new QVBoxLayout(&dialog);
    root->addWidget(new QLabel(tr("Σ coeficiente × deslocamento = 0. Até 4 termos; deixe o coeficiente 0 para ignorar o termo."), &dialog));
    auto *grid = new QGridLayout;
    QComboBox *node[4], *dof[4];
    QDoubleSpinBox *coef[4];
    for (int i = 0; i < 4; ++i) {
        node[i] = new QComboBox(&dialog);
        for (const int n : m_selected) {
            node[i]->addItem(tr("Nó %1").arg(n), n);
        }
        node[i]->setCurrentIndex(std::min(i, static_cast<int>(m_selected.size()) - 1));
        dof[i] = new QComboBox(&dialog);
        for (int d = 0; d < 6; ++d) {
            dof[i]->addItem(QLatin1String(kDofNames[d]), d);
        }
        coef[i] = new QDoubleSpinBox(&dialog);
        coef[i]->setRange(-1e9, 1e9);
        coef[i]->setDecimals(6);
        coef[i]->setValue(i == 0 ? 1.0 : (i == 1 ? -1.0 : 0.0));
        grid->addWidget(coef[i], i, 0);
        grid->addWidget(new QLabel(QStringLiteral("×"), &dialog), i, 1);
        grid->addWidget(dof[i], i, 2);
        grid->addWidget(node[i], i, 3);
    }
    root->addLayout(grid);
    auto *report = new QCheckBox(tr("Incluir a força do vínculo nas reações"), &dialog);
    root->addWidget(report);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(box);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    PD::LinearConstraint c;
    c.reportReaction = report->isChecked();
    for (int i = 0; i < 4; ++i) {
        if (coef[i]->value() != 0.0) {
            c.nodes.append(node[i]->currentData().toInt());
            c.dofs.append(dof[i]->currentData().toInt());
            c.coefs.append(coef[i]->value());
        }
    }
    if (c.nodes.isEmpty()) {
        return;
    }
    m_extras.constraints.append(c);
    refresh();
    m_tabs->setCurrentIndex(4);
}

} // namespace Structura::UI
