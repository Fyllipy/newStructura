#include "JointAnalysisDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QStringConverter>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

using namespace Structura::Design::JointClass;
using Structura::App::Services::JointAnalysisService;

namespace {

QString n(double v, int digits = 5)
{
    const double a = std::abs(v);
    if (a >= 1.0e4 && a < 1.0e9) {
        return QString::number(v, 'f', 0);
    }
    return QString::number(v, 'g', digits);
}

QString classColor(StiffnessClass c)
{
    switch (c) {
    case StiffnessClass::Pinned: return QStringLiteral("#b8641a");
    case StiffnessClass::SemiRigid: return QStringLiteral("#8a6d00");
    case StiffnessClass::Rigid: return QStringLiteral("#1d7a3a");
    case StiffnessClass::NotClassified: break;
    }
    return QStringLiteral("#555555");
}

/// Esquema do no: barras projetadas no plano de analise, as selecionadas em destaque.
class SchematicWidget final : public QWidget
{
public:
    explicit SchematicWidget(const JointAnalysisService::Context &ctx, QWidget *parent = nullptr)
        : QWidget(parent), m_ctx(ctx)
    {
        setMinimumHeight(220);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), Qt::white);
        const auto nrm = m_ctx.planeNormal;
        // base do plano: v = Z projetado (ou X se o plano for horizontal); h = v x n
        std::array<double, 3> v {0, 0, 1};
        double dv = v[0] * nrm[0] + v[1] * nrm[1] + v[2] * nrm[2];
        std::array<double, 3> vp {v[0] - dv * nrm[0], v[1] - dv * nrm[1], v[2] - dv * nrm[2]};
        double len = std::sqrt(vp[0] * vp[0] + vp[1] * vp[1] + vp[2] * vp[2]);
        if (len < 1e-6) {
            vp = {1, 0, 0};
            dv = nrm[0];
            vp = {1 - dv * nrm[0], -dv * nrm[1], -dv * nrm[2]};
            len = std::sqrt(vp[0] * vp[0] + vp[1] * vp[1] + vp[2] * vp[2]);
        }
        for (double &c : vp) {
            c /= len;
        }
        const std::array<double, 3> hp {vp[1] * nrm[2] - vp[2] * nrm[1], vp[2] * nrm[0] - vp[0] * nrm[2], vp[0] * nrm[1] - vp[1] * nrm[0]};
        const QPointF c(width() / 2.0, height() / 2.0);
        double shortest = 1e300;
        for (const auto &b : m_ctx.bars) {
            shortest = std::min(shortest, std::max(b.length, 1e-6));
        }
        const double radius = std::min(width(), height()) * 0.36;
        p.setPen(QColor(0x1f, 0x2a, 0x37));
        p.drawText(8, 16, QObject::tr("Esquema do nó %1 (plano de análise; barras fora do plano aparecem projetadas e tracejadas)").arg(m_ctx.nodeId));
        for (const auto &b : m_ctx.bars) {
            const double x = b.direction[0] * hp[0] + b.direction[1] * hp[1] + b.direction[2] * hp[2];
            const double y = b.direction[0] * vp[0] + b.direction[1] * vp[1] + b.direction[2] * vp[2];
            const double off = std::abs(b.direction[0] * nrm[0] + b.direction[1] * nrm[1] + b.direction[2] * nrm[2]);
            const QPointF end(c.x() + x * radius, c.y() - y * radius);
            QPen pen(b.selected ? QColor(0x0d, 0x7f, 0xb9) : QColor(0x8a, 0x94, 0xa0), b.selected ? 5.0 : 2.5);
            if (off > 0.1) {
                pen.setStyle(Qt::DashLine);
            }
            p.setPen(pen);
            p.drawLine(c, end);
            p.setPen(QColor(0x1f, 0x2a, 0x37));
            const QPointF label = c + (end - c) * 1.12;
            p.drawText(QRectF(label.x() - 60, label.y() - 18, 120, 36), Qt::AlignCenter,
                       QObject::tr("%1 %2%3").arg(b.memberId).arg(QString::fromStdString(roleName(b.role))).arg(b.selected ? QString::fromUtf8(" ●") : QString()));
        }
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(QColor(0xc8, 0x2d, 0x2d));
        p.drawEllipse(c, 8, 8);
        p.setPen(QColor(0x6b, 0x77, 0x85));
        p.drawText(8, height() - 8, QObject::tr("● = barra selecionada (azul, espessa)  |  demais barras do nó em cinza  |  ligação analisada: par selecionado"));
    }

private:
    JointAnalysisService::Context m_ctx;
};

QTableWidget *table(const QStringList &headers, QWidget *parent)
{
    auto *t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->verticalHeader()->setVisible(false);
    return t;
}

void addRow(QTableWidget *t, const QStringList &cells, bool bold = false)
{
    const int row = t->rowCount();
    t->insertRow(row);
    for (int i = 0; i < cells.size(); ++i) {
        auto *item = new QTableWidgetItem(cells[i]);
        if (bold) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
        t->setItem(row, i, item);
    }
}

} // namespace

JointAnalysisDialog::JointAnalysisDialog(const JointAnalysisService::Context &context, const std::optional<JointBehavior> &initial, bool hasModelBehavior,
                                       const Structura::App::Services::JointComponentService::Result &component, QWidget *parent)
    : QDialog(parent), m_context(context), m_component(component), m_hasModelBehavior(hasModelBehavior)
{
    setWindowTitle(tr("Análise de Ligação"));
    setSizeGripEnabled(true);
    resize(1180, 760);
    if (const auto model = JointAnalysisService::behaviorFromModel(context)) {
        m_modelBehavior = *model;
        m_hasModelBehavior = true;
    }
    buildUi();
    m_analysis = analyzeJoint(initial ? *initial : JointBehavior {}, m_context.limits, m_context.storey, m_context.kind);
    if (initial) {
        // preenche a aba de dados com o comportamento inicial
        const auto &pos = initial->positive;
        const auto &neg = initial->negative;
        auto text = [&](const std::optional<double> &v) { return v ? n(*v, 8) : QString(); };
        setFields(text(pos.resistantMoment), text(pos.theta23), text(pos.stiffness), text(neg.resistantMoment), text(neg.theta23), text(neg.stiffness));
        QString curve;
        for (const auto &pt : pos.curve) {
            curve += QStringLiteral("%1 ; %2\n").arg(pt.theta, 0, 'g', 8).arg(pt.moment, 0, 'g', 8);
        }
        for (const auto &pt : neg.curve) {
            curve += QStringLiteral("%1 ; %2\n").arg(-pt.theta, 0, 'g', 8).arg(-pt.moment, 0, 'g', 8);
        }
        m_curveText->setPlainText(curve);
        m_description->setText(QString::fromStdString(initial->description));
        const int idx = initial->origin == DataOrigin::ExperimentalCurve ? 1 : initial->origin == DataOrigin::NumericalCurve ? 2
                      : initial->origin == DataOrigin::UserStiffness ? 3 : initial->origin == DataOrigin::ModelSpring ? 4 : initial->origin == DataOrigin::ComponentMethod ? 5 : initial->origin == DataOrigin::PreliminaryModel ? 6 : 0;
        m_origin->setCurrentIndex(idx);
    }
    refresh();
}

void JointAnalysisDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    m_header = new QLabel(this);
    m_header->setTextFormat(Qt::RichText);
    m_header->setWordWrap(true);
    QString plane = tr("normal (%1; %2; %3)").arg(n(m_context.planeNormal[0], 3), n(m_context.planeNormal[1], 3), n(m_context.planeNormal[2], 3));
    plane += m_context.planeFromSelection ? tr(", definido pelas duas barras") : tr(", ADOTADO (barras colineares)");
    m_header->setText(tr("<span style='font-size:15pt;font-weight:600'>Análise de Ligação</span><br>Nó <b>%1</b> &nbsp;|&nbsp; barras <b>%2</b> e <b>%3</b> &nbsp;|&nbsp; "
                         "configuração: <b>%4</b> &nbsp;|&nbsp; plano: %5")
                          .arg(m_context.nodeId).arg(m_context.selectedMembers[0]).arg(m_context.selectedMembers[1])
                          .arg(QString::fromStdString(kindName(m_context.kind)), plane));
    root->addWidget(m_header);

    auto *split = new QSplitter(Qt::Horizontal, this);
    m_tabs = new QTabWidget(split);

    // --- aba 1: momento-rotacao
    auto *mrPage = new QWidget(m_tabs);
    auto *mrLayout = new QVBoxLayout(mrPage);
    auto *bar = new QHBoxLayout();
    m_thetaUnit = new QComboBox(mrPage);
    m_thetaUnit->addItems({QStringLiteral("rad"), QStringLiteral("mrad")});
    m_momentUnit = new QComboBox(mrPage);
    m_momentUnit->addItems({QStringLiteral("kN·m"), QStringLiteral("N·m"), QStringLiteral("kN·cm")});
    bar->addWidget(new QLabel(tr("Rotação:"), mrPage));
    bar->addWidget(m_thetaUnit);
    bar->addWidget(new QLabel(tr("Momento:"), mrPage));
    bar->addWidget(m_momentUnit);
    auto *reset = new QPushButton(tr("Ajustar escala"), mrPage);
    auto *png = new QPushButton(tr("Exportar PNG…"), mrPage);
    auto *svg = new QPushButton(tr("Exportar SVG…"), mrPage);
    bar->addStretch(1);
    bar->addWidget(reset);
    bar->addWidget(png);
    bar->addWidget(svg);
    mrLayout->addLayout(bar);
    m_mrChart = new MomentRotationChart(mrPage);
    mrLayout->addWidget(m_mrChart, 1);
    auto *hint = new QLabel(tr("Roda do mouse: zoom · arrastar: pan · duplo clique: ajustar · passe o cursor sobre a curva para ler os pontos."), mrPage);
    hint->setStyleSheet(QStringLiteral("color:#6b7785"));
    mrLayout->addWidget(hint);
    m_tabs->addTab(mrPage, tr("Momento-Rotação"));
    auto applyUnits = [this]() { setUnits(m_thetaUnit->currentText(), m_momentUnit->currentText()); };
    connect(m_thetaUnit, &QComboBox::currentTextChanged, this, applyUnits);
    connect(m_momentUnit, &QComboBox::currentTextChanged, this, applyUnits);
    connect(reset, &QPushButton::clicked, m_mrChart, &MomentRotationChart::resetView);
    connect(png, &QPushButton::clicked, this, [this]() { exportChart(false); });
    connect(svg, &QPushButton::clicked, this, [this]() { exportChart(true); });

    // --- aba 2: classificacao
    auto *clPage = new QWidget(m_tabs);
    auto *clLayout = new QVBoxLayout(clPage);
    auto *clBar = new QHBoxLayout();
    auto *clPng = new QPushButton(tr("Exportar PNG…"), clPage);
    auto *clSvg = new QPushButton(tr("Exportar SVG…"), clPage);
    clBar->addStretch(1);
    clBar->addWidget(clPng);
    clBar->addWidget(clSvg);
    clLayout->addLayout(clBar);
    m_classChart = new ClassificationChart(clPage);
    clLayout->addWidget(m_classChart, 1);
    m_tabs->addTab(clPage, tr("Classificação da Rigidez"));
    connect(clPng, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, tr("Exportar gráfico de classificação"), QStringLiteral("classificacao_ligacao.png"), tr("PNG (*.png)"));
        if (!path.isEmpty() && !m_classChart->exportPng(path)) {
            QMessageBox::warning(this, windowTitle(), tr("Não foi possível gravar o arquivo."));
        }
    });
    connect(clSvg, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, tr("Exportar gráfico de classificação"), QStringLiteral("classificacao_ligacao.svg"), tr("SVG (*.svg)"));
        if (!path.isEmpty() && !m_classChart->exportSvg(path, tr("Classificação da rigidez da ligação"))) {
            QMessageBox::warning(this, windowTitle(), tr("Não foi possível gravar o arquivo."));
        }
    });

    // --- aba: dados da ligacao
    auto *dataPage = new QWidget(m_tabs);
    auto *dataLayout = new QVBoxLayout(dataPage);
    auto *intro = new QLabel(tr("Se o nó tem uma ligação do projeto de tipologia suportada (mesas soldadas ou chapa de extremidade), use \"Calcular pelo método dos componentes\". "
                                "Caso contrário, informe os dados abaixo (ensaio, análise numérica ou outro procedimento); nenhuma curva é inventada. Valores negativos na curva (rotação e momento) formam a curva do momento negativo."),
                             dataPage);
    intro->setWordWrap(true);
    dataLayout->addWidget(intro);
    // ---- FONTE A1: ligacao cadastrada (dimensionada), opcional
    auto *connBox = new QGroupBox(tr("Fonte A1 — ligação já cadastrada no módulo de ligações (dimensionada; opcional)"), dataPage);
    auto *connLayout = new QVBoxLayout(connBox);
    m_connStatus = new QLabel(connBox);
    m_connStatus->setWordWrap(true);
    connLayout->addWidget(m_connStatus);
    auto *connRow = new QHBoxLayout();
    m_candidates = new QComboBox(connBox);
    m_candidates->setMinimumWidth(260);
    m_useCandidate = new QPushButton(tr("Usar esta ligação"), connBox);
    m_editConnections = new QPushButton(tr("Abrir o módulo de ligações (dimensionamento: etapa posterior)…"), connBox);
    connRow->addWidget(new QLabel(tr("Ligações cadastradas neste nó:"), connBox));
    connRow->addWidget(m_candidates, 1);
    connRow->addWidget(m_useCandidate);
    connLayout->addLayout(connRow);
    connLayout->addWidget(m_editConnections);
    dataLayout->addWidget(connBox);
    // ---- FONTE A2: modelo preliminar (nao exige ligacao dimensionada)
    auto *prelimBox = new QGroupBox(tr("Fonte A2 — modelo preliminar: configuração de referência, NÃO dimensionada (hipóteses declaradas)"), dataPage);
    auto *prelimLayout = new QVBoxLayout(prelimBox);
    auto *prelimIntro = new QLabel(tr("Estima a rigidez rotacional e o diagrama momento-rotação a partir dos perfis das duas barras e de uma configuração de referência adotada por regras. "
                                      "Não exige chapa, parafusos ou soldas dimensionados. O resultado é PRELIMINAR; o dimensionamento construtivo é uma etapa posterior."), prelimBox);
    prelimIntro->setWordWrap(true);
    prelimLayout->addWidget(prelimIntro);
    auto *prelimRow = new QHBoxLayout();
    m_prelimKind = new QComboBox(prelimBox);
    m_prelimKind->addItems({tr("Chapa de extremidade estendida (referência)"), tr("Mesas soldadas ao pilar (referência)")});
    m_prelimBeta = new QComboBox(prelimBox);
    m_prelimBeta->addItems({tr("Viga de um lado do pilar (β = 1)"), tr("Vigas dos dois lados, momentos iguais (β = 0)")});
    prelimRow->addWidget(new QLabel(tr("Referência:"), prelimBox));
    prelimRow->addWidget(m_prelimKind, 1);
    prelimRow->addWidget(m_prelimBeta, 1);
    prelimLayout->addLayout(prelimRow);
    auto *prelimFields = new QHBoxLayout();
    m_prelimPlate = new QLineEdit(prelimBox);
    m_prelimPlate->setPlaceholderText(tr("espessura da chapa (mm): vazio = automática"));
    m_prelimBolt = new QLineEdit(prelimBox);
    m_prelimBolt->setPlaceholderText(tr("diâmetro do parafuso (mm): vazio = 20"));
    m_prelimRows = new QLineEdit(prelimBox);
    m_prelimRows->setPlaceholderText(tr("fileiras tracionadas: vazio = 1"));
    prelimFields->addWidget(m_prelimPlate);
    prelimFields->addWidget(m_prelimBolt);
    prelimFields->addWidget(m_prelimRows);
    prelimLayout->addLayout(prelimFields);
    m_prelimCalc = new QPushButton(tr("Calcular modelo preliminar"), prelimBox);
    prelimLayout->addWidget(m_prelimCalc);
    m_prelimBand = new QLabel(prelimBox);
    m_prelimBand->setWordWrap(true);
    m_prelimBand->setTextFormat(Qt::RichText);
    prelimLayout->addWidget(m_prelimBand);
    dataLayout->addWidget(prelimBox);
    for (auto *button : {m_useCandidate, m_editConnections, m_prelimCalc}) {
        button->setEnabled(false);
    }
    connect(m_useCandidate, &QPushButton::clicked, this, [this]() {
        if (m_hooks.useCandidate && m_candidates->currentIndex() >= 0) {
            adoptComponent(m_hooks.useCandidate(m_candidates->currentIndex()));
        }
    });
    connect(m_prelimCalc, &QPushButton::clicked, this, [this]() { calculatePreliminary(); });
    connect(m_editConnections, &QPushButton::clicked, this, [this]() {
        if (m_hooks.edit) {
            adoptComponent(m_hooks.edit());
        }
    });
    auto *form = new QFormLayout();
    m_origin = new QComboBox(dataPage);
    m_origin->addItems({tr("Curva importada"), tr("Curva experimental"), tr("Curva de análise numérica"), tr("Rigidez e momento resistente informados"),
                        tr("Modelo estrutural (mola/liberação de extremidade)"), tr("Método dos componentes (ligação cadastrada, dimensionada)"), tr("Modelo preliminar (configuração de referência)")});
    m_description = new QLineEdit(dataPage);
    m_description->setPlaceholderText(tr("fonte: ensaio, relatório, arquivo, programa…"));
    form->addRow(tr("Origem dos dados:"), m_origin);
    form->addRow(tr("Descrição da fonte:"), m_description);
    dataLayout->addLayout(form);
    auto *grid = new QGridLayout();
    auto field = [&](QLineEdit *&edit, const QString &hintText) {
        edit = new QLineEdit(dataPage);
        edit->setPlaceholderText(hintText);
        return edit;
    };
    grid->addWidget(new QLabel(tr("<b>Momento positivo</b>"), dataPage), 0, 1);
    grid->addWidget(new QLabel(tr("<b>Momento negativo</b>"), dataPage), 0, 2);
    grid->addWidget(new QLabel(tr("Mj,Rd (kN·m)"), dataPage), 1, 0);
    grid->addWidget(field(m_mrdPos, tr("opcional")), 1, 1);
    grid->addWidget(field(m_mrdNeg, tr("opcional")), 1, 2);
    grid->addWidget(new QLabel(tr("θ(2/3 Mj,Rd) (rad)"), dataPage), 2, 0);
    grid->addWidget(field(m_thetaPos, tr("opcional")), 2, 1);
    grid->addWidget(field(m_thetaNeg, tr("opcional")), 2, 2);
    grid->addWidget(new QLabel(tr("Si (kN·m/rad)"), dataPage), 3, 0);
    grid->addWidget(field(m_siPos, tr("opcional")), 3, 1);
    grid->addWidget(field(m_siNeg, tr("opcional")), 3, 2);
    dataLayout->addLayout(grid);
    auto *curveBox = new QGroupBox(tr("Curva momento-rotação (uma linha por ponto: rotação ; momento)"), dataPage);
    auto *curveLayout = new QVBoxLayout(curveBox);
    auto *unitsRow = new QHBoxLayout();
    m_curveThetaUnit = new QComboBox(curveBox);
    m_curveThetaUnit->addItems({QStringLiteral("rad"), QStringLiteral("mrad")});
    m_curveMomentUnit = new QComboBox(curveBox);
    m_curveMomentUnit->addItems({QStringLiteral("kN·m"), QStringLiteral("N·m"), QStringLiteral("kN·cm")});
    unitsRow->addWidget(new QLabel(tr("Unidade da rotação:"), curveBox));
    unitsRow->addWidget(m_curveThetaUnit);
    unitsRow->addWidget(new QLabel(tr("do momento:"), curveBox));
    unitsRow->addWidget(m_curveMomentUnit);
    unitsRow->addStretch(1);
    auto *load = new QPushButton(tr("Importar arquivo…"), curveBox);
    unitsRow->addWidget(load);
    curveLayout->addLayout(unitsRow);
    m_curveText = new QPlainTextEdit(curveBox);
    m_curveText->setPlaceholderText(QStringLiteral("# theta ; M\n0.002 ; 40\n0.005 ; 90"));
    curveLayout->addWidget(m_curveText, 1);
    dataLayout->addWidget(curveBox, 1);
    auto *applyRow = new QHBoxLayout();
    m_inputError = new QLabel(dataPage);
    m_inputError->setWordWrap(true);
    m_inputError->setStyleSheet(QStringLiteral("color:#b03020"));
    auto *useModel = new QPushButton(tr("Usar dados do modelo"), dataPage);
    useModel->setEnabled(m_hasModelBehavior);
    useModel->setToolTip(m_hasModelBehavior ? tr("A viga tem mola rotacional ou liberação de extremidade no nó") : tr("A viga é contínua no nó no modelo: continuidade é hipótese de modelagem, não dado da ligação"));
    auto *useComponents = new QPushButton(tr("Calcular pelo método dos componentes"), dataPage);
    useComponents->setEnabled(m_component.supported);
    useComponents->setToolTip(m_component.supported ? tr("Rigidez inicial pelos componentes da EN 1993-1-8 e Mj,Rd pelas verificações da NBR 8800 da ligação definida no projeto")
                                                    : QString::fromStdString(m_component.message.empty() ? std::string("Defina a ligação do nó (botão Ligações) com tipologia rígida viga-pilar") : m_component.message));
    auto *apply = new QPushButton(tr("Aplicar e recalcular"), dataPage);
    apply->setDefault(true);
    applyRow->addWidget(m_inputError, 1);
    applyRow->addWidget(useComponents);
    applyRow->addWidget(useModel);
    applyRow->addWidget(apply);
    dataLayout->addLayout(applyRow);
    m_tabs->addTab(dataPage, tr("Dados da ligação"));
    connect(apply, &QPushButton::clicked, this, [this]() { applyInputs(); });
    connect(useModel, &QPushButton::clicked, this, [this]() { useModelBehavior(); });
    connect(useComponents, &QPushButton::clicked, this, [this]() {
        m_origin->setCurrentIndex(5);
        applyInputs();
    });
    connect(load, &QPushButton::clicked, this, [this]() { loadCurveFile(); });

    // --- aba: ligacoes no no
    auto *nodePage = new QWidget(m_tabs);
    auto *nodeLayout = new QVBoxLayout(nodePage);
    nodeLayout->addWidget(new SchematicWidget(m_context, nodePage), 2);
    auto *pairs = table({tr("Barra A"), tr("Barra B"), tr("Configuração"), tr("Ângulo (°)"), tr("Selecionada")}, nodePage);
    for (const auto &pair : m_context.pairs) {
        addRow(pairs, {QString::number(pair.memberA), QString::number(pair.memberB), QString::fromStdString(kindName(pair.kind)), n(pair.angleDeg, 4),
                       pair.selected ? tr("sim (esta análise)") : QString()}, pair.selected);
    }
    nodeLayout->addWidget(new QLabel(tr("Cada par de barras é uma ligação distinta; as rigidezes não são somadas nem contadas duas vezes."), nodePage));
    nodeLayout->addWidget(pairs, 1);
    m_tabs->addTab(nodePage, tr("Ligações no nó"));

    // --- aba: idealizacao na analise global
    auto *idealPage = new QWidget(m_tabs);
    auto *idealLayout = new QVBoxLayout(idealPage);
    auto *idealIntro = new QLabel(tr("A classificação acima é o RESULTADO da análise mecânica da ligação. Aqui você decide, depois, como a ligação entra na análise global da estrutura: "
                                     "rotulada (articulação), semirrígida (mola rotacional com a rigidez Si calculada) ou rígida (contínua). Essa escolha é uma decisão de modelagem: "
                                     "ela não altera nem apaga a curva, a rigidez ou a classificação calculadas. Se a idealização diferir do comportamento calculado, o programa avisa."), idealPage);
    idealIntro->setWordWrap(true);
    idealLayout->addWidget(idealIntro);
    m_idealInfo = new QLabel(idealPage);
    m_idealInfo->setWordWrap(true);
    m_idealInfo->setTextFormat(Qt::RichText);
    idealLayout->addWidget(m_idealInfo);
    auto *idealRow = new QHBoxLayout();
    m_idealKind = new QComboBox(idealPage);
    m_idealKind->addItems({tr("Rotulada (articulação)"), tr("Semirrígida linear (mola rotacional Si)"), tr("Rígida (contínua)"), tr("Semirrígida não linear (rótula bilinear)")});
    m_applyIdeal = new QPushButton(tr("Aplicar ao modelo estrutural"), idealPage);
    m_applyIdeal->setEnabled(false);
    idealRow->addWidget(new QLabel(tr("Idealização na análise global:"), idealPage));
    idealRow->addWidget(m_idealKind, 1);
    idealRow->addWidget(m_applyIdeal);
    idealLayout->addLayout(idealRow);
    m_idealWarn = new QLabel(idealPage);
    m_idealWarn->setWordWrap(true);
    m_idealWarn->setStyleSheet(QStringLiteral("color:#b03020"));
    idealLayout->addWidget(m_idealWarn);
    m_idealStatus = new QLabel(idealPage);
    m_idealStatus->setWordWrap(true);
    idealLayout->addWidget(m_idealStatus);
    idealLayout->addStretch(1);
    m_tabs->addTab(idealPage, tr("Nível B — Idealização (análise global)"));
    connect(m_idealKind, &QComboBox::currentIndexChanged, this, [this]() { updateSideUi(); });
    connect(m_applyIdeal, &QPushButton::clicked, this, [this]() { applyIdealization(m_idealKind->currentIndex()); });

    // --- aba: propriedades
    auto *propPage = new QWidget(m_tabs);
    auto *propLayout = new QVBoxLayout(propPage);
    auto *props = table({tr("Barra"), tr("Papel"), tr("Sel."), tr("L (m)"), tr("I no plano (cm⁴)"), tr("E (MPa)"), tr("E·I/L (kN·m/rad)"), tr("Extremidade no nó")}, propPage);
    for (std::size_t i = 0; i < m_context.bars.size(); ++i) {
        const auto &b = m_context.bars[i];
        QString end = tr("contínua");
        if (b.springKnm) {
            end = tr("mola %1 kN·m/rad").arg(n(*b.springKnm));
        } else if (b.rotationalRelease) {
            end = tr("rotação liberada");
        }
        addRow(props, {QString::number(b.memberId), QString::fromStdString(roleName(b.role)), b.selected ? tr("sim") : QString(), n(b.length, 5), n(b.inertia * 1e8, 6),
                       n(b.youngModulus / 1000.0, 6), b.length > 0 ? n(b.youngModulus * b.inertia / b.length, 6) : QString(), end},
               static_cast<int>(i) == m_context.referenceBar);
    }
    propLayout->addWidget(props);
    auto *storeyLabel = new QLabel(propPage);
    storeyLabel->setWordWrap(true);
    QString storeyText = tr("<b>Pavimento (plano de análise)</b><br>Vigas: ");
    for (const auto &b : m_context.storeyBeams) {
        storeyText += tr("%1 (I/L = %2 cm⁴/m) ").arg(b.memberId).arg(n(b.inertia * 1e8 / b.length, 5));
    }
    storeyText += tr("<br>Pilares: ");
    for (const auto &b : m_context.storeyColumns) {
        storeyText += tr("%1 (I/L = %2 cm⁴/m) ").arg(b.memberId).arg(n(b.inertia * 1e8 / b.length, 5));
    }
    storeyLabel->setText(storeyText);
    propLayout->addWidget(storeyLabel);
    propLayout->addStretch(1);
    m_tabs->addTab(propPage, tr("Propriedades"));

    // --- aba: memoria
    auto *memPage = new QWidget(m_tabs);
    auto *memLayout = new QVBoxLayout(memPage);
    auto *memBar = new QHBoxLayout();
    auto *copy = new QPushButton(tr("Copiar"), memPage);
    auto *save = new QPushButton(tr("Salvar memória…"), memPage);
    memBar->addStretch(1);
    memBar->addWidget(copy);
    memBar->addWidget(save);
    memLayout->addLayout(memBar);
    m_memory = new QPlainTextEdit(memPage);
    m_memory->setReadOnly(true);
    QFont mono = m_memory->font();
    mono.setFamily(QStringLiteral("Consolas"));
    m_memory->setFont(mono);
    memLayout->addWidget(m_memory, 1);
    m_tabs->addTab(memPage, tr("Memória de Cálculo"));
    connect(copy, &QPushButton::clicked, this, [this]() { copyMemory(); });
    connect(save, &QPushButton::clicked, this, [this]() { saveMemory(); });

    // --- aba: referencias
    auto *refs = new QPlainTextEdit(m_tabs);
    refs->setReadOnly(true);
    refs->setPlainText(QString::fromStdString(referencesText()));
    m_tabs->addTab(refs, tr("Referências Normativas"));

    // painel lateral
    m_panel = new QLabel(split);
    m_panel->setTextFormat(Qt::RichText);
    m_panel->setWordWrap(true);
    m_panel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_panel->setMinimumWidth(270);
    m_panel->setMaximumWidth(340);
    m_panel->setMargin(10);
    m_panel->setStyleSheet(QStringLiteral("background:#f2f5fa;border:1px solid #d7dee8;border-radius:6px"));
    split->addWidget(m_tabs);
    split->addWidget(m_panel);
    split->setStretchFactor(0, 1);
    root->addWidget(split, 1);
}

