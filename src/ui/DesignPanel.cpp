#include "DesignPanel.h"

#include "DesignColors.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>

namespace Structura::UI {

using Structura::App::Services::DesignService;
using Structura::App::Services::StructuralChecks;

namespace {

constexpr int kEtaRole = Qt::UserRole + 1;
constexpr int kMemberRole = Qt::UserRole + 2;

/// Numero ordenavel (compara pelo valor, nao pelo texto).
class NumberItem final : public QTableWidgetItem
{
public:
    NumberItem(double value, int decimals)
        : QTableWidgetItem(QString::number(value, 'f', decimals))
        , m_value(value)
    {
        setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    bool operator<(const QTableWidgetItem &other) const override
    {
        const auto *n = dynamic_cast<const NumberItem *>(&other);
        return n ? m_value < n->m_value : QTableWidgetItem::operator<(other);
    }

private:
    double m_value;
};

/// Barra de aproveitamento: preenchimento proporcional a eta (cheio em 1,0) com a cor da faixa.
class EtaDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QVariant v = index.data(kEtaRole);
        if (!v.isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        const double eta = v.toDouble();
        painter->save();
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(option.rect, option.palette.highlight());
        }
        const QRect r = option.rect.adjusted(4, 5, -4, -5);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(QStringLiteral("#e4e8ee")));
        painter->drawRoundedRect(r, 3, 3);
        QRect fill = r;
        fill.setWidth(static_cast<int>(r.width() * std::min(1.0, eta)));
        painter->setBrush(designEtaColor(eta));
        painter->drawRoundedRect(fill, 3, 3);
        painter->setPen(eta > 0.45 ? Qt::white : QColor(QStringLiteral("#1e232b")));
        QFont f = painter->font();
        f.setBold(true);
        painter->setFont(f);
        painter->drawText(r, Qt::AlignCenter, QString::number(eta, 'f', 2));
        painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        s.setHeight(std::max(s.height(), 24));
        return s;
    }
};

