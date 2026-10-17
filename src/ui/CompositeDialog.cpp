#include "CompositeDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace Structura::UI {

namespace C = Structura::Design::Composite;

CompositeDialog::Field CompositeDialog::addField(QFormLayout *form, QVector<Field> &list, const QString &key, const QString &label, double value, double min, double max,
                                                 int decimals, double step, const QString &suffix, const QString &tip)
{
    auto *box = new QDoubleSpinBox;
    box->setRange(min, max);
    box->setDecimals(decimals);
    box->setSingleStep(step);
    box->setValue(value);
    if (!suffix.isEmpty()) {
        box->setSuffix(suffix);
    }
    if (!tip.isEmpty()) {
        box->setToolTip(tip);
    }
    form->addRow(label, box);
    Field f {key, box};
    list.append(f);
    return f;
}

CompositeDialog::CompositeDialog(const QVector<CompositeCandidate> &beams, int initialMember, const QMap<QString, double> &deckSaved, QWidget *parent)
    : QDialog(parent)
    , m_beams(beams)
    , m_deckSaved(deckSaved)
{
    setWindowTitle(tr("Vigas e lajes mistas — NBR 8800:2024, Anexos L e N"));
    resize(1280, 820);
    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget;
    root->addWidget(tabs);
    buildBeamTab(tabs);
    buildDeckTab(tabs);
    int start = 0;
    for (int i = 0; i < m_beams.size(); ++i) {
        if (m_beams[i].memberId == initialMember) {
            start = i;
        }
    }
    if (m_member->count() > 0) {
        m_member->setCurrentIndex(start);
    }
    loadBeam(start);
}