void JointAnalysisDialog::setOrigin(DataOrigin origin)
{
    const int idx = origin == DataOrigin::ExperimentalCurve ? 1 : origin == DataOrigin::NumericalCurve ? 2 : origin == DataOrigin::UserStiffness ? 3
                  : origin == DataOrigin::ModelSpring ? 4 : origin == DataOrigin::ComponentMethod ? 5 : origin == DataOrigin::PreliminaryModel ? 6 : 0;
    m_origin->setCurrentIndex(idx);
}

void JointAnalysisDialog::setCurveText(const QString &text) { m_curveText->setPlainText(text); }

void JointAnalysisDialog::setFields(const QString &a, const QString &b, const QString &c, const QString &d, const QString &e, const QString &f)
{
    m_mrdPos->setText(a);
    m_thetaPos->setText(b);
    m_siPos->setText(c);
    m_mrdNeg->setText(d);
    m_thetaNeg->setText(e);
    m_siNeg->setText(f);
}

void JointAnalysisDialog::setUnits(const QString &thetaUnit, const QString &momentUnit)
{
    JointChartUnits u;
    if (thetaUnit == QLatin1String("mrad")) {
        u.thetaFactor = 1000.0;
        u.thetaLabel = QStringLiteral("mrad");
    }
    if (momentUnit == QStringLiteral("N·m")) {
        u.momentFactor = 1000.0;
        u.momentLabel = momentUnit;
    } else if (momentUnit == QStringLiteral("kN·cm")) {
        u.momentFactor = 100.0;
        u.momentLabel = momentUnit;
    }
    m_mrChart->setUnits(u);
    if (m_thetaUnit->currentText() != thetaUnit) {
        const QSignalBlocker block(m_thetaUnit);
        m_thetaUnit->setCurrentText(thetaUnit);
    }
    if (m_momentUnit->currentText() != momentUnit) {
        const QSignalBlocker block(m_momentUnit);
        m_momentUnit->setCurrentText(momentUnit);
    }
}

