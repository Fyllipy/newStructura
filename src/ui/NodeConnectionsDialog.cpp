#include "NodeConnectionsDialog.h"

#include "ConnectionPreview.h"

#include "app/services/DesignService.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QIcon>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::ProjectData::NodeConnection;
using Structura::App::Services::ConnectionService;
namespace D = Structura::Design::Connections;

NodeConnectionsDialog::NodeConnectionsDialog(const QVector<NodeConnection> &connections, Callbacks callbacks, QWidget *parent)
    : QDialog(parent)
    , m_connections(connections)
    , m_callbacks(std::move(callbacks))
{
    setWindowTitle(tr("Ligações do modelo"));
    resize(1180, 860);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Ligações associadas aos nós do modelo: viga–apoio (chapa de cisalhamento, cantoneira dupla), chapa de nó de treliça/contraventamento, emenda de viga, "
                                "base de pilar e ligação tubular (NBR 8800:2024, itens 6.2, 6.3, 6.5 e 6.7; CIDECT DG1; NBR 6118 no bloco). Os esforços vêm das combinações últimas na extremidade "
                                "da barra; rode a análise antes. Distribuição elástica de parafusos e soldas, sem efeito alavanca; o apoio (alma/mesa do pilar ou do banzo) deve ser verificado à parte, "
                                "exceto na ligação tubular, em que o banzo é verificado."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto *split = new QSplitter(Qt::Horizontal);
    root->addWidget(split, 1);

    // ---- lista ------------------------------------------------------------------------------------------------
    auto *left = new QWidget;
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    m_list = new QTableWidget(0, 5);
    m_list->setHorizontalHeaderLabels({tr("Nó"), tr("Viga"), tr("Tipo"), tr("η"), tr("Situação")});
    m_list->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_list->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_list->verticalHeader()->hide();
    m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    leftLayout->addWidget(m_list, 1);
    auto *add = new QPushButton(tr("Nova ligação…"));
    add->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 12px; border-radius: 4px; }"));
    auto *remove = new QPushButton(tr("Excluir"));
    auto *all = new QPushButton(tr("Verificar todas"));
    auto *calc = new QPushButton(tr("Calculadora avulsa…"));
    for (auto *button : {add, remove, all, calc}) {
        leftLayout->addWidget(button);
    }
    left->setMinimumWidth(330);
    split->addWidget(left);

    // ---- detalhe ----------------------------------------------------------------------------------------------
    auto *right = new QWidget;
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_header = new QLabel(tr("Selecione ou crie uma ligação."));
    m_header->setStyleSheet(QStringLiteral("font-weight:700; font-size:11pt;"));
    rightLayout->addWidget(m_header);
    m_forcesLabel = new QLabel;
    m_forcesLabel->setWordWrap(true);
    m_forcesLabel->setStyleSheet(QStringLiteral("color:#5a6472;"));
    rightLayout->addWidget(m_forcesLabel);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_formHost = new QWidget;
    m_form = new QFormLayout(m_formHost);
    scroll->setWidget(m_formHost);
    scroll->setMinimumHeight(220);
    m_preview = new ConnectionPreview;
    auto *middle = new QHBoxLayout;
    middle->addWidget(m_preview, 1);
    middle->addWidget(scroll, 1);
    rightLayout->addLayout(middle, 1);

    auto *actions = new QHBoxLayout;
    auto *propose = new QPushButton(tr("Propor mais leve que atende"));
    propose->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 12px; border-radius: 4px; }"));
    auto *refresh = new QPushButton(tr("Atualizar esforços"));
    auto *focus = new QPushButton(tr("Ir para a viga"));
    auto *copy = new QPushButton(tr("Copiar memorial"));
    auto *support = new QPushButton(tr("Barra de apoio…"));
    support->setToolTip(tr("Escolhe em qual das barras que chegam ao nó a ligação se apoia; a barra fica destacada no modelo."));
    for (auto *button : {propose, refresh, focus, support, copy}) {
        actions->addWidget(button);
    }
    actions->addStretch(1);
    rightLayout->addLayout(actions);

    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels({tr("Verificação"), tr("Item NBR"), tr("Solicitação / valor"), tr("Resistência / limite"), tr("Razão")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(190);
    rightLayout->addWidget(m_table, 1);
    m_verdict = new QLabel;
    m_verdict->setWordWrap(true);
    rightLayout->addWidget(m_verdict);
    m_parts = new QLabel;
    m_parts->setWordWrap(true);
    m_parts->setStyleSheet(QStringLiteral("color:#5a6472;"));
    rightLayout->addWidget(m_parts);
    split->addWidget(right);
    split->setStretchFactor(1, 1);

    auto *close = new QPushButton(tr("Aplicar e fechar"));
    auto *cancel = new QPushButton(tr("Cancelar"));
    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    bottom->addWidget(close);
    bottom->addWidget(cancel);
    root->addLayout(bottom);

    connect(add, &QPushButton::clicked, this, [this]() { chooseType(); });
    connect(remove, &QPushButton::clicked, this, [this]() { removeCurrent(); });
    connect(all, &QPushButton::clicked, this, [this]() { verifyAll(); });
    connect(calc, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.calculator) {
            m_callbacks.calculator();
        }
    });
    connect(propose, &QPushButton::clicked, this, [this]() { proposeCurrent(); });
    connect(refresh, &QPushButton::clicked, this, [this]() { refreshForces(); });
    connect(focus, &QPushButton::clicked, this, [this]() {
        if (m_current >= 0 && m_callbacks.focus) {
            m_callbacks.focus(m_connections.at(m_current));
        }
    });
    connect(support, &QPushButton::clicked, this, [this]() {
        if (m_current < 0 || !m_callbacks.chooseSupport) {
            return;
        }
        auto &c = m_connections[m_current];
        const bool usesSupport = c.type == NodeConnection::kShearTab || c.type == NodeConnection::kDoubleAngle || c.type == NodeConnection::kGusset || c.type == NodeConnection::kEndPlate;
        if (!usesSupport) {
            QMessageBox::information(this, tr("Barra de apoio"), tr("Esta tipologia de ligação não tem barra de apoio."));
            return;
        }
        NodeConnection edited = c;
        if (m_callbacks.chooseSupport(edited)) {
            edited.verified = false;
            c = edited;
            selectRow(m_current);
        }
    });
    connect(copy, &QPushButton::clicked, this, [this]() { QApplication::clipboard()->setText(memorialText()); });
    connect(m_list, &QTableWidget::itemSelectionChanged, this, [this]() {
        const auto rows = m_list->selectionModel()->selectedRows();
        selectRow(rows.isEmpty() ? -1 : rows.first().row());
    });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    rebuildList();
    if (!m_connections.isEmpty()) {
        m_list->selectRow(0);
    }
}