QTableWidget *makeTable(const QStringList &headers)
{
    auto *table = new QTableWidget(0, headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(26);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    return table;
}

QTableWidgetItem *textItem(const QString &text, const QColor &color = QColor(), bool bold = false)
{
    auto *item = new QTableWidgetItem(text);
    if (color.isValid()) {
        item->setForeground(QBrush(color));
    }
    if (bold) {
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
    }
    return item;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// DesignPanel
// ---------------------------------------------------------------------------------------------------------------------

QWidget *DesignPanel::makeCard(const QString &caption, QLabel **value)
{
    auto *card = new QFrame;
    card->setStyleSheet(QStringLiteral("QFrame { background: #ffffff; border: 1px solid #d5dce8; border-radius: 6px; }"
                                       "QLabel { border: 0; background: transparent; }"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(0);
    *value = new QLabel(QStringLiteral("-"));
    (*value)->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: #1e232b;"));
    auto *cap = new QLabel(caption);
    cap->setStyleSheet(QStringLiteral("font-size: 11px; color: #5a6472;"));
    layout->addWidget(*value);
    layout->addWidget(cap);
    return card;
}

DesignPanel::DesignPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(430);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    m_title = new QLabel(tr("Dimensionamento"));
    m_title->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 700; color: #1e232b;"));
    root->addWidget(m_title);

    auto *actions = new QHBoxLayout;
    auto *verify = new QPushButton(tr("Verificar"));
    verify->setToolTip(tr("Verifica os perfis adotados com os esforços da última análise"));
    auto *optimize = new QPushButton(tr("Dimensionar..."));
    optimize->setToolTip(tr("Propõe o perfil mais leve que atende, com prévia antes de aplicar"));
    optimize->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 4px 12px; border-radius: 4px; }"
                                           "QPushButton:hover { background: #0b6fa3; }"));
    auto *settings = new QPushButton(tr("Parâmetros..."));
    m_changeProfile = new QPushButton(tr("Trocar perfil..."));
    m_changeProfile->setToolTip(tr("Mostra os perfis da série com o aproveitamento de cada um e troca o perfil da barra selecionada (e do seu grupo)"));
    m_changeProfile->setEnabled(false);
    actions->addWidget(verify);
    actions->addWidget(optimize);
    actions->addWidget(m_changeProfile);
    actions->addStretch(1);
    actions->addWidget(settings);
    root->addLayout(actions);
    connect(verify, &QPushButton::clicked, this, &DesignPanel::verifyRequested);
    connect(optimize, &QPushButton::clicked, this, &DesignPanel::optimizeRequested);
    connect(settings, &QPushButton::clicked, this, &DesignPanel::settingsRequested);
    connect(m_changeProfile, &QPushButton::clicked, this, [this] {
        const int row = m_table->currentRow();
        if (row >= 0 && m_table->item(row, 0)) {
            emit changeProfileRequested(m_table->item(row, 0)->data(kMemberRole).toInt());
        }
    });

    auto *cards = new QHBoxLayout;
    cards->setSpacing(6);
    cards->addWidget(makeCard(tr("atendem"), &m_cardOk));
    cards->addWidget(makeCard(tr("não atendem"), &m_cardFail));
    cards->addWidget(makeCard(tr("η máximo"), &m_cardEta));
    cards->addWidget(makeCard(tr("peso (kg)"), &m_cardWeight));
    root->addLayout(cards);

    m_banner = new QLabel;
    m_banner->setWordWrap(true);
    m_banner->setVisible(false);
    m_banner->setStyleSheet(QStringLiteral("background: #fff4e0; border: 1px solid #f0c987; border-radius: 4px; padding: 6px; color: #7a4b00;"));
    root->addWidget(m_banner);

    m_tabs = new QTabWidget;
    // ── Barras ────────────────────────────────────────────────────────
    auto *barsPage = new QWidget;
    auto *barsLayout = new QVBoxLayout(barsPage);
    barsLayout->setContentsMargins(0, 6, 0, 0);
    auto *filters = new QHBoxLayout;
    m_filter = new QComboBox;
    m_filter->addItems({tr("Todas as barras"), tr("Não atendem"), tr("η > 0,9"), tr("Não verificadas")});
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Buscar barra ou perfil"));
    m_search->setClearButtonEnabled(true);
    filters->addWidget(m_filter);
    filters->addWidget(m_search, 1);
    barsLayout->addLayout(filters);
    m_table = makeTable({tr("Barra"), tr("Perfil"), tr("η"), tr("Governa")});
    m_table->setSortingEnabled(true);
    m_table->setItemDelegateForColumn(2, new EtaDelegate(m_table));
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 90);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    barsLayout->addWidget(m_table, 1);
    m_tabs->addTab(barsPage, tr("Barras"));

    // ── Servico ───────────────────────────────────────────────────────
    auto *servicePage = new QWidget;
    auto *serviceLayout = new QVBoxLayout(servicePage);
    serviceLayout->setContentsMargins(0, 6, 0, 0);
    serviceLayout->addWidget(new QLabel(tr("<b>Flechas</b> (combinações ELS, NBR 8800 Tab. B.1)")));
    m_deflections = makeTable({tr("Barra"), tr("Comb."), tr("δ (mm)"), tr("Limite (mm)"), tr("L/n"), tr("δ / limite")});
    m_deflections->setItemDelegateForColumn(5, new EtaDelegate(m_deflections));
    m_deflections->setSortingEnabled(true);
    serviceLayout->addWidget(m_deflections, 1);
    serviceLayout->addWidget(new QLabel(tr("<b>Deslocamentos por andar</b> e B2 (Anexo C)")));
    m_stories = makeTable({tr("Andar"), tr("Comb."), tr("Δ rel. (mm)"), tr("Limite"), tr("η"), tr("B2 x / y")});
    m_stories->setItemDelegateForColumn(4, new EtaDelegate(m_stories));
    serviceLayout->addWidget(m_stories, 1);
    m_tabs->addTab(servicePage, tr("Serviço"));

    // ── Memoria ───────────────────────────────────────────────────────
    auto *memoryPage = new QWidget;
    auto *memoryLayout = new QVBoxLayout(memoryPage);
    memoryLayout->setContentsMargins(0, 6, 0, 0);
    m_detail = new QPlainTextEdit;
    m_detail->setReadOnly(true);
    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    m_detail->setFont(mono);
    m_detail->setPlaceholderText(tr("Selecione uma barra na aba Barras para ver a memória de cálculo."));
    auto *exportButton = new QPushButton(tr("Exportar memória de todas as barras..."));
    memoryLayout->addWidget(m_detail, 1);
    memoryLayout->addWidget(exportButton, 0, Qt::AlignRight);
    connect(exportButton, &QPushButton::clicked, this, &DesignPanel::exportReport);
    m_tabs->addTab(memoryPage, tr("Memória"));
    root->addWidget(m_tabs, 1);

    connect(m_filter, &QComboBox::currentIndexChanged, this, [this](int) { rebuildTable(); });
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &) { rebuildTable(); });
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row) {
        if (m_updating || row < 0 || !m_table->item(row, 0)) {
            return;
        }
        const int member = m_table->item(row, 0)->data(kMemberRole).toInt();
        m_changeProfile->setEnabled(true);
        showDetail(member);
        emit memberActivated(member);
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (m_table->item(row, 0)) {
            emit changeProfileRequested(m_table->item(row, 0)->data(kMemberRole).toInt());
        }
    });
    connect(m_deflections, &QTableWidget::currentCellChanged, this, [this](int row) {
        if (!m_updating && row >= 0 && m_deflections->item(row, 0)) {
            emit memberActivated(m_deflections->item(row, 0)->data(kMemberRole).toInt());
        }
    });
}