bool JointAnalysisDialog::applyInputs()
{
    m_inputError->clear();
    JointBehavior b;
    const int idx = m_origin->currentIndex();
    if (idx == 5 || idx == 6) {
        if (idx == 5 && m_component.preliminary) {
            m_inputError->setText(tr("O resultado atual vem do modelo preliminar: use \"Calcular modelo preliminar\" ou escolha uma ligação cadastrada."));
            return false;
        }
        if (idx == 6 && !m_component.preliminary) {
            m_inputError->setText(tr("Ainda não há modelo preliminar calculado: use \"Calcular modelo preliminar\"."));
            return false;
        }
        if (!m_component.supported) {
            m_inputError->setText(QString::fromStdString(m_component.message.empty() ? std::string("Não há modelo de componentes para esta ligação.") : m_component.message));
            return false;
        }
        b = m_component.behavior;
    } else if (idx == 4) {
        if (!m_hasModelBehavior) {
            m_inputError->setText(tr("O modelo não define mola nem liberação rotacional nesta extremidade."));
            return false;
        }
        b = m_modelBehavior;
    } else {
        b.origin = idx == 1 ? DataOrigin::ExperimentalCurve : idx == 2 ? DataOrigin::NumericalCurve : idx == 3 ? DataOrigin::UserStiffness : DataOrigin::ImportedCurve;
        b.description = m_description->text().trimmed().toStdString();
        bool ok = true;
        auto read = [&](QLineEdit *edit, const QString &name) -> std::optional<double> {
            QString t = edit->text().trimmed();
            t.replace(QLatin1Char(','), QLatin1Char('.'));
            if (t.isEmpty()) {
                return std::nullopt;
            }
            bool parsed = false;
            const double v = t.toDouble(&parsed);
            if (!parsed || !std::isfinite(v)) {
                m_inputError->setText(tr("Valor inválido em %1.").arg(name));
                ok = false;
                return std::nullopt;
            }
            return v;
        };
        b.positive.resistantMoment = read(m_mrdPos, tr("Mj,Rd (positivo)"));
        b.positive.theta23 = read(m_thetaPos, tr("θ(2/3 Mj,Rd) (positivo)"));
        b.positive.stiffness = read(m_siPos, tr("Si (positivo)"));
        b.negative.resistantMoment = read(m_mrdNeg, tr("Mj,Rd (negativo)"));
        b.negative.theta23 = read(m_thetaNeg, tr("θ(2/3 Mj,Rd) (negativo)"));
        b.negative.stiffness = read(m_siNeg, tr("Si (negativo)"));
        if (!ok) {
            return false;
        }
        for (auto *dir : {&b.positive, &b.negative}) {
            if (dir->resistantMoment) {
                dir->resistantMoment = std::abs(*dir->resistantMoment);
            }
            if (dir->theta23) {
                dir->theta23 = std::abs(*dir->theta23);
            }
            if (dir->stiffness) {
                dir->stiffness = std::abs(*dir->stiffness);
            }
        }
        const double tf = m_curveThetaUnit->currentText() == QLatin1String("mrad") ? 1e-3 : 1.0;
        const double mf = m_curveMomentUnit->currentText() == QStringLiteral("N·m") ? 1e-3 : m_curveMomentUnit->currentText() == QStringLiteral("kN·cm") ? 0.01 : 1.0;
        const std::string error = parseCurveText(m_curveText->toPlainText().toStdString(), tf, mf, b.positive.curve, b.negative.curve);
        if (!error.empty()) {
            m_inputError->setText(QString::fromStdString(error));
            return false;
        }
        if (b.positive.curve.empty() && b.negative.curve.empty() && idx != 3 && !b.positive.hasData() && !b.negative.hasData()) {
            b.origin = DataOrigin::None;
        }
    }
    m_analysis = analyzeJoint(b, m_context.limits, m_context.storey, m_context.kind);
    refresh();
    emit behaviorChanged(b);
    return true;
}

