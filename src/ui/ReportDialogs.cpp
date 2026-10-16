#include "ReportDialogs.h"

#include "DesignColors.h"
#include "report/DocxWriter.h"
#include "report/FormulaText.h"
#include "report/ReportHtml.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::ReportService;

void showReport(QTextBrowser *browser, const Structura::Report::ReportDocument &document, bool includeCover)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto result = Structura::Report::toHtml(document, 19.0, includeCover);
    browser->setHtml(result.html);
    for (auto it = result.images.constBegin(); it != result.images.constEnd(); ++it) {
        browser->document()->addResource(QTextDocument::ImageResource, QUrl(it.key()), it.value());
    }
    browser->document()->markContentsDirty(0, browser->document()->characterCount());
    QApplication::restoreOverrideCursor();
}

bool exportDocx(QWidget *parent, const Structura::Report::ReportDocument &document, const QString &suggestedName)
{
    const QString path = QFileDialog::getSaveFileName(parent, QObject::tr("Exportar para Word"), suggestedName, QObject::tr("Documento do Word (*.docx)"));
    if (path.isEmpty()) {
        return false;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto bytes = Structura::Report::writeDocx(document);
    QFile file(path);
    const bool ok = file.open(QIODevice::WriteOnly) && file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size());
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(parent, QObject::tr("Exportar"), QObject::tr("Não foi possível gravar o arquivo (ele pode estar aberto no Word)."));
        return false;
    }
    if (QMessageBox::question(parent, QObject::tr("Exportar"), QObject::tr("Arquivo gerado:\n%1\n\nAbrir agora?").arg(path)) == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// ReportDialog
// ---------------------------------------------------------------------------------------------------------------------

ReportDialog::ReportDialog(const ReportService::Input &input, const QVector<int> &selectedMembers, QWidget *parent)
    : QDialog(parent)
    , m_input(input)
    , m_selectedMembers(selectedMembers)
{
    setWindowTitle(tr("Memorial de cálculo e relatório geral"));
    resize(1280, 800);
    auto *root = new QHBoxLayout(this);

    // ── opcoes ────────────────────────────────────────────────────────
    auto *side = new QVBoxLayout;
    auto *general = new QGroupBox(tr("Documento"));
    auto *gl = new QVBoxLayout(general);
    m_cover = new QCheckBox(tr("Capa"));
    m_toc = new QCheckBox(tr("Sumário"));
    m_criteria = new QCheckBox(tr("Critérios e normas"));
    for (auto *c : {m_cover, m_toc, m_criteria}) {
        c->setChecked(true);
        gl->addWidget(c);
    }
    side->addWidget(general);

    auto *data = new QGroupBox(tr("Dados do projeto"));
    auto *dl = new QVBoxLayout(data);
    m_materials = new QCheckBox(tr("Materiais e seções"));
    m_geometry = new QCheckBox(tr("Geometria (nós, barras, vínculos)"));
    m_loads = new QCheckBox(tr("Ações, casos e combinações"));
    m_results = new QCheckBox(tr("Resultados da análise (deslocamentos e reações)"));
    for (auto *c : {m_materials, m_geometry, m_loads, m_results}) {
        c->setChecked(true);
        dl->addWidget(c);
    }
    side->addWidget(data);

    auto *design = new QGroupBox(tr("Dimensionamento"));
    auto *dg = new QVBoxLayout(design);
    m_designSummary = new QCheckBox(tr("Resumo por barra (tabela de η)"));
    m_designDetails = new QCheckBox(tr("Memória de cálculo por barra"));
    m_service = new QCheckBox(tr("Flechas, deslocamentos por andar e B2"));
    m_drawings = new QCheckBox(tr("Desenho e propriedades da seção"));
    m_formulas = new QCheckBox(tr("Fórmulas e valores (desmarque para só o resultado)"));
    for (auto *c : {m_designSummary, m_designDetails, m_service, m_drawings, m_formulas}) {
        c->setChecked(true);
        dg->addWidget(c);
    }
    m_scope = new QComboBox;
    m_scope->addItem(tr("Todas as barras"), static_cast<int>(ReportService::BarScope::All));
    m_scope->addItem(tr("Somente as que não atendem"), static_cast<int>(ReportService::BarScope::Failing));
    m_scope->addItem(tr("Somente as barras selecionadas (%1)").arg(selectedMembers.size()), static_cast<int>(ReportService::BarScope::Selected));
    dg->addWidget(new QLabel(tr("Barras incluídas:")));
    dg->addWidget(m_scope);
    dg->addWidget(new QLabel(tr("Itens de cálculo incluídos:")));
    m_items = new QListWidget;
    m_items->setMaximumHeight(190);
    for (const auto &item : ReportService::calculationItems()) {
        auto *it = new QListWidgetItem(item.title, m_items);
        it->setData(Qt::UserRole, item.id);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Checked);
    }
    dg->addWidget(m_items);
    design->setEnabled(m_input.hasDesign);
    if (!m_input.hasDesign) {
        design->setToolTip(tr("Execute a análise e verifique os perfis para incluir o dimensionamento."));
    }
    side->addWidget(design);
    side->addStretch(1);

    auto *refresh = new QPushButton(tr("Atualizar prévia"));
    auto *exportButton = new QPushButton(tr("Exportar para Word (.docx)…"));
    exportButton->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 7px 14px; border-radius: 4px; }"
                                               "QPushButton:hover { background: #0b6fa3; }"));
    side->addWidget(refresh);
    side->addWidget(exportButton);
    auto *sideWidget = new QWidget;
    sideWidget->setLayout(side);
    sideWidget->setFixedWidth(360);
    root->addWidget(sideWidget);

    // ── previa ────────────────────────────────────────────────────────
    auto *right = new QVBoxLayout;
    m_status = new QLabel;
    m_status->setStyleSheet(QStringLiteral("color: #5a6472;"));
    m_preview = new QTextBrowser;
    m_preview->setOpenLinks(false);
    m_preview->setStyleSheet(QStringLiteral("QTextBrowser { background: white; border: 1px solid #d5dce8; padding: 12px; }"));
    right->addWidget(m_status);
    right->addWidget(m_preview, 1);
    root->addLayout(right, 1);

    connect(refresh, &QPushButton::clicked, this, &ReportDialog::refreshPreview);
    connect(exportButton, &QPushButton::clicked, this, &ReportDialog::exportFile);
    refreshPreview();
}