void DesignPanel::setData(const DesignPanelData &data)
{
    m_data = data;
    m_title->setText(data.title.isEmpty() ? tr("Dimensionamento") : data.title);
    int ok = 0, fail = 0;
    double worst = 0.0;
    double weight = 0.0;
    for (const auto &v : data.verdicts) {
        if (!v.supported || v.output.checks.empty()) {
            continue;
        }
        (v.output.maxRatio > 1.0 ? fail : ok)++;
        worst = std::max(worst, v.output.maxRatio);
        weight += data.memberWeightKg.value(v.memberId, 0.0);
    }
    m_cardOk->setText(QString::number(ok));
    m_cardOk->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: #1e8449;"));
    m_cardFail->setText(QString::number(fail));
    m_cardFail->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: %1;").arg(fail > 0 ? QStringLiteral("#c0392b") : QStringLiteral("#1e232b")));
    m_cardEta->setText(QString::number(worst, 'f', 2));
    m_cardEta->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: %1;").arg(designEtaColor(worst).name()));
    m_cardWeight->setText(QString::number(weight, 'f', 0));

    // Servico
    m_updating = true;
    m_deflections->setSortingEnabled(false);
    m_deflections->setRowCount(data.deflections.size());
    int deflFail = 0;
    for (int i = 0; i < data.deflections.size(); ++i) {
        const auto &d = data.deflections[i];
        auto *bar = new NumberItem(d.memberId, 0);
        bar->setData(kMemberRole, d.memberId);
        m_deflections->setItem(i, 0, bar);
        m_deflections->setItem(i, 1, textItem(d.combination));
        m_deflections->setItem(i, 2, new NumberItem(d.deflectionMm, 1));
        m_deflections->setItem(i, 3, new NumberItem(d.limitMm, 1));
        m_deflections->setItem(i, 4, textItem(d.cantilever ? QStringLiteral("2L/%1").arg(d.denominator) : QStringLiteral("L/%1").arg(d.denominator)));
        auto *eta = new NumberItem(d.ratio, 2);
        eta->setData(kEtaRole, d.ratio);
        m_deflections->setItem(i, 5, eta);
        deflFail += d.ratio > 1.0 ? 1 : 0;
    }
    m_deflections->setSortingEnabled(true);
    m_deflections->sortByColumn(5, Qt::DescendingOrder);
    m_deflections->setColumnWidth(5, 90);

    QVector<QStringList> rows;
    m_stories->setRowCount(0);
    int driftFail = 0;
    const auto &st = data.settings;
    const int storyCount = data.stories.size();
    for (const auto &r : data.storyResults) {
        const double drift = std::max(r.driftXMm, r.driftYMm);
        double limit = 0.0;
        QString limitText = QStringLiteral("-");
        if (!st.singleStorey) {
            limit = r.heightMm / 500.0;
            limitText = QStringLiteral("h/500 = %1 mm").arg(limit, 0, 'f', 1);
        }
        double eta = limit > 0 ? drift / limit : 0.0;
        QString driftText = QString::number(drift, 'f', 1);
        if (r.story == storyCount - 1) {
            const double topLimit = r.heightFromBaseMm / (st.singleStorey ? 300.0 : 400.0);
            const double topEta = r.topDisplacementMm / topLimit;
            if (topEta > eta) {
                eta = topEta;
                limitText = QStringLiteral("H/%1 = %2 mm (topo: %3)").arg(st.singleStorey ? 300 : 400).arg(topLimit, 0, 'f', 1).arg(r.topDisplacementMm, 0, 'f', 1);
            }
        }
        const int row = m_stories->rowCount();
        m_stories->insertRow(row);
        m_stories->setItem(row, 0, textItem(tr("Andar %1 (%2–%3 m)").arg(r.story + 1).arg(data.stories[r.story].zBottom, 0, 'f', 1).arg(data.stories[r.story].zTop, 0, 'f', 1)));
        m_stories->setItem(row, 1, textItem(r.combination));
        m_stories->setItem(row, 2, textItem(driftText));
        m_stories->setItem(row, 3, textItem(limitText));
        auto *etaItem = new QTableWidgetItem;
        etaItem->setData(kEtaRole, eta);
        m_stories->setItem(row, 4, etaItem);
        m_stories->setItem(row, 5, textItem(QStringLiteral("%1 / %2").arg(r.b2x, 0, 'f', 3).arg(r.b2y, 0, 'f', 3)));
        driftFail += eta > 1.0 ? 1 : 0;
    }
    m_stories->setColumnWidth(4, 90);
    m_tabs->setTabText(1, (deflFail + driftFail) > 0 ? tr("Serviço (%1 ✗)").arg(deflFail + driftFail) : tr("Serviço"));

    QStringList notes = data.warnings;
    if (deflFail + driftFail > 0) {
        notes.prepend(tr("Serviço: %1 flecha(s) e %2 deslocamento(s) de andar acima do limite.").arg(deflFail).arg(driftFail));
    }
    m_banner->setVisible(!notes.isEmpty());
    m_banner->setText(notes.join(QStringLiteral("<br>")));
    m_updating = false;
    rebuildTable();
}