void JointAnalysisDialog::useModelBehavior()
{
    m_origin->setCurrentIndex(4);
    applyInputs();
}

void JointAnalysisDialog::loadCurveFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Importar curva momento-rotação"), QString(), tr("Texto/CSV (*.txt *.csv *.dat);;Todos (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, windowTitle(), tr("Não foi possível ler o arquivo."));
        return;
    }
    m_curveText->setPlainText(QString::fromUtf8(file.readAll()));
    m_description->setText(QFileInfo(path).fileName());
}

QString JointAnalysisDialog::finalClassificationText() const
{
    const DirectionAnalysis *d = m_analysis.primary();
    if (!d) {
        return tr("SEM DADOS");
    }
    if (!d->classification.applicable) {
        return tr("NÃO CLASSIFICADA");
    }
    QString text = QString::fromStdString(className(d->classification.final));
    if (d->classification.storeyPending) {
        text += tr(" (rígida pendente de Kv/Kp)");
    }
    return text;
}

MemoryContext JointAnalysisDialog::memoryContext() const
{
    MemoryContext c;
    c.nodeId = m_context.nodeId;
    c.memberA = m_context.selectedMembers[0];
    c.memberB = m_context.selectedMembers[1];
    c.planeNote = QStringLiteral("normal (%1; %2; %3)%4").arg(m_context.planeNormal[0], 0, 'f', 3).arg(m_context.planeNormal[1], 0, 'f', 3).arg(m_context.planeNormal[2], 0, 'f', 3)
                      .arg(m_context.planeFromSelection ? QStringLiteral(", definido pelas duas barras") : QStringLiteral(", adotado (barras colineares)")).toStdString();
    c.youngModulus = m_context.youngModulus;
    c.beamInertia = m_context.beamInertia;
    c.beamLength = m_context.beamLength;
    if (m_context.referenceBar >= 0) {
        c.referenceMember = m_context.bars[static_cast<std::size_t>(m_context.referenceBar)].memberId;
    }
    c.notes = m_context.notes;
    if ((m_analysis.behavior.origin == DataOrigin::ComponentMethod || m_analysis.behavior.origin == DataOrigin::PreliminaryModel) && m_component.supported) {
        c.componentLines = m_component.memory;
        if (!m_component.message.empty()) {
            c.notes.push_back(m_component.message);
        }
    }
    if (m_context.kind != JointKind::BeamColumn) {
        c.notes.push_back("Configuração " + kindName(m_context.kind) + ": o critério de 6.1.2 (viga-pilar) não é aplicado automaticamente.");
    }
    return c;
}