void NodeConnectionsDialog::rebuildList()
{
    const int keep = m_current;
    m_list->blockSignals(true);
    m_list->setRowCount(m_connections.size());
    for (int i = 0; i < m_connections.size(); ++i) {
        const auto &c = m_connections[i];
        const QString status = !c.verified ? tr("não verificada") : (c.eta <= 1.0 + 1e-9 ? tr("atende") : tr("NÃO atende"));
        const QStringList cells {QString::number(c.nodeId), QStringLiteral("%1 (%2)").arg(c.memberId).arg(c.atEnd ? tr("fim") : tr("início")),
                                 ConnectionService::typeName(c.type), c.verified ? QString::number(c.eta, 'f', 2) : QStringLiteral("-"), status};
        for (int col = 0; col < cells.size(); ++col) {
            auto *item = new QTableWidgetItem(cells[col]);
            if (col >= 3 && c.verified) {
                item->setForeground(c.eta <= 1.0 + 1e-9 ? QColor(QStringLiteral("#1e8449")) : QColor(QStringLiteral("#c0392b")));
            }
            m_list->setItem(i, col, item);
        }
    }
    m_list->blockSignals(false);
    if (keep >= 0 && keep < m_connections.size()) {
        m_list->blockSignals(true);
        m_list->selectRow(keep);
        m_list->blockSignals(false);
    }
}

void NodeConnectionsDialog::selectRow(int row)
{
    m_current = row;
    m_forces.reset();
    if (row < 0 || row >= m_connections.size()) {
        m_current = -1;
        m_header->setText(tr("Selecione ou crie uma ligação."));
        m_forcesLabel->clear();
        m_table->setRowCount(0);
        m_verdict->clear();
        m_parts->clear();
        m_preview->clear();
        buildForm();
        return;
    }
    const auto &c = m_connections.at(row);
    m_header->setText(tr("%1 — nó %2, viga %3 (%4)").arg(ConnectionService::typeName(c.type)).arg(c.nodeId).arg(c.memberId).arg(c.atEnd ? tr("extremidade final") : tr("extremidade inicial")));
    m_preview->setConnection(c, c.verified ? c.eta : 0.0);
    buildForm();
    refreshForces();
}