void DesignPanel::rebuildTable()
{
    m_updating = true;
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    const int filter = m_filter->currentIndex();
    const QString needle = m_search->text().trimmed();
    for (const auto &v : m_data.verdicts) {
        const bool checked = v.supported && !v.output.checks.empty();
        const double eta = checked ? v.output.maxRatio : 0.0;
        if ((filter == 1 && !(checked && eta > 1.0)) || (filter == 2 && !(checked && eta > 0.9)) || (filter == 3 && checked)) {
            continue;
        }
        if (!needle.isEmpty() && !QString::number(v.memberId).contains(needle) && !v.sectionName.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *id = new NumberItem(v.memberId, 0);
        id->setData(kMemberRole, v.memberId);
        m_table->setItem(row, 0, id);
        m_table->setItem(row, 1, textItem(v.sectionName));
        auto *etaItem = new NumberItem(eta, 3);
        if (checked) {
            etaItem->setData(kEtaRole, eta);
            etaItem->setText(QString());
        } else {
            etaItem->setText(QStringLiteral("—"));
        }
        m_table->setItem(row, 2, etaItem);
        QString governs;
        if (checked) {
            const auto &gov = v.output.checks.front();
            governs = QString::fromStdString(gov.title);
        } else if (!v.output.warnings.empty()) {
            governs = QString::fromStdString(v.output.warnings.front());
        }
        m_table->setItem(row, 3, textItem(governs, checked && eta > 1.0 ? QColor(QStringLiteral("#c0392b")) : QColor(), checked && eta > 1.0));
    }
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(2, Qt::DescendingOrder);
    m_updating = false;
}

void DesignPanel::selectMember(int memberId)
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->item(row, 0)->data(kMemberRole).toInt() == memberId) {
            m_updating = true;
            m_table->selectRow(row);
            m_table->scrollToItem(m_table->item(row, 0));
            m_updating = false;
            showDetail(memberId);
            return;
        }
    }
}