QString JointAnalysisDialog::memoryText() const { return m_memory ? m_memory->toPlainText() : QString(); }

QString JointAnalysisDialog::panelHtml() const
{
    QString h;
    const DirectionAnalysis *main = m_analysis.primary();
    h += tr("<div style='font-size:9pt;color:#6b7785'>CLASSIFICAÇÃO (nível B)</div>");
    if (main && main->classification.computed) {
        const QString color = main->classification.applicable ? classColor(main->classification.final) : classColor(StiffnessClass::NotClassified);
        h += QStringLiteral("<div style='font-size:17pt;font-weight:700;color:%1'>%2</div>").arg(color, finalClassificationText());
    } else {
        h += tr("<div style='font-size:14pt;font-weight:700;color:#555'>%1</div>").arg(finalClassificationText());
    }
    if (m_analysis.behavior.origin == DataOrigin::PreliminaryModel) {
        h += tr("<p style='background:#fff3cd;border:1px solid #e0b84d;padding:4px'><b>RESULTADO PRELIMINAR</b><br>Configuração de referência, <u>não dimensionada</u>. A classificação vale como estimativa para decidir a modelagem.</p>");
    }
    h += tr("<p><b>NÍVEL A — Comportamento mecânico</b></p>");
    auto line = [&](const QString &label, const QString &value) { h += QStringLiteral("<tr><td>%1</td><td align='right'><b>%2</b></td></tr>").arg(label, value); };
    auto direction = [&](const QString &title, const DirectionAnalysis &d) {
        h += QStringLiteral("<p><b>%1</b></p>").arg(title);
        if (!d.hasData) {
            h += tr("<i>sem dados</i>");
            return;
        }
        if (!d.stiffness.valid) {
            h += tr("<i>Si não determinada</i>");
            for (const auto &m : d.stiffness.messages) {
                h += QStringLiteral("<br><span style='color:#b03020'>%1</span>").arg(QString::fromStdString(m).toHtmlEscaped());
            }
            return;
        }
        h += QStringLiteral("<table width='100%'>");
        line(tr("Si"), tr("%1 kN·m/rad").arg(n(d.stiffness.stiffness)));
        line(tr("η = Si/(E·Iv/Lv)"), n(d.classification.eta));
        if (d.stiffness.momentRd > 0.0) {
            line(tr("Mj,Rd"), tr("%1 kN·m").arg(n(d.stiffness.momentRd)));
            line(tr("θ(2/3 Mj,Rd)"), tr("%1 rad").arg(n(d.stiffness.theta23, 5)));
        }
        if (d.classification.applicable) {
            line(tr("Classe (rigidez)"), QString::fromStdString(className(d.classification.byStiffness)));
            line(tr("Classe final"), QString::fromStdString(className(d.classification.final)));
        }
        h += QStringLiteral("</table>");
    };
    direction(tr("Momento positivo"), m_analysis.positive);
    if (m_analysis.negative.hasData) {
        direction(tr("Momento negativo"), m_analysis.negative);
    }
    h += tr("<p><b>NÍVEL B — Classificação (NBR 8800:2024, 6.1.2)</b><br>Limites (E·Iv/Lv = %1 kN·m/rad)</p><table width='100%'>").arg(n(m_context.limits.referenceStiffness));
    line(tr("Inferior 0,5·E·Iv/Lv"), tr("%1").arg(n(m_context.limits.sMin)));
    line(tr("Superior 25·E·Iv/Lv"), tr("%1").arg(n(m_context.limits.sMax)));
    h += QStringLiteral("</table>");
    h += tr("<p><b>Kv/Kp ≥ 0,1</b></p>");
    if (m_context.storey.status == StoreyStatus::Ok) {
        h += QStringLiteral("<table width='100%'>");
        line(tr("Kv (média Iv/Lv)"), tr("%1 m³").arg(n(m_context.storey.kv, 4)));
        line(tr("Kp (média Ip/Lp)"), tr("%1 m³").arg(n(m_context.storey.kp, 4)));
        line(tr("Kv/Kp"), n(m_context.storey.ratio, 4));
        h += QStringLiteral("</table>");
    }
    const bool need = main && main->classification.storeyConditionNeeded;
    QString status;
    if (m_context.storey.status == StoreyStatus::Pending) {
        status = tr("PENDENTE: %1").arg(QString::fromStdString(m_context.storey.reason));
    } else if (m_context.storey.ratio >= Limits::kMinStoreyRatio) {
        status = need ? tr("ATENDIDA (necessária para classificar como rígida)") : tr("ATENDIDA (não decisiva para esta ligação)");
    } else {
        status = need ? tr("NÃO ATENDIDA: a ligação não pode ser rígida") : tr("NÃO ATENDIDA (não decisiva: Si abaixo do limite de rigidez)");
    }
    h += QStringLiteral("<p>%1</p>").arg(status);
    h += tr("<p style='color:#6b7785'>Origem: %1<br><b>%2</b></p>").arg(QString::fromStdString(originName(m_analysis.behavior.origin)), QString::fromStdString(originNature(m_analysis.behavior.origin)).toHtmlEscaped());
    if ((m_analysis.behavior.origin == DataOrigin::ComponentMethod || m_analysis.behavior.origin == DataOrigin::PreliminaryModel) && m_component.supported && m_component.momentResistanceKnm > 0.0) {
        h += tr("<p style='color:#444'>Mj,Rd da curva (método dos componentes, EN): %1 kN·m.<br>Perda de rigidez a partir de %2 kN·m (2/3 Mj,Rd); em Mj,Rd: secante %3 kN·m/rad, tangente %4 kN·m/rad.</p>")
                 .arg(n(m_component.momentResistanceEnKnm, 4), n(m_component.metrics.onsetOfNonlinearity, 4), n(m_component.metrics.secantAtResistance, 4), n(m_component.metrics.tangentAtResistance, 4));
    }
    h += tr("<p><b>NÍVEL C — Dimensionamento construtivo (etapa posterior)</b></p>");
    if (m_component.supported && !m_component.preliminary && m_component.nbrCheckAvailable) {
        h += m_component.nbrBelowCurve
                 ? tr("<p style='color:#b03020'>Verificação da NBR 8800 da ligação cadastrada: Mj,Rd = %1 kN·m, <b>menor</b> que o da curva (%2 kN·m). A ligação cadastrada não sustenta o momento da curva: redimensione-a.</p>")
                       .arg(n(m_component.momentResistanceNbrKnm, 4), n(m_component.momentResistanceEnKnm, 4))
                 : tr("<p>Verificação da NBR 8800 da ligação cadastrada: Mj,Rd = %1 kN·m (≥ Mj,Rd da curva). Verificações completas de parafusos, soldas e chapas: módulo de ligações.</p>").arg(n(m_component.momentResistanceNbrKnm, 4));
    } else {
        h += tr("<p style='color:#6b7785'>Não realizado. O comportamento acima não depende de uma ligação dimensionada; depois de decidir a modelagem, dimensione e verifique a ligação (parafusos, soldas, chapas, enrijecedores) no módulo de ligações.</p>");
    }
    if (!m_component.supported && !m_component.message.empty()) {
        h += QStringLiteral("<p style='color:#b03020'><b>%1</b><br>%2</p>").arg(tr("Método dos componentes indisponível:"), QString::fromStdString(m_component.message).toHtmlEscaped());
    }
    if (main && !main->classification.justification.empty()) {
        h += QStringLiteral("<p style='color:#444'>%1</p>").arg(QString::fromStdString(main->classification.justification).toHtmlEscaped());
    }
    return h;
}