void NodeConnectionsDialog::buildForm()
{
    m_building = true;
    m_fields.clear();
    while (m_form->rowCount() > 0) {
        m_form->removeRow(0);
    }
    m_grade = nullptr;
    m_thread = nullptr;
    m_steelPreset = nullptr;
    m_choices.clear();
    if (m_current >= 0) {
        const auto &c = m_connections.at(m_current);
        auto addSection = [this](const QString &title) {
            auto *label = new QLabel(title);
            label->setStyleSheet(QStringLiteral("font-weight:700; color:#0d5f8a; padding-top:8px; border-bottom:1px solid #c9d3de;"));
            m_form->addRow(label);
        };
        if (ConnectionService::usesBolts(c.type)) {
            m_grade = new QComboBox;
            for (const auto &g : D::boltGrades()) {
                m_grade->addItem(QString::fromStdString(g.name));
            }
            const int idx = m_grade->findText(c.grade);
            m_grade->setCurrentIndex(idx >= 0 ? idx : 2);
            m_form->addRow(tr("Parafuso"), m_grade);
            m_thread = new QCheckBox(tr("Plano de corte passa pela rosca"));
            m_thread->setChecked(c.values.value(QStringLiteral("thread"), 1.0) != 0.0);
            m_form->addRow(m_thread);
        }
        const auto keys = ConnectionService::steelKeys(c.type);
        if (!keys.first.isEmpty()) {
            m_steelPreset = new QComboBox;
            m_steelPreset->addItem(tr("(escolher para preencher fy/fu da chapa/cantoneira)"), QString());
            for (const auto &s : Structura::App::Services::DesignService::steelGrades()) {
                m_steelPreset->addItem(QStringLiteral("%1  (fy %2 / fu %3 MPa)").arg(s.name).arg(s.fy).arg(s.fu), s.name);
            }
            m_form->addRow(tr("Aço da chapa / cantoneira"), m_steelPreset);
        }
        QString lastGroup;
        for (const auto &def : ConnectionService::fields(c.type)) {
            if (def.group != lastGroup) {
                lastGroup = def.group;
                if (!lastGroup.isEmpty()) {
                    addSection(lastGroup);
                }
            }
            if (!def.options.isEmpty()) {
                auto *choice = new QComboBox;
                choice->addItems(def.options);
                choice->setCurrentIndex(std::clamp(static_cast<int>(c.values.value(def.key, 0.0) + 0.5), 0, static_cast<int>(def.options.size()) - 1));
                m_form->addRow(def.label, choice);
                m_choices.insert(def.key, choice);
                connect(choice, &QComboBox::currentIndexChanged, this, [this]() { readForm(); verifyCurrent(); });
                continue;
            }
            auto *box = new QDoubleSpinBox;
            box->setRange(def.min, def.max);
            box->setDecimals(def.decimals);
            box->setSuffix(def.suffix);
            box->setValue(c.values.value(def.key, def.min));
            if (!def.tip.isEmpty()) {
                box->setToolTip(def.tip);
            }
            m_form->addRow(def.label, box);
            m_fields.insert(def.key, box);
            connect(box, &QDoubleSpinBox::valueChanged, this, [this]() { readForm(); verifyCurrent(); });
        }
        if (m_grade) {
            connect(m_grade, &QComboBox::currentIndexChanged, this, [this]() { readForm(); verifyCurrent(); });
            connect(m_thread, &QCheckBox::toggled, this, [this]() { readForm(); verifyCurrent(); });
        }
        if (m_steelPreset) {
            connect(m_steelPreset, &QComboBox::currentIndexChanged, this, [this]() {
                const QString name = m_steelPreset->currentData().toString();
                const auto steel = Structura::App::Services::DesignService::findSteel(name);
                if (!steel || m_current < 0) {
                    return;
                }
                const auto pair = ConnectionService::steelKeys(m_connections.at(m_current).type);
                m_building = true;
                if (auto *fy = m_fields.value(pair.first)) {
                    fy->setValue(steel->fy);
                }
                if (auto *fu = m_fields.value(pair.second)) {
                    fu->setValue(steel->fu);
                }
                m_building = false;
                readForm();
                verifyCurrent();
            });
        }
    }
    m_building = false;
}