void DesignPanel::showDetail(int memberId)
{
    for (const auto &v : m_data.verdicts) {
        if (v.memberId == memberId) {
            m_detail->setPlainText(DesignService::report(v, m_data.settings));
            return;
        }
    }
}

void DesignPanel::exportReport()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar memória de cálculo"), QStringLiteral("memoria_calculo.txt"), tr("Texto (*.txt)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Exportar"), tr("Não foi possível gravar o arquivo."));
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (const auto &v : m_data.verdicts) {
        stream << DesignService::report(v, m_data.settings) << "\n" << QString(100, QLatin1Char('=')) << "\n\n";
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// DesignPreviewDialog
// ---------------------------------------------------------------------------------------------------------------------

DesignPreviewDialog::DesignPreviewDialog(const QVector<Row> &rows, double targetEta, QWidget *parent)
    : QDialog(parent)
    , m_rows(rows)
{
    setWindowTitle(tr("Prévia do dimensionamento"));
    resize(900, 480);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Estimativa com os esforços atuais (η alvo = %1). Ao aplicar, os perfis são trocados e a estrutura é reanalisada até convergir, "
                                "pois a redistribuição de rigidez altera os esforços. Desmarque os grupos que devem permanecer como estão.")
                                 .arg(targetEta, 0, 'f', 2));
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_table = makeTable({tr("Aplicar"), tr("Grupo"), tr("Barras"), tr("Perfil atual"), tr("η atual"), tr("Perfil proposto"), tr("η novo"), tr("Peso (kg)")});
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const Row &r = rows[i];
        auto *check = new QTableWidgetItem;
        check->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        const bool changes = r.found && r.proposed != r.current;
        check->setCheckState(changes ? Qt::Checked : Qt::Unchecked);
        if (!changes) {
            check->setFlags(Qt::NoItemFlags);
        }
        check->setData(Qt::UserRole, r.key);
        m_table->setItem(i, 0, check);
        m_table->setItem(i, 1, textItem(r.label));
        m_table->setItem(i, 2, textItem(QString::number(r.bars)));
        m_table->setItem(i, 3, textItem(r.current));
        m_table->setItem(i, 4, textItem(QString::number(r.currentEta, 'f', 2), designEtaColor(r.currentEta), r.currentEta > 1.0));
        if (!r.found) {
            m_table->setItem(i, 5, textItem(tr("nenhum atende"), QColor(QStringLiteral("#c0392b")), true));
            m_table->setItem(i, 6, textItem(QStringLiteral("—")));
            m_table->setItem(i, 7, textItem(QString::number(r.currentWeightKg, 'f', 0)));
        } else {
            m_table->setItem(i, 5, textItem(changes ? r.proposed : tr("%1 (mantido)").arg(r.proposed), QColor(), changes));
            m_table->setItem(i, 6, textItem(QString::number(r.proposedEta, 'f', 2), designEtaColor(r.proposedEta)));
            m_table->setItem(i, 7, textItem(QStringLiteral("%1 → %2").arg(r.currentWeightKg, 0, 'f', 0).arg(r.proposedWeightKg, 0, 'f', 0)));
        }
        if (!r.note.isEmpty()) {
            m_table->item(i, 1)->setToolTip(r.note);
        }
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setStretchLastSection(true);
    root->addWidget(m_table, 1);
    m_total = new QLabel;
    m_total->setStyleSheet(QStringLiteral("font-weight: 600;"));
    root->addWidget(m_total);
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *) { updateTotals(); });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto *apply = buttons->addButton(tr("Aplicar e reanalisar"), QDialogButtonBox::AcceptRole);
    apply->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
    updateTotals();
}

