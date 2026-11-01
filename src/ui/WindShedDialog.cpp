#include "WindShedDialog.h"

#include "TopographicFactorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::WindShedService;

namespace {

QDoubleSpinBox *spin(QWidget *parent, double min, double max, double value, int decimals, const QString &suffix = QString())
{
    auto *s = new QDoubleSpinBox(parent);
    s->setRange(min, max);
    s->setDecimals(decimals);
    s->setValue(value);
    s->setSuffix(suffix);
    return s;
}

} // namespace

WindShedDialog::WindShedDialog(const QVector<WindShedService::Bar> &bars, QWidget *parent)
    : QDialog(parent)
    , m_bars(bars)
{
    setWindowTitle(tr("Vento em galpões (NBR 6123:2023)"));
    resize(880, 820);
    auto *root = new QVBoxLayout(this);

    auto *site = new QGroupBox(tr("Velocidade característica"), this);
    auto *siteForm = new QFormLayout(site);
    m_v0 = spin(site, 1.0, 100.0, 35.0, 1, tr(" m/s"));
    m_v0->setToolTip(tr("Velocidade básica V0: isopletas da Figura 1 da NBR 6123:2023"));
    m_s1 = spin(site, 0.1, 2.5, 1.0, 3);
    auto *s1Button = new QPushButton(tr("Calcular S1 topográfico…"), site);
    auto *s1Row = new QWidget(site);
    auto *s1Layout = new QHBoxLayout(s1Row);
    s1Layout->setContentsMargins(0, 0, 0, 0);
    s1Layout->addWidget(m_s1);
    s1Layout->addWidget(s1Button);
    m_s3 = spin(site, 0.1, 2.0, 1.0, 2);
    m_s3->setToolTip(tr("Fator estatístico S3 (Tabela 4)"));
    m_category = new QComboBox(site);
    m_category->addItem(tr("I — superfícies lisas de grandes dimensões"), 1);
    m_category->addItem(tr("II — terreno aberto, poucos obstáculos"), 2);
    m_category->addItem(tr("III — terreno plano com obstáculos esparsos"), 3);
    m_category->addItem(tr("IV — obstáculos numerosos e pouco espaçados"), 4);
    m_category->addItem(tr("V — obstáculos grandes, altos e pouco espaçados"), 5);
    m_category->setCurrentIndex(1);
    m_reference = new QComboBox(site);
    m_reference->addItem(tr("Cumeeira (altura total)"), static_cast<int>(WindShedService::ReferenceHeight::Ridge));
    m_reference->addItem(tr("Beiral"), static_cast<int>(WindShedService::ReferenceHeight::Eave));
    m_reference->addItem(tr("Altura média da cobertura"), static_cast<int>(WindShedService::ReferenceHeight::Mean));
    m_reference->setToolTip(tr("Cota em que se calcula o fator S2. A cumeeira é o critério usual e conservador."));
    siteForm->addRow(tr("Velocidade básica V0"), m_v0);
    siteForm->addRow(tr("Fator topográfico S1"), s1Row);
    siteForm->addRow(tr("Fator estatístico S3"), m_s3);
    siteForm->addRow(tr("Categoria de rugosidade"), m_category);
    siteForm->addRow(tr("Cota de referência de S2"), m_reference);
    root->addWidget(site);

    auto *geometry = new QGroupBox(tr("Edificação e casos"), this);
    auto *geometryForm = new QFormLayout(geometry);
    m_ridge = new QComboBox(geometry);
    m_ridge->addItem(tr("Cumeeira paralela ao eixo Y (pórticos no plano XZ)"), static_cast<int>(WindShedService::RidgeAxis::Y));
    m_ridge->addItem(tr("Cumeeira paralela ao eixo X (pórticos no plano YZ)"), static_cast<int>(WindShedService::RidgeAxis::X));
    m_overhang = spin(geometry, 0.0, 20.0, 0.0, 2, tr(" m"));
    m_overhang->setToolTip(tr("Prolongamento da faixa de influência além do primeiro e do último pórtico (beiral de oitão)"));
    m_transverse = new QCheckBox(tr("Vento transversal à cumeeira (T+ e T−, α = 90°)"), geometry);
    m_transverse->setChecked(true);
    m_longitudinal = new QCheckBox(tr("Vento longitudinal (L+ e L−, α = 0°)"), geometry);
    m_longitudinal->setChecked(true);
    geometryForm->addRow(tr("Eixo da cumeeira"), m_ridge);
    geometryForm->addRow(tr("Prolongamento nas extremidades"), m_overhang);
    geometryForm->addRow(QString(), m_transverse);
    geometryForm->addRow(QString(), m_longitudinal);
    root->addWidget(geometry);

    auto *openings = new QGroupBox(tr("Pressão interna (6.3.2)"), this);
    auto *openingsForm = new QFormLayout(openings);
    m_cpiMode = new QComboBox(openings);
    m_cpiMode->addItem(tr("Edificação estanque: Cpi = −0,2 ou 0"), static_cast<int>(WindShedService::CpiMode::Sealed));
    m_cpiMode->addItem(tr("Quatro faces igualmente permeáveis: Cpi = −0,3 ou 0"), static_cast<int>(WindShedService::CpiMode::FourFacesPermeable));
    m_cpiMode->addItem(tr("Duas faces opostas permeáveis, as outras impermeáveis"), static_cast<int>(WindShedService::CpiMode::TwoOppositeFaces));
    m_cpiMode->addItem(tr("Abertura dominante na face de barlavento"), static_cast<int>(WindShedService::CpiMode::DominantWindward));
    m_cpiMode->addItem(tr("Abertura dominante na face de sotavento (Cpi = Ce da parede de sotavento)"), static_cast<int>(WindShedService::CpiMode::DominantLeeward));
    m_cpiMode->addItem(tr("Abertura dominante em zona de alta sucção externa"), static_cast<int>(WindShedService::CpiMode::DominantHighSuction));
    m_cpiMode->addItem(tr("Valores informados"), static_cast<int>(WindShedService::CpiMode::Custom));
    m_cpiMode->setCurrentIndex(1);
    m_permeable = new QComboBox(openings);
    m_permeable->addItem(tr("Paredes laterais permeáveis (perpendiculares ao vento transversal)"), 1);
    m_permeable->addItem(tr("Oitões permeáveis (perpendiculares ao vento longitudinal)"), 0);
    m_ratio = spin(openings, 0.05, 20.0, 1.0, 2);
    m_ratio->setToolTip(tr("Razão entre a área das aberturas dominantes e a área total das demais aberturas (6.3.2.1-c)"));
    m_cpiFirst = spin(openings, -2.0, 2.0, -0.3, 2);
    m_cpiSecond = spin(openings, -2.0, 2.0, 0.0, 2);
    openingsForm->addRow(tr("Situação das aberturas"), m_cpiMode);
    openingsForm->addRow(tr("Faces permeáveis"), m_permeable);
    openingsForm->addRow(tr("Razão de áreas"), m_ratio);
    openingsForm->addRow(tr("Cpi (1º valor)"), m_cpiFirst);
    openingsForm->addRow(tr("Cpi (2º valor)"), m_cpiSecond);
    root->addWidget(openings);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    root->addWidget(m_summary);
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({tr("Caso"), tr("q (kN/m²)"), tr("Classe"), tr("Cpi"), tr("Barras carregadas")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(m_table, 1);
    m_notes = new QPlainTextEdit(this);
    m_notes->setReadOnly(true);
    m_notes->setMaximumHeight(120);
    root->addWidget(m_notes);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(tr("Gerar casos de carga"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);

    connect(s1Button, &QPushButton::clicked, this, [this]() { pickTopography(); });
    for (auto *s : {m_v0, m_s1, m_s3, m_ratio, m_cpiFirst, m_cpiSecond, m_overhang}) {
        connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this]() { updatePreview(); });
    }
    for (auto *c : {m_category, m_ridge, m_reference, m_cpiMode, m_permeable}) {
        connect(c, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { updatePreview(); });
    }
    for (auto *c : {m_transverse, m_longitudinal}) {
        connect(c, &QCheckBox::toggled, this, [this]() { updatePreview(); });
    }
    // eixo da cumeeira: o que tem tesouras inclinadas num plano perpendicular a ele
    m_ridge->setCurrentIndex(0);
    if (WindShedService::compute(parameters(), m_bars).rafters == 0) {
        m_ridge->setCurrentIndex(1);
    }
    updatePreview();
}

void WindShedDialog::pickTopography()
{
    TopographicFactorDialog dialog(m_s1->value(), this);
    if (dialog.exec() == QDialog::Accepted) {
        m_s1->setValue(dialog.s1());
    }
}

WindShedService::Parameters WindShedDialog::parameters() const
{
    WindShedService::Parameters p;
    p.site.v0 = m_v0->value();
    p.site.s1 = m_s1->value();
    p.site.s3 = m_s3->value();
    p.site.category = m_category->currentData().toInt();
    p.ridgeAxis = static_cast<WindShedService::RidgeAxis>(m_ridge->currentData().toInt());
    p.reference = static_cast<WindShedService::ReferenceHeight>(m_reference->currentData().toInt());
    p.cpiMode = static_cast<WindShedService::CpiMode>(m_cpiMode->currentData().toInt());
    p.openingRatio = m_ratio->value();
    p.transversePermeable = m_permeable->currentData().toInt() == 1;
    p.cpiFirst = m_cpiFirst->value();
    p.cpiSecond = m_cpiSecond->value();
    p.overhang = m_overhang->value();
    p.transverse = m_transverse->isChecked();
    p.longitudinal = m_longitudinal->isChecked();
    return p;
}

WindShedService::Result WindShedDialog::result() const
{
    return WindShedService::compute(parameters(), m_bars);
}

void WindShedDialog::updatePreview()
{
    const auto mode = static_cast<WindShedService::CpiMode>(m_cpiMode->currentData().toInt());
    m_permeable->setEnabled(mode == WindShedService::CpiMode::TwoOppositeFaces);
    m_ratio->setEnabled(mode == WindShedService::CpiMode::DominantWindward || mode == WindShedService::CpiMode::DominantHighSuction);
    m_cpiFirst->setEnabled(mode == WindShedService::CpiMode::Custom);
    m_cpiSecond->setEnabled(mode == WindShedService::CpiMode::Custom);

    const auto r = result();
    const auto &g = r.geometry;
    QString summary;
    if (r.cases.isEmpty()) {
        summary = r.warnings.isEmpty() ? tr("Nenhum caso a gerar.") : r.warnings.join(QStringLiteral("<br>")).toHtmlEscaped();
    } else {
        summary = tr("<b>%1</b>: a = %2 m, b = %3 m, h = %4 m (h/b = %5), cumeeira a %6 m, θ = %7° — %8 coluna(s) e %9 barra(s) de cobertura.")
                      .arg(g.twoPitch ? tr("Duas águas") : tr("Uma água")).arg(g.a, 0, 'f', 2).arg(g.b, 0, 'f', 2).arg(g.h, 0, 'f', 2).arg(g.h / g.b, 0, 'f', 2)
                      .arg(g.ridgeHeight, 0, 'f', 2).arg(g.theta, 0, 'f', 1).arg(r.columns).arg(r.rafters);
        for (const QString &w : r.warnings) {
            summary += QStringLiteral("<br><span style='color:#a15c00'>%1</span>").arg(w.toHtmlEscaped());
        }
    }
    m_summary->setText(summary);
    m_table->setRowCount(0);
    QStringList notes;
    for (const auto &c : r.cases) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        const QStringList cells = {c.name, QString::number(c.q, 'f', 3), QString(QChar::fromLatin1(c.windClass)), QString::number(c.cpi, 'f', 2), QString::number(c.loads.size())};
        for (int col = 0; col < cells.size(); ++col) {
            m_table->setItem(row, col, new QTableWidgetItem(cells[col]));
        }
        if (row % 2 == 0 || mode == WindShedService::CpiMode::TwoOppositeFaces || mode == WindShedService::CpiMode::DominantWindward) {
            notes << c.notes;
        }
    }
    m_notes->setPlainText(notes.join(QLatin1Char('\n')));
}

} // namespace Structura::UI