void NodeConnectionsDialog::readForm()
{
    if (m_building || m_current < 0) {
        return;
    }
    auto &c = m_connections[m_current];
    if (m_grade) {
        c.grade = m_grade->currentText();
        c.values[QStringLiteral("thread")] = m_thread->isChecked() ? 1.0 : 0.0;
    }
    for (auto it = m_fields.constBegin(); it != m_fields.constEnd(); ++it) {
        c.values[it.key()] = it.value()->value();
    }
    for (auto it = m_choices.constBegin(); it != m_choices.constEnd(); ++it) {
        c.values[it.key()] = it.value()->currentIndex();
    }
}

void NodeConnectionsDialog::refreshForces()
{
    m_forces.reset();
    if (m_current >= 0 && m_callbacks.forces) {
        m_forces = m_callbacks.forces(m_connections.at(m_current), m_connections);
    }
    if (m_forces) {
        const int type = m_connections.at(m_current).type;
        QString text;
        if (type == NodeConnection::kGusset) {
            text = tr("Esforços de cálculo na diagonal: tração máxima = %1 kN; compressão máxima = %2 kN.").arg(m_forces->tension / 1000.0, 0, 'f', 1).arg(m_forces->compression / 1000.0, 0, 'f', 1);
        } else if (type == NodeConnection::kSplice || type == NodeConnection::kBasePlate) {
            text = tr("Esforços na extremidade (envoltória; a verificação usa a combinação mais desfavorável, %1 combinações): V = %2 kN, N = %3 kN (%4), M = %5 kN·m.")
                       .arg(m_forces->cases.size()).arg(m_forces->shear / 1000.0, 0, 'f', 1).arg(std::abs(m_forces->axial) / 1000.0, 0, 'f', 1)
                       .arg(m_forces->axial > 0 ? tr("tração") : tr("compressão")).arg(m_forces->moment / 1e6, 0, 'f', 1);
        } else if (type == NodeConnection::kTubular) {
            text = tr("Esforços do banzo (N0, M0) e das diagonais (N1, N2) lidos de %1 combinação(ões); a verificação usa a mais desfavorável.").arg(m_forces->cases.size());
        } else {
            text = tr("Esforços de cálculo na extremidade: V = %1 kN (resultante), N = %2 kN (%3).")
                       .arg(m_forces->shear / 1000.0, 0, 'f', 1).arg(std::abs(m_forces->axial) / 1000.0, 0, 'f', 1)
                       .arg(m_forces->axial > 0 ? tr("tração") : (m_forces->axial < 0 ? tr("compressão") : tr("nulo")));
        }
        m_forcesLabel->setText(text);
    } else {
        m_forcesLabel->setText(tr("Sem esforços: rode a análise e o dimensionamento (a ligação será verificada com V = 0)."));
    }
    verifyCurrent();
}

void NodeConnectionsDialog::verifyCurrent()
{
    if (m_building || m_current < 0) {
        return;
    }
    auto &c = m_connections[m_current];
    ConnectionService::Forces forces;
    if (m_forces) {
        forces = *m_forces;
    }
    m_evaluation = ConnectionService::evaluate(c, forces);
    c.eta = m_evaluation.maxRatio;
    c.governing = m_evaluation.governing;
    c.verified = m_forces.has_value();
    showEvaluation(m_evaluation);
    // atualiza a linha da lista sem perder a selecao
    const int row = m_current;
    m_list->blockSignals(true);
    m_list->setItem(row, 3, new QTableWidgetItem(c.verified ? QString::number(c.eta, 'f', 2) : QStringLiteral("-")));
    m_list->setItem(row, 4, new QTableWidgetItem(!c.verified ? tr("não verificada") : (c.eta <= 1.0 + 1e-9 ? tr("atende") : tr("NÃO atende"))));
    m_list->blockSignals(false);
}