void JointAnalysisDialog::refresh()
{
    m_mrChart->setHint(m_component.supported || m_component.message.empty() ? QString() : QString::fromStdString(m_component.message));
    m_mrChart->setAnalysis(m_analysis);
    m_classChart->setAnalysis(m_analysis);
    m_panel->setText(panelHtml());
    m_memory->setPlainText(QString::fromStdString(buildMemory(m_analysis, memoryContext())));
    updateSideUi();
}

QString JointAnalysisDialog::connectionStatusText() const { return m_connStatus ? m_connStatus->text() : QString(); }

QString JointAnalysisDialog::idealizationStatusText() const { return m_idealStatus ? m_idealStatus->text() : QString(); }

QStringList JointAnalysisDialog::candidateItems() const
{
    QStringList items;
    for (int i = 0; m_candidates && i < m_candidates->count(); ++i) {
        items << m_candidates->itemText(i);
    }
    return items;
}

void JointAnalysisDialog::setHooks(const Hooks &hooks)
{
    m_hooks = hooks;
    m_prelimCalc->setEnabled(static_cast<bool>(hooks.preliminary));
    m_editConnections->setEnabled(static_cast<bool>(hooks.edit));
    m_applyIdeal->setEnabled(static_cast<bool>(hooks.applyIdealization));
    reloadCandidates();
    // sem ligacao dimensionada utilizavel, o comportamento e estimado automaticamente pelo modelo preliminar (nivel A)
    if (hooks.preliminary && !m_component.supported && m_analysis.primary() == nullptr) {
        calculatePreliminary();
    }
    updatePreliminaryBand();
    updateSideUi();
}

void JointAnalysisDialog::reloadCandidates()
{
    m_candidates->clear();
    if (m_hooks.candidates) {
        m_candidates->addItems(m_hooks.candidates());
    }
    m_useCandidate->setEnabled(m_hooks.useCandidate && m_candidates->count() > 0);
}

void JointAnalysisDialog::setComponentResult(const ComponentResult &result) { adoptComponent(result); }

bool JointAnalysisDialog::refreshFromModel()
{
    if (!m_hooks.reevaluate || m_reevaluating) {
        return false;
    }
    m_reevaluating = true;
    bool changed = false;
    const ComponentResult fresh = m_hooks.reevaluate();
    if (m_component.preliminary && !fresh.supported) {
        m_reevaluating = false;   // o modelo preliminar escolhido continua valendo enquanto nao houver ligacao cadastrada utilizavel
        return false;
    }
    if (fresh.fingerprint != m_component.fingerprint || fresh.supported != m_component.supported) {
        const bool usingComponents = m_analysis.behavior.origin == DataOrigin::ComponentMethod || m_analysis.behavior.origin == DataOrigin::PreliminaryModel || !m_component.supported;
        if (usingComponents || fresh.supported) {
            adoptComponent(fresh);
            changed = true;
            if (fresh.supported) {
                m_idealStatus->setText(tr("A ligação foi alterada: curva, rigidez e classificação foram recalculadas. Reaplique a idealização para atualizar o modelo estrutural."));
            }
        }
    }
    m_reevaluating = false;
    return changed;
}

void JointAnalysisDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    // o usuario pode ter alterado a ligacao no editor de ligacoes: resultados dependentes sao recalculados (ou invalidados) ao voltar a esta janela
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        refreshFromModel();
    }
}

void JointAnalysisDialog::adoptComponent(const ComponentResult &result)
{
    m_component = result;
    reloadCandidates();
    if (result.supported) {
        m_origin->setCurrentIndex(result.preliminary ? 6 : 5);
        applyInputs();   // a classificacao e calculada automaticamente a partir do modelo mecanico
    } else {
        refresh();
    }
}

QString JointAnalysisDialog::idealizationWarning(int kind) const
{
    const DirectionAnalysis *d = m_analysis.primary();
    if (!d || !d->classification.applicable || !d->classification.computed) {
        return {};
    }
    const StiffnessClass chosen = kind == 0 ? StiffnessClass::Pinned : (kind == 1 || kind == 3) ? StiffnessClass::SemiRigid : StiffnessClass::Rigid;
    if (d->classification.final == chosen) {
        return {};
    }
    const QString names[4] = {tr("rotulada"), tr("semirrígida (mola linear)"), tr("rígida"), tr("semirrígida (rótula bilinear)")};
    return tr("Atenção: o comportamento calculado é %1 (Si = %2 kN·m/rad, η = %3), mas a idealização escolhida é %4. A análise global não representará a rigidez estimada da ligação.")
        .arg(QString::fromStdString(className(d->classification.final)).toLower(), n(d->stiffness.stiffness), n(d->classification.eta, 4), names[std::clamp(kind, 0, 3)]);
}