namespace {

QTableWidget *makeChecksTable()
{
    auto *table = new QTableWidget(0, 5);
    table->setHorizontalHeaderLabels({QObject::tr("Verificação"), QObject::tr("Solicitação"), QObject::tr("Resistência"), QObject::tr("Razão"), QObject::tr("Item")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(24);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    return table;
}

} // namespace

void CompositeDialog::showChecks(QTableWidget *table, const std::vector<Structura::Design::Connections::Check> &checks)
{
    table->setRowCount(static_cast<int>(checks.size()));
    for (std::size_t i = 0; i < checks.size(); ++i) {
        const auto &c = checks[i];
        auto cell = [&](int col, const QString &text, bool right) {
            auto *item = new QTableWidgetItem(text);
            item->setTextAlignment((right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
            table->setItem(static_cast<int>(i), col, item);
            return item;
        };
        cell(0, QString::fromStdString(c.name), false);
        const QString unit = QString::fromStdString(c.unit);
        if (c.rangeRule || c.minimumRule) {
            cell(1, QStringLiteral("%1 %2").arg(c.demand, 0, 'f', 2).arg(unit), true);
            cell(2, QStringLiteral("%1 %2 %3").arg(c.rangeRule && !c.rangeLower ? QStringLiteral("≤") : QStringLiteral("≥")).arg(c.resistance, 0, 'f', 2).arg(unit), true);
        } else {
            cell(1, QStringLiteral("%1 %2").arg(c.demand, 0, 'f', 2).arg(unit), true);
            cell(2, QStringLiteral("%1 %2").arg(c.resistance, 0, 'f', 2).arg(unit), true);
        }
        auto *ratio = cell(3, c.rangeRule ? (c.ok() ? tr("atende") : tr("não atende")) : QString::number(c.ratio(), 'f', 3), true);
        ratio->setForeground(c.ok() ? QColor(QStringLiteral("#1b7f3b")) : QColor(QStringLiteral("#c0392b")));
        QFont font = ratio->font();
        font.setBold(true);
        ratio->setFont(font);
        cell(4, QString::fromStdString(c.clause), false);
        auto *name = table->item(static_cast<int>(i), 0);
        QStringList memo;
        for (const auto &line : c.lines) {
            memo << QString::fromStdString(line);
        }
        name->setToolTip(memo.join(QLatin1Char('\n')));
    }
}

// ── viga ─────────────────────────────────────────────────────────────────────

void CompositeDialog::buildBeamTab(QTabWidget *tabs)
{
    auto *page = new QWidget;
    auto *layout = new QHBoxLayout(page);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFixedWidth(430);
    auto *inner = new QWidget;
    auto *left = new QVBoxLayout(inner);

    auto *memberBox = new QGroupBox(tr("Viga"));
    auto *memberForm = new QFormLayout(memberBox);
    m_member = new QComboBox;
    for (const auto &b : m_beams) {
        m_member->addItem(tr("Barra %1 — %2 (%3 m)").arg(b.memberId).arg(b.name).arg(b.lengthMm / 1000.0, 0, 'f', 2), b.memberId);
    }
    memberForm->addRow(tr("Barra"), m_member);
    addField(memberForm, m_beamFields, QStringLiteral("left"), tr("Distância à viga vizinha (esq.)"), 3000, 0, 20000, 0, 100, tr(" mm"), tr("Entre eixos; 0 = sem laje desse lado. Largura efetiva de cada lado = menor entre L/8 e metade da distância."));
    addField(memberForm, m_beamFields, QStringLiteral("right"), tr("Distância à viga vizinha (dir.)"), 3000, 0, 20000, 0, 100, tr(" mm"));
    m_shored = new QCheckBox(tr("Construção escorada"));
    memberForm->addRow(m_shored);
    addField(memberForm, m_beamFields, QStringLiteral("lbc"), tr("Lb na construção (0 = travada pela forma)"), 0, 0, 20000, 0, 100, tr(" mm"));
    left->addWidget(memberBox);

    auto *slabBox = new QGroupBox(tr("Laje"));
    auto *slabForm = new QFormLayout(slabBox);
    m_slabKind = new QComboBox;
    m_slabKind->addItem(tr("Maciça moldada no local"), 0);
    m_slabKind->addItem(tr("Steel deck — nervuras perpendiculares à viga"), 1);
    m_slabKind->addItem(tr("Steel deck — nervuras paralelas à viga"), 2);
    slabForm->addRow(tr("Tipo"), m_slabKind);
    addField(slabForm, m_beamFields, QStringLiteral("tc"), tr("Concreto acima da forma (t_c)"), 100, 20, 600, 0, 5, tr(" mm"));
    addField(slabForm, m_beamFields, QStringLiteral("hF"), tr("Altura da nervura (h_F)"), 0, 0, 200, 0, 5, tr(" mm"), tr("0 na laje maciça"));
    addField(slabForm, m_beamFields, QStringLiteral("bF"), tr("Largura média da nervura (b_F)"), 120, 0, 400, 0, 5, tr(" mm"));
    addField(slabForm, m_beamFields, QStringLiteral("fck"), tr("f_ck do concreto"), 25, 20, 90, 0, 5, tr(" MPa"));
    addField(slabForm, m_beamFields, QStringLiteral("density"), tr("Massa específica"), 2400, 1800, 2600, 0, 50, tr(" kg/m³"));
    left->addWidget(slabBox);

    auto *studBox = new QGroupBox(tr("Conectores (pino com cabeça)"));
    auto *studForm = new QFormLayout(studBox);
    addField(studForm, m_beamFields, QStringLiteral("stud_d"), tr("Diâmetro"), 19, 12, 25, 0, 1, tr(" mm"));
    addField(studForm, m_beamFields, QStringLiteral("stud_h"), tr("Comprimento após soldagem (0 = não verificar)"), 0, 0, 300, 0, 5, tr(" mm"));
    addField(studForm, m_beamFields, QStringLiteral("stud_fu"), tr("f_u do conector"), 415, 300, 600, 0, 5, tr(" MPa"));
    addField(studForm, m_beamFields, QStringLiteral("per_rib"), tr("Conectores por nervura"), 1, 1, 4, 0, 1);
    addField(studForm, m_beamFields, QStringLiteral("rows"), tr("Linhas de conectores na mesa"), 1, 1, 3, 0, 1);
    addField(studForm, m_beamFields, QStringLiteral("emh"), tr("e_mh (forma perpendicular)"), 60, 10, 200, 0, 5, tr(" mm"));
    addField(studForm, m_beamFields, QStringLiteral("spacing"), tr("Espaçamento longitudinal"), 200, 50, 1000, 0, 25, tr(" mm"));
    auto *propose = new QPushButton(tr("Propor espaçamento"));
    propose->setToolTip(tr("Maior espaçamento (múltiplo de 25 mm) que atende o momento e o grau mínimo de interação"));
    studForm->addRow(propose);
    left->addWidget(studBox);

    auto *loadBox = new QGroupBox(tr("Cargas e esforços"));
    auto *loadForm = new QFormLayout(loadBox);
    addField(loadForm, m_beamFields, QStringLiteral("q_perm"), tr("Permanente adicional"), 1.5, 0, 50, 2, 0.25, tr(" kN/m²"), tr("Revestimento, forro, instalações (aplicadas depois do endurecimento)"));
    addField(loadForm, m_beamFields, QStringLiteral("q_var"), tr("Sobrecarga"), 3.0, 0, 50, 2, 0.5, tr(" kN/m²"));
    addField(loadForm, m_beamFields, QStringLiteral("q_constr"), tr("Construção"), 1.0, 0, 10, 2, 0.25, tr(" kN/m²"));
    addField(loadForm, m_beamFields, QStringLiteral("psi2"), tr("ψ₂ da sobrecarga (longa duração)"), 0.3, 0, 1, 2, 0.1);
    addField(loadForm, m_beamFields, QStringLiteral("camber"), tr("Contraflecha"), 0, 0, 200, 0, 5, tr(" mm"));
    addField(loadForm, m_beamFields, QStringLiteral("defl_n"), tr("Flecha limite: L /"), 350, 100, 1000, 0, 50);
    m_demandSource = new QComboBox;
    m_demandSource->addItem(tr("Esforços da análise (combinações últimas)"), 0);
    m_demandSource->addItem(tr("Calculados pelas cargas acima (biapoiada)"), 1);
    m_demandSource->addItem(tr("Informados"), 2);
    loadForm->addRow(tr("Esforços de cálculo"), m_demandSource);
    m_msdManual = new QDoubleSpinBox;
    m_msdManual->setRange(0, 1e6);
    m_msdManual->setDecimals(1);
    m_msdManual->setSuffix(tr(" kN·m"));
    m_vsdManual = new QDoubleSpinBox;
    m_vsdManual->setRange(0, 1e6);
    m_vsdManual->setDecimals(1);
    m_vsdManual->setSuffix(tr(" kN"));
    loadForm->addRow(tr("M_Sd informado"), m_msdManual);
    loadForm->addRow(tr("V_Sd informado"), m_vsdManual);
    left->addWidget(loadBox);
    left->addStretch(1);
    scroll->setWidget(inner);
    layout->addWidget(scroll);

    auto *right = new QVBoxLayout;
    m_beamSummary = new QLabel;
    m_beamSummary->setTextFormat(Qt::RichText);
    m_beamSummary->setWordWrap(true);
    right->addWidget(m_beamSummary);
    m_beamTable = makeChecksTable();
    right->addWidget(m_beamTable, 3);
    m_beamMemo = new QPlainTextEdit;
    m_beamMemo->setReadOnly(true);
    m_beamMemo->setPlaceholderText(tr("Memória de cálculo"));
    right->addWidget(m_beamMemo, 2);
    layout->addLayout(right, 1);
    tabs->addTab(page, tr("Viga mista"));

    auto rerun = [this]() { runBeam(); };
    for (const auto &f : m_beamFields) {
        connect(f.box, &QDoubleSpinBox::valueChanged, this, rerun);
    }
    connect(m_slabKind, &QComboBox::currentIndexChanged, this, rerun);
    connect(m_demandSource, &QComboBox::currentIndexChanged, this, rerun);
    connect(m_shored, &QCheckBox::toggled, this, rerun);
    connect(m_msdManual, &QDoubleSpinBox::valueChanged, this, rerun);
    connect(m_vsdManual, &QDoubleSpinBox::valueChanged, this, rerun);
    connect(m_member, &QComboBox::currentIndexChanged, this, [this](int index) { loadBeam(index); });
    connect(propose, &QPushButton::clicked, this, [this]() { proposeBeamSpacing(); });
}

QMap<QString, double> CompositeDialog::beamValues() const
{
    QMap<QString, double> v;
    for (const auto &f : m_beamFields) {
        v.insert(f.key, f.box->value());
    }
    v.insert(QStringLiteral("slab_kind"), m_slabKind->currentData().toInt());
    v.insert(QStringLiteral("shored"), m_shored->isChecked() ? 1.0 : 0.0);
    v.insert(QStringLiteral("demand_source"), m_demandSource->currentData().toInt());
    v.insert(QStringLiteral("msd_manual"), m_msdManual->value());
    v.insert(QStringLiteral("vsd_manual"), m_vsdManual->value());
    return v;
}

void CompositeDialog::loadBeam(int index)
{
    if (index < 0 || index >= m_beams.size()) {
        return;
    }
    m_loading = true;
    const auto &b = m_beams[index];
    for (auto &f : m_beamFields) {
        if (b.saved.contains(f.key)) {
            f.box->setValue(b.saved.value(f.key));
        }
    }
    if (b.saved.contains(QStringLiteral("slab_kind"))) {
        m_slabKind->setCurrentIndex(std::clamp(static_cast<int>(b.saved.value(QStringLiteral("slab_kind"))), 0, 2));
    }
    m_shored->setChecked(b.saved.value(QStringLiteral("shored")) != 0.0);
    m_msdManual->setValue(b.saved.value(QStringLiteral("msd_manual")));
    m_vsdManual->setValue(b.saved.value(QStringLiteral("vsd_manual")));
    const int source = b.saved.contains(QStringLiteral("demand_source")) ? static_cast<int>(b.saved.value(QStringLiteral("demand_source"))) : (b.hasAnalysis ? 0 : 1);
    m_demandSource->setCurrentIndex(std::clamp(source, 0, 2));
    m_loading = false;
    runBeam();
}

namespace {

QString html(const QString &text, bool ok)
{
    return QStringLiteral("<span style='color:%1;font-weight:600;'>%2</span>").arg(ok ? QStringLiteral("#1b7f3b") : QStringLiteral("#c0392b"), text);
}

} // namespace

void CompositeDialog::runBeam()
{
    if (m_loading || m_member->currentIndex() < 0) {
        return;
    }
    const auto &b = m_beams[m_member->currentIndex()];
    const auto v = beamValues();
    C::BeamInput in;
    in.steel.d = b.d;
    in.steel.bf = b.bf;
    in.steel.tf = b.tf;
    in.steel.tw = b.tw;
    in.steel.area = b.section.g.area;
    in.steel.inertia = b.section.g.Ix;
    in.steel.zpl = b.section.g.Zx;
    in.steel.fy = b.fy;
    in.steel.fu = b.fu;
    in.concrete.fck = v.value(QStringLiteral("fck"));
    in.concrete.density = v.value(QStringLiteral("density"));
    in.slab.kind = static_cast<C::SlabKind>(m_slabKind->currentData().toInt());
    in.slab.tc = v.value(QStringLiteral("tc"));
    in.slab.hF = in.slab.kind == C::SlabKind::Solid ? 0.0 : v.value(QStringLiteral("hF"));
    in.slab.bF = v.value(QStringLiteral("bF"));
    in.span = b.lengthMm;
    in.leftSpacing = v.value(QStringLiteral("left"));
    in.rightSpacing = v.value(QStringLiteral("right"));
    in.shored = m_shored->isChecked();
    in.lbConstruction = v.value(QStringLiteral("lbc"));
    in.studs.diameter = v.value(QStringLiteral("stud_d"));
    in.studs.height = v.value(QStringLiteral("stud_h"));
    in.studs.fu = v.value(QStringLiteral("stud_fu"));
    in.studs.perRib = static_cast<int>(v.value(QStringLiteral("per_rib")));
    in.studs.rows = static_cast<int>(v.value(QStringLiteral("rows")));
    in.studs.emh = v.value(QStringLiteral("emh"));
    in.studs.spacing = v.value(QStringLiteral("spacing"));

    // cargas lineares (N/mm = kN/m) pela largura de influencia
    const double trib = 0.5 * (in.leftSpacing + in.rightSpacing) / 1000.0;   // m
    const double steelWeight = in.steel.aa() * 1e-6 * 7850.0 * 9.80665 / 1000.0;   // kN/m
    const double wet = (in.slab.tc + 0.5 * in.slab.hF) / 1000.0 * ((in.concrete.density + 100.0) * 9.80665 / 1000.0);   // kN/m2
    const double deckWeight = in.slab.kind == C::SlabKind::Solid ? 0.0 : 0.10;
    C::BeamDemand dm;
    const double L = in.span;
    const double k = L * L / 8.0;   // N.mm por (N/mm)
    dm.qBefore = steelWeight + (wet + deckWeight) * trib;
    dm.qPermanent = v.value(QStringLiteral("q_perm")) * trib;
    dm.qVariable = v.value(QStringLiteral("q_var")) * trib;
    dm.psi2 = v.value(QStringLiteral("psi2"));
    dm.camber = v.value(QStringLiteral("camber"));
    dm.deflectionLimit = v.value(QStringLiteral("defl_n"));
    const double qConstr = v.value(QStringLiteral("q_constr")) * trib;
    dm.mGaS = dm.qBefore * k;
    dm.mGaSd = (1.4 * (steelWeight + deckWeight * trib) + 1.5 * (wet * trib + qConstr)) * k;
    dm.mLS = (dm.qPermanent + dm.qVariable) * k;
    const double calcMsd = (1.4 * (dm.qBefore + dm.qPermanent) + 1.5 * dm.qVariable) * k;
    const double calcVsd = (1.4 * (dm.qBefore + dm.qPermanent) + 1.5 * dm.qVariable) * L / 2.0;
    const int source = m_demandSource->currentData().toInt();
    QString sourceText;
    if (source == 0 && b.hasAnalysis) {
        dm.msd = b.msdAnalysis;
        dm.vsd = b.vsdAnalysis;
        sourceText = tr("esforços da análise");
    } else if (source == 2) {
        dm.msd = m_msdManual->value() * 1e6;
        dm.vsd = m_vsdManual->value() * 1e3;
        sourceText = tr("esforços informados");
    } else {
        dm.msd = calcMsd;
        dm.vsd = calcVsd;
        sourceText = source == 0 ? tr("cargas informadas (análise indisponível)") : tr("cargas informadas");
    }
    const auto r = C::checkBeam(in, dm, &b.section);
    showChecks(m_beamTable, r.checks);
    QString summary = tr("<b>Barra %1 — %2</b> · M_Sd = %3 kN·m, V_Sd = %4 kN (%5)<br>b_ef = %6 mm · Q_Rd = %7 kN (R_g = %8, R_p = %9) · %10 conectores por meio vão (completa: %11) · η_i = %12 (mín. %13) · M_Rd = %14 kN·m")
                          .arg(b.memberId).arg(b.name).arg(dm.msd / 1e6, 0, 'f', 1).arg(dm.vsd / 1e3, 0, 'f', 1).arg(sourceText).arg(r.bEff, 0, 'f', 0).arg(r.qRd / 1e3, 0, 'f', 1)
                          .arg(r.rg, 0, 'f', 2).arg(r.rp, 0, 'f', 2).arg(r.connectorsHalfSpan).arg(r.connectorsFullInteraction).arg(r.interaction, 0, 'f', 2)
                          .arg(r.interactionMin, 0, 'f', 2).arg(r.mRd / 1e6, 0, 'f', 1);
    summary += QStringLiteral("<br>") + (r.maxRatio <= 1.0 + 1e-9 ? html(tr("Atende — razão máxima %1 (%2)").arg(r.maxRatio, 0, 'f', 3).arg(QString::fromStdString(r.governing)), true)
                                                              : html(tr("Não atende — %1 (razão %2)").arg(QString::fromStdString(r.governing)).arg(r.maxRatio, 0, 'f', 3), false));
    m_beamSummary->setText(summary);
    QStringList memo;
    for (const auto &c : r.checks) {
        memo << QStringLiteral("%1 [%2]").arg(QString::fromStdString(c.name), QString::fromStdString(c.clause));
        for (const auto &line : c.lines) {
            memo << QStringLiteral("    ") + QString::fromStdString(line);
        }
    }
    for (const auto &note : r.notes) {
        memo << QStringLiteral("* ") + QString::fromStdString(note);
    }
    m_beamMemo->setPlainText(memo.join(QLatin1Char('\n')));
    if (v != b.saved) {   // so grava no projeto quando o usuario muda alguma entrada
        m_beams[m_member->currentIndex()].saved = v;
        emit beamInputsChanged(b.memberId, v);
    }
}

void CompositeDialog::proposeBeamSpacing()
{
    // reaproveita a mesma montagem de runBeam: roda uma vez com o espaçamento atual para obter demandas e então varre
    if (m_member->currentIndex() < 0) {
        return;
    }
    const auto &b = m_beams[m_member->currentIndex()];
    const auto v = beamValues();
    C::BeamInput in;
    in.steel.d = b.d;
    in.steel.bf = b.bf;
    in.steel.tf = b.tf;
    in.steel.tw = b.tw;
    in.steel.area = b.section.g.area;
    in.steel.inertia = b.section.g.Ix;
    in.steel.zpl = b.section.g.Zx;
    in.steel.fy = b.fy;
    in.steel.fu = b.fu;
    in.concrete.fck = v.value(QStringLiteral("fck"));
    in.concrete.density = v.value(QStringLiteral("density"));
    in.slab.kind = static_cast<C::SlabKind>(m_slabKind->currentData().toInt());
    in.slab.tc = v.value(QStringLiteral("tc"));
    in.slab.hF = in.slab.kind == C::SlabKind::Solid ? 0.0 : v.value(QStringLiteral("hF"));
    in.slab.bF = v.value(QStringLiteral("bF"));
    in.span = b.lengthMm;
    in.leftSpacing = v.value(QStringLiteral("left"));
    in.rightSpacing = v.value(QStringLiteral("right"));
    in.shored = m_shored->isChecked();
    in.studs.diameter = v.value(QStringLiteral("stud_d"));
    in.studs.fu = v.value(QStringLiteral("stud_fu"));
    in.studs.perRib = static_cast<int>(v.value(QStringLiteral("per_rib")));
    in.studs.rows = static_cast<int>(v.value(QStringLiteral("rows")));
    in.studs.emh = v.value(QStringLiteral("emh"));
    C::BeamDemand dm;
    const int source = m_demandSource->currentData().toInt();
    if (source == 0 && b.hasAnalysis) {
        dm.msd = b.msdAnalysis;
        dm.vsd = b.vsdAnalysis;
    } else if (source == 2) {
        dm.msd = m_msdManual->value() * 1e6;
        dm.vsd = m_vsdManual->value() * 1e3;
    } else {
        const double trib = 0.5 * (in.leftSpacing + in.rightSpacing) / 1000.0;
        const double steelWeight = in.steel.aa() * 1e-6 * 7850.0 * 9.80665 / 1000.0;
        const double wet = (in.slab.tc + 0.5 * in.slab.hF) / 1000.0 * ((in.concrete.density + 100.0) * 9.80665 / 1000.0);
        const double q = steelWeight + wet * trib + v.value(QStringLiteral("q_perm")) * trib;
        const double qv = v.value(QStringLiteral("q_var")) * trib;
        dm.msd = (1.4 * q + 1.5 * qv) * in.span * in.span / 8.0;
    }
    const double s = C::proposeSpacing(in, dm, &b.section);
    if (s > 0.0) {
        for (auto &f : m_beamFields) {
            if (f.key == QLatin1String("spacing")) {
                f.box->setValue(s);   // dispara a nova verificacao
            }
        }
    } else {
        m_beamSummary->setText(html(tr("Nenhum espaçamento permitido atende o momento solicitante: aumente a laje, os conectores (linhas) ou o perfil."), false));
    }
}

// ── laje ─────────────────────────────────────────────────────────────────────

void CompositeDialog::buildDeckTab(QTabWidget *tabs)
{
    auto *page = new QWidget;
    auto *layout = new QHBoxLayout(page);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFixedWidth(430);
    auto *inner = new QWidget;
    auto *left = new QVBoxLayout(inner);

    auto *geom = new QGroupBox(tr("Laje (por metro de largura)"));
    auto *geomForm = new QFormLayout(geom);
    auto def = [&](const QString &key, double fallback) { return m_deckSaved.contains(key) ? m_deckSaved.value(key) : fallback; };
    addField(geomForm, m_deckFields, QStringLiteral("hF"), tr("Altura da nervura h_F"), def(QStringLiteral("hF"), 75), 20, 150, 0, 5, tr(" mm"));
    addField(geomForm, m_deckFields, QStringLiteral("tc"), tr("Concreto acima da forma t_c"), def(QStringLiteral("tc"), 70), 40, 300, 0, 5, tr(" mm"));
    addField(geomForm, m_deckFields, QStringLiteral("bn"), tr("Distância entre nervuras b_n"), def(QStringLiteral("bn"), 150), 50, 400, 0, 5, tr(" mm"));
    addField(geomForm, m_deckFields, QStringLiteral("bb"), tr("Largura média da nervura b_b"), def(QStringLiteral("bb"), 110), 30, 300, 0, 5, tr(" mm"));
    addField(geomForm, m_deckFields, QStringLiteral("span"), tr("Vão teórico L_F"), def(QStringLiteral("span"), 2500), 500, 8000, 0, 100, tr(" mm"));
    m_continuity = new QComboBox;
    m_continuity->addItem(tr("Vão simples"), 1);
    m_continuity->addItem(tr("Dois vãos"), 2);
    m_continuity->addItem(tr("Três ou mais vãos"), 3);
    geomForm->addRow(tr("Continuidade"), m_continuity);
    addField(geomForm, m_deckFields, QStringLiteral("fck"), tr("f_ck do concreto"), def(QStringLiteral("fck"), 25), 20, 90, 0, 5, tr(" MPa"));
    addField(geomForm, m_deckFields, QStringLiteral("density"), tr("Massa específica"), def(QStringLiteral("density"), 2400), 1800, 2600, 0, 50, tr(" kg/m³"));
    left->addWidget(geom);

    auto *prop = new QGroupBox(tr("Forma de aço (dados do fabricante)"));
    auto *propForm = new QFormLayout(prop);
    addField(propForm, m_deckFields, QStringLiteral("tF"), tr("Espessura t_F"), def(QStringLiteral("tF"), 0.80), 0.4, 3.0, 2, 0.05, tr(" mm"));
    addField(propForm, m_deckFields, QStringLiteral("fyF"), tr("f_yF"), def(QStringLiteral("fyF"), 345), 230, 550, 0, 5, tr(" MPa"));
    addField(propForm, m_deckFields, QStringLiteral("afEf"), tr("A_F,ef"), def(QStringLiteral("afEf"), 1100), 100, 6000, 0, 50, tr(" mm²/m"));
    addField(propForm, m_deckFields, QStringLiteral("ie"), tr("I efetivo"), def(QStringLiteral("ie"), 400000), 10000, 5e7, 0, 10000, tr(" mm⁴/m"));
    addField(propForm, m_deckFields, QStringLiteral("wef"), tr("W efetivo (fase inicial)"), def(QStringLiteral("wef"), 13000), 500, 200000, 0, 500, tr(" mm³/m"));
    addField(propForm, m_deckFields, QStringLiteral("wpl"), tr("W plástico efetivo"), def(QStringLiteral("wpl"), 15000), 500, 250000, 0, 500, tr(" mm³/m"));
    addField(propForm, m_deckFields, QStringLiteral("e"), tr("CG da seção efetiva (da base) e"), def(QStringLiteral("e"), 40), 5, 100, 1, 1, tr(" mm"));
    addField(propForm, m_deckFields, QStringLiteral("ep"), tr("LNP da seção efetiva (da base) e_p"), def(QStringLiteral("ep"), 40), 5, 100, 1, 1, tr(" mm"));
    addField(propForm, m_deckFields, QStringLiteral("selfw"), tr("Peso da forma"), def(QStringLiteral("selfw"), 0.10), 0.02, 0.5, 2, 0.01, tr(" kN/m²"));
    addField(propForm, m_deckFields, QStringLiteral("m"), tr("m (cisalhamento longitudinal)"), def(QStringLiteral("m"), 150), 20, 600, 0, 5, tr(" N/mm²"), tr("Constantes de ensaio (EN 1994-1-1, CSSBI S2 ou ANSI/ASCE 3), com a unidade adaptada a N/mm²"));
    addField(propForm, m_deckFields, QStringLiteral("k"), tr("k (cisalhamento longitudinal)"), def(QStringLiteral("k"), 0.20), 0.0, 2.0, 3, 0.05, tr(" N/mm²"));
    addField(propForm, m_deckFields, QStringLiteral("gsl"), tr("γ do cisalhamento longitudinal"), def(QStringLiteral("gsl"), 1.25), 1.0, 2.0, 2, 0.05);
    addField(propForm, m_deckFields, QStringLiteral("vvf"), tr("V_v,F,Rd da forma"), def(QStringLiteral("vvf"), 0), 0, 1e6, 0, 1000, tr(" N/m"));
    left->addWidget(prop);

    auto *loads = new QGroupBox(tr("Ações"));
    auto *loadForm = new QFormLayout(loads);
    addField(loadForm, m_deckFields, QStringLiteral("perm"), tr("Permanente (revestimento etc.)"), def(QStringLiteral("perm"), 1.5), 0, 30, 2, 0.25, tr(" kN/m²"));
    addField(loadForm, m_deckFields, QStringLiteral("var"), tr("Sobrecarga"), def(QStringLiteral("var"), 3.0), 0, 30, 2, 0.5, tr(" kN/m²"));
    addField(loadForm, m_deckFields, QStringLiteral("constr"), tr("Construção"), def(QStringLiteral("constr"), 1.0), 0, 10, 2, 0.25, tr(" kN/m²"));
    left->addWidget(loads);
    left->addStretch(1);
    scroll->setWidget(inner);
    layout->addWidget(scroll);

    auto *right = new QVBoxLayout;
    m_deckSummary = new QLabel;
    m_deckSummary->setTextFormat(Qt::RichText);
    m_deckSummary->setWordWrap(true);
    right->addWidget(m_deckSummary);
    m_deckTable = makeChecksTable();
    right->addWidget(m_deckTable, 3);
    m_deckMemo = new QPlainTextEdit;
    m_deckMemo->setReadOnly(true);
    right->addWidget(m_deckMemo, 2);
    layout->addLayout(right, 1);
    tabs->addTab(page, tr("Laje mista (steel deck)"));

    m_continuity->setCurrentIndex(std::clamp(static_cast<int>(def(QStringLiteral("continuity"), 1)) - 1, 0, 2));
    for (const auto &f : m_deckFields) {
        connect(f.box, &QDoubleSpinBox::valueChanged, this, [this]() { runDeck(); });
    }
    connect(m_continuity, &QComboBox::currentIndexChanged, this, [this]() { runDeck(); });
    runDeck();
}

QMap<QString, double> CompositeDialog::deckValues() const
{
    QMap<QString, double> v;
    for (const auto &f : m_deckFields) {
        v.insert(f.key, f.box->value());
    }
    v.insert(QStringLiteral("continuity"), m_continuity->currentData().toInt());
    return v;
}

void CompositeDialog::runDeck()
{
    const auto v = deckValues();
    C::DeckInput deck;
    deck.hF = v.value(QStringLiteral("hF"));
    deck.tc = v.value(QStringLiteral("tc"));
    deck.bn = v.value(QStringLiteral("bn"));
    deck.bb = v.value(QStringLiteral("bb"));
    deck.spanF = v.value(QStringLiteral("span"));
    deck.continuity = static_cast<int>(v.value(QStringLiteral("continuity")));
    deck.concrete.fck = v.value(QStringLiteral("fck"));
    deck.concrete.density = v.value(QStringLiteral("density"));
    deck.tF = v.value(QStringLiteral("tF"));
    deck.fyF = v.value(QStringLiteral("fyF"));
    deck.afEf = v.value(QStringLiteral("afEf"));
    deck.ie = v.value(QStringLiteral("ie"));
    deck.wEfBottom = v.value(QStringLiteral("wef"));
    deck.wPlastic = v.value(QStringLiteral("wpl"));
    deck.e = v.value(QStringLiteral("e"));
    deck.ep = v.value(QStringLiteral("ep"));
    deck.selfWeight = v.value(QStringLiteral("selfw"));
    deck.mFactor = v.value(QStringLiteral("m"));
    deck.kFactor = v.value(QStringLiteral("k"));
    deck.gammaSl = v.value(QStringLiteral("gsl"));
    deck.vvF = v.value(QStringLiteral("vvf"));
    C::DeckLoads loads;
    loads.permanent = v.value(QStringLiteral("perm"));
    loads.variable = v.value(QStringLiteral("var"));
    loads.construction = v.value(QStringLiteral("constr"));
    const auto r = C::checkDeck(deck, loads);
    showChecks(m_deckTable, r.checks);
    QString summary = tr("Concreto fresco: %1 kN/m² · M_Rd⁺ = %2 kN·m/m · V_l,Rd = %3 kN/m · V_v,Rd = %4 kN/m<br>")
                          .arg(r.slabWeight, 0, 'f', 2).arg(r.mRdPositive / 1e6, 0, 'f', 2).arg(r.vlRd / 1e3, 0, 'f', 1).arg(r.vvRd / 1e3, 0, 'f', 1);
    summary += r.maxRatio <= 1.0 + 1e-9 ? html(tr("Atende — razão máxima %1 (%2)").arg(r.maxRatio, 0, 'f', 3).arg(QString::fromStdString(r.governing)), true)
                                         : html(tr("Não atende — %1 (razão %2)").arg(QString::fromStdString(r.governing)).arg(r.maxRatio, 0, 'f', 3), false);
    m_deckSummary->setText(summary);
    QStringList memo;
    for (const auto &c : r.checks) {
        memo << QStringLiteral("%1 [%2]").arg(QString::fromStdString(c.name), QString::fromStdString(c.clause));
        for (const auto &line : c.lines) {
            memo << QStringLiteral("    ") + QString::fromStdString(line);
        }
    }
    for (const auto &note : r.notes) {
        memo << QStringLiteral("* ") + QString::fromStdString(note);
    }
    m_deckMemo->setPlainText(memo.join(QLatin1Char('\n')));
    if (v != m_deckSaved) {
        m_deckSaved = v;
        emit deckInputsChanged(v);
    }
}

} // namespace Structura::UI