void NodeConnectionsDialog::showEvaluation(const ConnectionService::Evaluation &e)
{
    if (m_current >= 0 && m_current < m_connections.size()) {
        m_preview->setConnection(m_connections.at(m_current), e.maxRatio);
    }
    m_table->setRowCount(static_cast<int>(e.checks.size()));
    for (std::size_t i = 0; i < e.checks.size(); ++i) {
        const auto &c = e.checks[i];
        const QString unit = QString::fromStdString(c.unit);
        const QStringList cells {QString::fromStdString(c.name), QString::fromStdString(c.clause), QStringLiteral("%1 %2").arg(c.demand, 0, 'f', 2).arg(unit),
                                 QStringLiteral("%1 %2").arg(c.resistance, 0, 'f', 2).arg(unit), QString::number(c.ratio(), 'f', 3)};
        for (int col = 0; col < cells.size(); ++col) {
            auto *item = new QTableWidgetItem(cells[col]);
            if (col == 4) {
                item->setForeground(c.ok() ? QColor(QStringLiteral("#1e8449")) : QColor(QStringLiteral("#c0392b")));
                QFont font;
                font.setBold(true);
                item->setFont(font);
            }
            if (col == 0) {
                QStringList tip;
                for (const auto &l : c.lines) {
                    tip << QString::fromStdString(l);
                }
                item->setToolTip(tip.join(QLatin1Char('\n')));
            }
            m_table->setItem(static_cast<int>(i), col, item);
        }
    }
    const bool ok = e.maxRatio <= 1.0 + 1e-9;
    m_verdict->setStyleSheet(ok ? QStringLiteral("color:#1e8449; font-weight:600;") : QStringLiteral("color:#c0392b; font-weight:600;"));
    m_verdict->setText(ok ? tr("Atende (razão máxima %1 — %2).").arg(e.maxRatio, 0, 'f', 3).arg(e.governing)
                          : tr("NÃO atende (razão máxima %1 — %2).").arg(e.maxRatio, 0, 'f', 3).arg(e.governing));
    QStringList parts;
    double mass = 0.0;
    for (const auto &p : e.parts) {
        parts << QStringLiteral("%1 × %2").arg(p.quantity).arg(QString::fromStdString(p.description));
        mass += p.totalMass();
    }
    m_parts->setText(tr("Peças: %1. Massa aproximada: %2 kg.").arg(parts.join(QStringLiteral("; ")), QString::number(mass, 'f', 1)));
}

void NodeConnectionsDialog::chooseType()
{
    struct Entry {
        int type;
        QString group;
    };
    const QVector<Entry> entries = {{NodeConnection::kShearTab, tr("Viga–apoio")},
                                    {NodeConnection::kDoubleAngle, tr("Viga–apoio")},
                                    {NodeConnection::kGusset, tr("Treliça e contraventamento")},
                                    {NodeConnection::kTubular, tr("Treliça tubular")},
                                    {NodeConnection::kEndPlate, tr("Viga–pilar (rígida)")},
                                    {NodeConnection::kSplice, tr("Continuidade de viga")},
                                    {NodeConnection::kBasePlate, tr("Fundação")},
                                    {NodeConnection::kColdFormed, tr("Perfis formados a frio")}};
    QDialog picker(this);
    picker.setWindowTitle(tr("Nova ligação"));
    picker.resize(760, 700);
    auto *layout = new QVBoxLayout(&picker);
    layout->addWidget(new QLabel(tr("Escolha a tipologia. Em seguida, selecione no modelo a barra indicada (a extremidade será perguntada).")));
    auto *list = new QListWidget;
    list->setWordWrap(true);
    list->setSpacing(2);
    list->setIconSize(QSize(150, 100));
    for (const auto &entry : entries) {
        auto *item = new QListWidgetItem(QIcon(ConnectionPreview::example(entry.type, QSize(150, 100))), QStringLiteral("%1  ·  %2\n%3").arg(ConnectionService::typeName(entry.type), entry.group, ConnectionService::typeSummary(entry.type)));
        item->setData(Qt::UserRole, entry.type);
        item->setSizeHint(QSize(0, 108));
        list->addItem(item);
    }
    list->setCurrentRow(0);
    layout->addWidget(list, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &picker, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &picker, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &picker, &QDialog::accept);
    if (picker.exec() != QDialog::Accepted || !list->currentItem()) {
        return;
    }
    addConnection(list->currentItem()->data(Qt::UserRole).toInt());
}