bool JointAnalysisDialog::applyIdealization(int kind, bool confirm)
{
    if (!m_hooks.applyIdealization) {
        m_idealStatus->setText(tr("Sem ligação com o modelo estrutural: a idealização não pôde ser aplicada."));
        return false;
    }
    const DirectionAnalysis *d = m_analysis.primary();
    double si = 0.0, yield = 0.0, hardening = 0.0;
    if (kind == 1) {
        if (!d || !d->stiffness.valid) {
            m_idealStatus->setText(tr("Si não determinada: sem a rigidez não há como idealizar como semirrígida."));
            return false;
        }
        si = d->stiffness.stiffness;
    } else if (kind == 3) {
        if (!d || !bilinearEquivalent(*d, si, yield, hardening)) {
            m_idealStatus->setText(tr("A rótula bilinear exige Si, Mj,Rd e uma curva que alcance Mj,Rd: com os dados atuais não pode ser derivada do comportamento calculado."));
            return false;
        }
    }
    const QString warning = idealizationWarning(kind);
    if (!warning.isEmpty() && confirm) {
        if (QMessageBox::warning(this, windowTitle(), warning + QStringLiteral("\n\n") + tr("Aplicar mesmo assim?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            return false;
        }
    }
    const QString state = m_hooks.applyIdealization(kind, si, yield, hardening);
    m_idealStatus->setText(state + (warning.isEmpty() ? QString() : QStringLiteral("\n") + warning));
    return true;
}

void JointAnalysisDialog::updateSideUi()
{
    if (!m_connStatus) {
        return;
    }
    if (m_component.supported && m_component.preliminary) {
        m_connStatus->setStyleSheet(QStringLiteral("color:#8a6d00"));
        m_connStatus->setText(tr("Nenhuma ligação dimensionada é necessária: o comportamento foi estimado por um MODELO PRELIMINAR (%1; configuração de referência, não dimensionada). "
                                 "As hipóteses e o domínio de validade estão na aba Memória de Cálculo.").arg(QString::fromStdString(m_component.family)));
    } else if (m_component.supported) {
        m_connStatus->setStyleSheet(QString());
        m_connStatus->setText(tr("Ligação reconhecida no modelo: %1. O comportamento mecânico e a classificação são calculados automaticamente; não é necessário informar o tipo de comportamento.")
                                  .arg(QString::fromStdString(m_component.family)));
    } else {
        m_connStatus->setStyleSheet(QStringLiteral("color:#b03020"));
        m_connStatus->setText(m_component.message.empty()
                                  ? tr("Sem ligação construtiva definida: não há dados suficientes para determinar a curva real. Escolha uma ligação do nó, crie uma nova configuração, informe os dados ou importe uma curva.")
                                  : QStringLiteral("%1 %2").arg(tr("Sem dados suficientes para determinar a curva real."), QString::fromStdString(m_component.message)));
    }
    const DirectionAnalysis *d = m_analysis.primary();
    QString info;
    if (d && d->classification.computed) {
        info = d->classification.applicable
                   ? tr("Comportamento calculado: <b>%1</b> (Si = %2 kN·m/rad, η = %3).").arg(QString::fromStdString(className(d->classification.final)), n(d->stiffness.stiffness), n(d->classification.eta, 4))
                   : tr("Comportamento calculado: Si = %1 kN·m/rad (configuração %2: sem classificação normativa automática).").arg(n(d->stiffness.stiffness), QString::fromStdString(kindName(m_analysis.kind)));
    } else {
        info = tr("Ainda não há comportamento calculado: defina a ligação ou informe os dados antes de escolher a idealização.");
    }
    if (d && m_analysis.behavior.origin == DataOrigin::PreliminaryModel) {
        info += tr("<br><b>Atenção:</b> o comportamento vem de um modelo preliminar (configuração de referência não dimensionada). Adote a idealização como decisão de modelagem e dimensione a ligação depois, de modo que ela realize a rigidez adotada.");
    }
    if (d && m_component.supported && m_component.nbrBelowCurve) {
        info += tr("<br><b>Atenção:</b> a ligação cadastrada não sustenta o Mj,Rd da curva (verificação da NBR menor): a rigidez e a resistência adotadas na análise global podem não ser realizáveis.");
    }
    if (m_hooks.currentIdealization) {
        info += tr("<br>Modelagem atual da viga na análise global: %1").arg(m_hooks.currentIdealization().toHtmlEscaped());
    }
    m_idealInfo->setText(info);
    m_idealWarn->setText(idealizationWarning(m_idealKind->currentIndex()));
}

Structura::App::Services::JointComponentService::PreliminarySettings JointAnalysisDialog::preliminarySettings(int kind) const
{
    Structura::App::Services::JointComponentService::PreliminarySettings s;
    s.kind = kind;
    auto number = [](const QLineEdit *edit) {
        QString t = edit->text().trimmed();
        t.replace(QLatin1Char(','), QLatin1Char('.'));
        bool ok = false;
        const double v = t.toDouble(&ok);
        return ok && v > 0.0 ? v : 0.0;
    };
    s.plateThickness = number(m_prelimPlate);
    s.boltDiameter = number(m_prelimBolt);
    s.tensionRows = static_cast<int>(number(m_prelimRows));
    s.beta = m_prelimBeta->currentIndex() == 1 ? 0.0 : 1.0;
    return s;
}

bool JointAnalysisDialog::calculatePreliminary()
{
    if (!m_hooks.preliminary) {
        return false;
    }
    // indice 0 do combo = chapa de extremidade (kind 1); indice 1 = mesas soldadas (kind 0)
    const int kind = m_prelimKind->currentIndex() == 1 ? 0 : 1;
    adoptComponent(m_hooks.preliminary(preliminarySettings(kind)));
    updatePreliminaryBand();
    return m_component.supported;
}

QString JointAnalysisDialog::preliminaryBandText() const { return m_prelimBand ? m_prelimBand->text() : QString(); }

void JointAnalysisDialog::updatePreliminaryBand()
{
    if (!m_prelimBand) {
        return;
    }
    if (!m_hooks.preliminary) {
        m_prelimBand->clear();
        return;
    }
    // faixa preliminar: as duas referencias lado a lado, para apoiar a decisao de modelagem
    QStringList parts;
    for (const int kind : {1, 0}) {
        const auto r = m_hooks.preliminary(preliminarySettings(kind));
        const QString name = kind == 1 ? tr("chapa de extremidade") : tr("mesas soldadas");
        if (!r.supported || !r.stiffness.ok) {
            parts << tr("%1: indisponível").arg(name);
            continue;
        }
        const double si = r.stiffness.initialStiffnessKnm();
        QString text = tr("%1: Si = %2 kN·m/rad").arg(name, n(si));
        if (m_context.limits.referenceStiffness > 0.0) {
            const auto c = classify(si, m_context.limits, m_context.storey, m_context.kind);
            text += tr(" (η = %1%2)").arg(n(c.eta, 3), c.applicable ? QStringLiteral(" → ") + QString::fromStdString(className(c.final)) : QString());
        }
        parts << text;
    }
    m_prelimBand->setText(tr("<b>Faixa preliminar de referência (não dimensionada):</b> %1").arg(parts.join(QStringLiteral(" &nbsp;|&nbsp; "))));
}

void JointAnalysisDialog::copyMemory()
{
    QApplication::clipboard()->setText(m_memory->toPlainText());
}

bool JointAnalysisDialog::exportMemory(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << m_memory->toPlainText();
    return true;
}

void JointAnalysisDialog::saveMemory()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Salvar memória de cálculo"), QStringLiteral("memoria_ligacao.txt"), tr("Texto (*.txt)"));
    if (!path.isEmpty() && !exportMemory(path)) {
        QMessageBox::warning(this, windowTitle(), tr("Não foi possível gravar o arquivo."));
    }
}

void JointAnalysisDialog::exportChart(bool svg)
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar gráfico momento-rotação"), svg ? QStringLiteral("momento_rotacao.svg") : QStringLiteral("momento_rotacao.png"),
                                                      svg ? tr("SVG (*.svg)") : tr("PNG (*.png)"));
    if (path.isEmpty()) {
        return;
    }
    const bool ok = svg ? m_mrChart->exportSvg(path, tr("Diagrama momento-rotação")) : m_mrChart->exportPng(path);
    if (!ok) {
        QMessageBox::warning(this, windowTitle(), tr("Não foi possível gravar o arquivo."));
    }
}

} // namespace Structura::UI