ReportService::Options ReportDialog::options() const
{
    ReportService::Options o;
    o.cover = m_cover->isChecked();
    o.toc = m_toc->isChecked();
    o.criteria = m_criteria->isChecked();
    o.materialsAndSections = m_materials->isChecked();
    o.geometry = m_geometry->isChecked();
    o.supports = m_geometry->isChecked();
    o.loads = m_loads->isChecked();
    o.analysisResults = m_results->isChecked();
    o.designSummary = m_designSummary->isChecked();
    o.designDetails = m_designDetails->isChecked();
    o.serviceChecks = m_service->isChecked();
    o.sectionDrawings = m_drawings->isChecked();
    o.showFormulas = m_formulas->isChecked();
    o.scope = static_cast<ReportService::BarScope>(m_scope->currentData().toInt());
    o.selectedMembers = m_selectedMembers;
    bool all = true;
    for (int i = 0; i < m_items->count(); ++i) {
        const auto *it = m_items->item(i);
        if (it->checkState() == Qt::Checked) {
            o.itemIds.insert(it->data(Qt::UserRole).toString());
        } else {
            all = false;
        }
    }
    if (all) {
        o.itemIds.clear();
    }
    return o;
}

void ReportDialog::refreshPreview()
{
    // A previa limita o detalhamento por barra (o arquivo exportado contem todas)
    auto o = options();
    o.maxDetailBars = 6;
    const auto doc = ReportService::buildGlobal(m_input, o);
    showReport(m_preview, doc, true);
    m_status->setText(tr("Prévia: o detalhamento mostra no máximo %1 barras; o arquivo exportado segue as opções escolhidas.").arg(o.maxDetailBars));
}