void DesignPreviewDialog::updateTotals()
{
    double before = 0.0, after = 0.0;
    for (int i = 0; i < m_rows.size(); ++i) {
        const bool apply = m_table->item(i, 0)->checkState() == Qt::Checked;
        before += m_rows[i].currentWeightKg;
        after += apply ? m_rows[i].proposedWeightKg : m_rows[i].currentWeightKg;
    }
    const double delta = after - before;
    m_total->setText(tr("Peso total: %1 kg → %2 kg (%3%4 kg)").arg(before, 0, 'f', 0).arg(after, 0, 'f', 0).arg(delta >= 0 ? QStringLiteral("+") : QString()).arg(delta, 0, 'f', 0));
}

QStringList DesignPreviewDialog::excludedKeys() const
{
    QStringList keys;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        const auto *item = m_table->item(i, 0);
        if (item->checkState() != Qt::Checked) {
            keys.append(item->data(Qt::UserRole).toString());
        }
    }
    return keys;
}

// ---------------------------------------------------------------------------------------------------------------------
// ProfileChoiceDialog
// ---------------------------------------------------------------------------------------------------------------------

ProfileChoiceDialog::ProfileChoiceDialog(const QString &title, const QString &subtitle, const QVector<Row> &rows, QWidget *parent)
    : QDialog(parent)
    , m_rows(rows)
{
    setWindowTitle(title);
    resize(620, 520);
    auto *root = new QVBoxLayout(this);
    auto *sub = new QLabel(subtitle);
    sub->setWordWrap(true);
    root->addWidget(sub);
    m_onlyPassing = new QCheckBox(tr("Mostrar apenas os perfis que atendem (η ≤ 1,00)"));
    m_onlyPassing->setChecked(true);
    root->addWidget(m_onlyPassing);
    m_table = makeTable({tr("Perfil"), tr("Área (cm²)"), tr("Peso (kg/m)"), tr("η")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setItemDelegateForColumn(3, new EtaDelegate(m_table));
    m_table->setColumnWidth(3, 90);
    root->addWidget(m_table, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    m_ok = buttons->addButton(tr("Aplicar perfil"), QDialogButtonBox::AcceptRole);
    m_ok->setEnabled(false);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
    connect(m_onlyPassing, &QCheckBox::toggled, this, [this](bool) { rebuild(); });
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row) {
        m_selected = row >= 0 && m_table->item(row, 0) ? m_table->item(row, 0)->data(Qt::UserRole).toString() : QString();
        m_ok->setEnabled(!m_selected.isEmpty());
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (m_table->item(row, 0)) {
            m_selected = m_table->item(row, 0)->data(Qt::UserRole).toString();
            accept();
        }
    });
    rebuild();
}

void ProfileChoiceDialog::rebuild()
{
    m_table->setRowCount(0);
    for (const Row &r : m_rows) {
        if (m_onlyPassing->isChecked() && !r.current && (!r.supported || r.eta > 1.0)) {
            continue;
        }
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *name = textItem(r.current ? tr("%1  (atual)").arg(r.name) : r.name, QColor(), r.current);
        name->setData(Qt::UserRole, r.name);
        m_table->setItem(row, 0, name);
        m_table->setItem(row, 1, new NumberItem(r.areaCm2, 1));
        m_table->setItem(row, 2, new NumberItem(r.kgPerM, 1));
        auto *eta = new NumberItem(r.eta, 2);
        if (r.supported) {
            eta->setData(kEtaRole, r.eta);
            eta->setText(QString());
        } else {
            eta->setText(QStringLiteral("—"));
        }
        m_table->setItem(row, 3, eta);
    }
    m_selected.clear();
    m_ok->setEnabled(false);
}

} // namespace Structura::UI