void NodeConnectionsDialog::addConnection(int type)
{
    if (!m_callbacks.create) {
        return;
    }
    // QDialog::hide() encerraria o exec() deste dialogo modal (e o programa ficaria sem janela): em vez disso
    // a janela fica transparente enquanto o usuario escolhe a barra e o no no modelo.
    setWindowOpacity(0.0);
    NodeConnection created = m_callbacks.create(type);
    setWindowOpacity(1.0);
    raise();
    activateWindow();
    if (created.id == 0) {
        return;
    }
    int nextId = 1;
    for (const auto &c : m_connections) {
        nextId = std::max(nextId, c.id + 1);
    }
    created.id = nextId;
    // uma ligacao por (no, viga)
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections[i].nodeId == created.nodeId && m_connections[i].memberId == created.memberId) {
            if (QMessageBox::question(this, tr("Ligações"), tr("Já existe uma ligação nesse nó para essa viga. Substituir?")) != QMessageBox::Yes) {
                return;
            }
            m_connections.removeAt(i);
            break;
        }
    }
    m_connections.append(created);
    rebuildList();
    m_list->selectRow(m_connections.size() - 1);
    // proposta inicial com os esforcos da analise (se houver)
    proposeCurrent(false);
}

void NodeConnectionsDialog::removeCurrent()
{
    if (m_current < 0) {
        return;
    }
    m_connections.removeAt(m_current);
    m_current = -1;
    rebuildList();
    if (!m_connections.isEmpty()) {
        m_list->selectRow(0);
    } else {
        selectRow(-1);
    }
}

void NodeConnectionsDialog::proposeCurrent(bool announce)
{
    if (m_current < 0) {
        return;
    }
    readForm();
    ConnectionService::Forces forces;
    if (m_forces) {
        forces = *m_forces;
    }
    auto &c = m_connections[m_current];
    if (!ConnectionService::hasProposal(c.type)) {
        if (announce) {
            QMessageBox::information(this, tr("Ligações"), tr("A ligação tubular não tem proposta automática: ajuste os perfis, o afastamento/sobreposição e a excentricidade do nó "
                                                              "até que todas as razões fiquem abaixo de 1,0."));
        }
        verifyCurrent();
        return;
    }
    if (forces.shear <= 0.0 && std::abs(forces.axial) <= 0.0 && forces.tension <= 0.0 && forces.compression <= 0.0 && forces.moment <= 0.0) {
        verifyCurrent();
        return;
    }
    if (!ConnectionService::propose(c, forces)) {
        if (announce) {
            QMessageBox::information(this, tr("Ligações"), tr("Nenhuma combinação de parafusos, chapa e solda do conjunto pesquisado atende estes esforços. "
                                                              "Ajuste a geometria manualmente ou use outro tipo de ligação."));
        }
        verifyCurrent();
        return;
    }
    buildForm();
    verifyCurrent();
}

void NodeConnectionsDialog::verifyAll()
{
    const int keep = m_current;
    for (int i = 0; i < m_connections.size(); ++i) {
        m_current = i;
        const auto forces = m_callbacks.forces ? m_callbacks.forces(m_connections.at(i), m_connections) : std::nullopt;
        auto &c = m_connections[i];
        const auto e = ConnectionService::evaluate(c, forces.value_or(ConnectionService::Forces {}));
        c.eta = e.maxRatio;
        c.governing = e.governing;
        c.verified = forces.has_value();
    }
    m_current = keep;
    rebuildList();
    if (keep >= 0) {
        selectRow(keep);
    }
}

QString NodeConnectionsDialog::memorialText() const
{
    if (m_current < 0) {
        return QString();
    }
    const auto &c = m_connections.at(m_current);
    QString text = tr("LIGAÇÃO – %1\nNó %2, viga %3 (%4)\n%5\n").arg(ConnectionService::typeName(c.type)).arg(c.nodeId).arg(c.memberId)
                       .arg(c.atEnd ? tr("extremidade final") : tr("extremidade inicial")).arg(ConnectionService::describe(c));
    if (m_forces) {
        text += tr("Esforços de cálculo: V = %1 kN, N = %2 kN\n").arg(m_forces->shear / 1000.0, 0, 'f', 2).arg(m_forces->axial / 1000.0, 0, 'f', 2);
    }
    text += QLatin1Char('\n');
    for (const auto &check : m_evaluation.checks) {
        text += QStringLiteral("%1 [%2]  %3 / %4 %5 = %6  %7\n")
                    .arg(QString::fromStdString(check.name), QString::fromStdString(check.clause))
                    .arg(check.demand, 0, 'f', 2).arg(check.resistance, 0, 'f', 2).arg(QString::fromStdString(check.unit))
                    .arg(check.ratio(), 0, 'f', 3).arg(check.ok() ? QStringLiteral("OK") : QStringLiteral("NAO ATENDE"));
        for (const auto &line : check.lines) {
            text += QStringLiteral("    %1\n").arg(QString::fromStdString(line));
        }
    }
    return text;
}

} // namespace Structura::UI