void ReportDialog::exportFile()
{
    const auto doc = ReportService::buildGlobal(m_input, options());
    exportDocx(this, doc, QStringLiteral("Memorial de calculo - %1.docx").arg(m_input.projectName.isEmpty() ? QStringLiteral("projeto") : m_input.projectName));
}

// ---------------------------------------------------------------------------------------------------------------------
// BarReportDialog
// ---------------------------------------------------------------------------------------------------------------------

BarReportDialog::BarReportDialog(const ReportService::Input &input, int memberId, QWidget *parent)
    : QDialog(parent)
    , m_input(input)
    , m_memberId(memberId)
{
    setWindowTitle(tr("Memorial de cálculo — barra %1").arg(memberId));
    resize(1100, 780);
    auto *root = new QVBoxLayout(this);
    auto *split = new QSplitter(Qt::Horizontal);

    auto *left = new QWidget;
    auto *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 8, 0);
    auto *caption = new QLabel(tr("<b>Itens de cálculo</b>"));
    ll->addWidget(caption);
    m_list = new QListWidget;
    auto *full = new QListWidgetItem(tr("Memorial completo"), m_list);
    full->setData(Qt::UserRole, QString());
    QFont bold = full->font();
    bold.setBold(true);
    full->setFont(bold);
    for (const auto &v : m_input.verdicts) {
        if (v.memberId != memberId) {
            continue;
        }
        for (const auto &c : v.output.checks) {
            auto *item = new QListWidgetItem(QStringLiteral("%1   (η %2)").arg(QString::fromStdString(Structura::Report::restoreAccents(c.title)), QString::number(c.ratio, 'f', 2).replace('.', ',')), m_list);
            item->setData(Qt::UserRole, QString::fromStdString(c.id));
            item->setForeground(designEtaColor(c.ratio));
        }
    }
    m_list->setCurrentRow(0);
    ll->addWidget(m_list, 1);
    m_formulas = new QCheckBox(tr("Mostrar fórmulas e valores"));
    m_formulas->setChecked(true);
    ll->addWidget(m_formulas);
    left->setMinimumWidth(260);
    left->setMaximumWidth(360);
    split->addWidget(left);

    m_view = new QTextBrowser;
    m_view->setStyleSheet(QStringLiteral("QTextBrowser { background: white; border: 1px solid #d5dce8; padding: 12px; }"));
    split->addWidget(m_view);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    auto *buttons = new QHBoxLayout;
    auto *exportButton = new QPushButton(tr("Exportar para Word (.docx)…"));
    auto *close = new QPushButton(tr("Fechar"));
    buttons->addStretch(1);
    buttons->addWidget(exportButton);
    buttons->addWidget(close);
    root->addLayout(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) { refresh(); });
    connect(m_formulas, &QCheckBox::toggled, this, [this](bool) { refresh(); });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        const QString id = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QString();
        QSet<QString> ids;
        if (!id.isEmpty()) {
            ids.insert(id);
        }
        exportDocx(this, ReportService::buildBar(m_input, m_memberId, ids, m_formulas->isChecked()), QStringLiteral("Memorial barra %1.docx").arg(m_memberId));
    });
    refresh();
}

void BarReportDialog::refresh()
{
    const QString id = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QString();
    QSet<QString> ids;
    if (!id.isEmpty()) {
        ids.insert(id);
    }
    const auto doc = ReportService::buildBar(m_input, m_memberId, ids, m_formulas->isChecked(), id.isEmpty());
    showReport(m_view, doc, false);
}

} // namespace Structura::UI
