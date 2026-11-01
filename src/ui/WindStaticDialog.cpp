#include "WindStaticDialog.h"

#include "TopographicFactorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace Structura::UI {

using Structura::App::Services::WindStaticService;

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

WindStaticDialog::WindStaticDialog(const std::array<double, 4> &planBounds, const QVector<double> &levels, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Vento estático em edifícios (NBR 6123:2023)"));
    resize(760, 700);
    auto *root = new QVBoxLayout(this);

    auto *params = new QGroupBox(tr("Velocidade característica"), this);
    auto *form = new QFormLayout(params);
    m_v0 = spin(params, 1.0, 100.0, 35.0, 1, tr(" m/s"));
    m_v0->setToolTip(tr("Velocidade básica V0: mapa de isopletas da NBR 6123 (rajada de 3 s, 10 m, categoria II, período de retorno de 50 anos)"));
    m_s1 = spin(params, 0.1, 2.5, 1.0, 3);
    m_s1->setToolTip(tr("Fator topográfico S1 (terreno plano ou fracamente acidentado: 1,0)"));
    m_s3 = spin(params, 0.1, 2.0, 1.0, 2);
    m_s3->setToolTip(tr("Fator estatístico S3 (Tabela 4: 1,00 edificações em geral; 1,10 hospitais/escolas; 0,95 depósitos; 0,88 vedações; 0,83 temporárias)"));
    m_category = new QComboBox(params);
    m_category->addItem(tr("I — superfícies lisas de grandes dimensões (mar calmo, lagos)"), 1);
    m_category->addItem(tr("II — terreno aberto, poucos obstáculos"), 2);
    m_category->addItem(tr("III — terreno plano com obstáculos esparsos (subúrbios, fazendas)"), 3);
    m_category->addItem(tr("IV — obstáculos numerosos e pouco espaçados (zonas urbanizadas, industriais)"), 4);
    m_category->addItem(tr("V — obstáculos grandes, altos e pouco espaçados (centros de grandes cidades)"), 5);
    m_category->setCurrentIndex(2);
    m_neighbourhood = new QCheckBox(tr("Efeitos de vizinhança (excentricidade 0,15 em vez de 0,075 — 6.1.4)"), params);
    m_turbulence = new QCheckBox(tr("Vento de alta turbulência (sotavento com 2/3 do coeficiente — 6.1.3)"), params);
    m_parapet = spin(params, 0.0, 50.0, 0.0, 2, tr(" m"));
    m_parapet->setToolTip(tr("Platibanda ou ático acima do último pavimento: soma-se à altura carregada do topo"));
    form->addRow(tr("Velocidade básica V0"), m_v0);
    {
        auto *s1Row = new QWidget(params);
        auto *s1Layout = new QHBoxLayout(s1Row);
        s1Layout->setContentsMargins(0, 0, 0, 0);
        s1Layout->addWidget(m_s1);
        auto *s1Button = new QPushButton(tr("Calcular S1 topográfico…"), s1Row);
        s1Layout->addWidget(s1Button);
        connect(s1Button, &QPushButton::clicked, this, [this]() {
            TopographicFactorDialog dialog(m_s1->value(), this);
            if (dialog.exec() == QDialog::Accepted) {
                m_s1->setValue(dialog.s1());
            }
        });
        form->addRow(tr("Fator topográfico S1"), s1Row);
    }
    form->addRow(tr("Fator estatístico S3"), m_s3);
    form->addRow(tr("Categoria de rugosidade"), m_category);
    form->addRow(tr("Platibanda acima do topo"), m_parapet);
    form->addRow(QString(), m_neighbourhood);
    form->addRow(QString(), m_turbulence);
    root->addWidget(params);

    auto *plan = new QGroupBox(tr("Edificação"), this);
    auto *planForm = new QFormLayout(plan);
    const QString labels[4] = {tr("X mínimo"), tr("X máximo"), tr("Y mínimo"), tr("Y máximo")};
    auto *planRow = new QGridLayout;
    for (int i = 0; i < 4; ++i) {
        m_plan[i] = spin(plan, -1e6, 1e6, planBounds[static_cast<std::size_t>(i)], 3, tr(" m"));
        planRow->addWidget(new QLabel(labels[i], plan), i / 2, (i % 2) * 2);
        planRow->addWidget(m_plan[i], i / 2, (i % 2) * 2 + 1);
    }
    planForm->addRow(tr("Planta (retângulo envolvente)"), planRow);
    m_levels = new QLineEdit(plan);
    QStringList texts;
    for (const double z : levels) {
        texts << QString::number(z, 'f', 3).remove(QRegularExpression(QStringLiteral("\\.?0+$")));
    }
    m_levels->setText(texts.join(QStringLiteral("; ")));
    m_levels->setToolTip(tr("Cotas dos pavimentos separadas por ponto e vírgula; a primeira é a base (apoios). A força de cada pavimento atua na sua cota."));
    planForm->addRow(tr("Cotas (m): base; pav. 1; pav. 2; …"), m_levels);
    root->addWidget(plan);

    auto *cases = new QGroupBox(tr("Casos de carga a gerar"), this);
    auto *casesLayout = new QGridLayout(cases);
    const QString directionNames[4] = {tr("Vento +X"), tr("Vento −X"), tr("Vento +Y"), tr("Vento −Y")};
    for (int i = 0; i < 4; ++i) {
        m_directions[i] = new QCheckBox(directionNames[i], cases);
        m_directions[i]->setChecked(true);
        casesLayout->addWidget(m_directions[i], 0, i);
    }
    m_eccentricity = new QComboBox(cases);
    m_eccentricity->addItem(tr("Só a resultante centrada"), 0);
    m_eccentricity->addItem(tr("Centrada e com excentricidade ±e (6.1.4)"), 1);
    m_eccentricity->addItem(tr("Só com excentricidade ±e"), 2);
    m_eccentricity->setCurrentIndex(1);
    casesLayout->addWidget(new QLabel(tr("Excentricidade da força de arrasto"), cases), 1, 0, 1, 2);
    casesLayout->addWidget(m_eccentricity, 1, 2, 1, 2);
    root->addWidget(cases);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    root->addWidget(m_summary);
    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({tr("Caso"), tr("Cota (m)"), tr("S2"), tr("q (kN/m²)"), tr("Altura carregada (m)"), tr("Força (kN)")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(m_table, 1);

    auto *note = new QLabel(tr("As forças saem dos coeficientes de forma externos da Tabela 6 (parede de barlavento e de sotavento) e atuam nas cotas dos pavimentos: no nó mestre do diafragma "
                               "rígido do pavimento, com o momento da excentricidade, ou, sem diafragma, repartidas pelos nós do nível. Cada combinação das cargas de vento com as demais "
                               "(permanentes, sobrecarga) é feita em Casos e combinações. O vento oblíquo e os efeitos de vizinhança nos coeficientes não são tabelados pela norma."), this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #5a6472;"));
    root->addWidget(note);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(tr("Gerar casos de carga"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);

    auto refresh = [this]() { updatePreview(); };
    for (auto *s : {m_v0, m_s1, m_s3, m_parapet, m_plan[0], m_plan[1], m_plan[2], m_plan[3]}) {
        connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, refresh);
    }
    for (auto *c : {m_neighbourhood, m_turbulence, m_directions[0], m_directions[1], m_directions[2], m_directions[3]}) {
        connect(c, &QCheckBox::toggled, this, refresh);
    }
    connect(m_category, qOverload<int>(&QComboBox::currentIndexChanged), this, refresh);
    connect(m_eccentricity, qOverload<int>(&QComboBox::currentIndexChanged), this, refresh);
    connect(m_levels, &QLineEdit::textChanged, this, refresh);
    updatePreview();
}

QVector<double> WindStaticDialog::parseLevels(bool *ok) const
{
    QVector<double> values;
    bool all = true;
    for (const QString &token : m_levels->text().split(QRegularExpression(QStringLiteral("[;\\s]+")), Qt::SkipEmptyParts)) {
        bool good = false;
        const double v = QString(token).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&good);
        all = all && good;
        if (good) {
            values.append(v);
        }
    }
    std::sort(values.begin(), values.end());
    if (ok) {
        *ok = all;
    }
    return values;
}

WindStaticService::Parameters WindStaticDialog::parameters() const
{
    WindStaticService::Parameters p;
    p.v0 = m_v0->value();
    p.s1 = m_s1->value();
    p.s3 = m_s3->value();
    p.category = m_category->currentData().toInt();
    p.neighbourhood = m_neighbourhood->isChecked();
    p.highTurbulence = m_turbulence->isChecked();
    p.parapet = m_parapet->value();
    return p;
}

WindStaticService::Building WindStaticDialog::building() const
{
    WindStaticService::Building b;
    b.xMin = m_plan[0]->value();
    b.xMax = m_plan[1]->value();
    b.yMin = m_plan[2]->value();
    b.yMax = m_plan[3]->value();
    b.levels = parseLevels(nullptr);
    return b;
}

int WindStaticDialog::directions() const
{
    int mask = 0;
    for (int i = 0; i < 4; ++i) {
        mask |= m_directions[i]->isChecked() ? (1 << i) : 0;
    }
    return mask;
}

int WindStaticDialog::eccentricityMode() const
{
    return m_eccentricity->currentData().toInt();
}

WindStaticService::Result WindStaticDialog::result() const
{
    return WindStaticService::compute(parameters(), building(), directions(), eccentricityMode());
}

void WindStaticDialog::updatePreview()
{
    bool ok = true;
    parseLevels(&ok);
    const auto r = result();
    QString summary;
    if (!ok) {
        summary = tr("<b>Cotas inválidas</b>: use números separados por ponto e vírgula.");
    } else if (r.cases.isEmpty()) {
        summary = r.warnings.isEmpty() ? tr("Nenhum caso a gerar (marque ao menos uma direção).") : r.warnings.join(QStringLiteral("<br>"));
    } else {
        summary = tr("a = %1 m, b = %2 m, h = %3 m (h/b = %4, a/b = %5). ").arg(r.a, 0, 'f', 2).arg(r.b, 0, 'f', 2).arg(r.h, 0, 'f', 2).arg(r.h / r.b, 0, 'f', 2).arg(r.a / r.b, 0, 'f', 2);
        QStringList lines;
        QSet<QString> seen;
        for (const auto &c : r.cases) {
            const QString key = QStringLiteral("%1").arg(static_cast<int>(c.direction));
            if (c.eccentricity != 0 || seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            static const char *names[4] = {"+X", "-X", "+Y", "-Y"};
            lines << tr("%1: α = %2°, classe %3, Ce barlavento %4, sotavento %5 (largura %6 m)")
                         .arg(QLatin1String(names[c.direction])).arg(c.alpha, 0, 'f', 0).arg(QLatin1Char(c.windClass))
                         .arg(c.windwardCoefficient, 0, 'f', 2).arg(c.leewardCoefficient, 0, 'f', 2).arg(c.faceWidth, 0, 'f', 2);
        }
        summary += lines.join(QStringLiteral("; "));
        for (const QString &w : r.warnings) {
            summary += QStringLiteral("<br><span style='color:#a15c00'>%1</span>").arg(w.toHtmlEscaped());
        }
    }
    m_summary->setText(summary);
    m_table->setRowCount(0);
    for (const auto &c : r.cases) {
        for (const auto &f : c.floors) {
            const int row = m_table->rowCount();
            m_table->insertRow(row);
            const QStringList cells = {c.name, QString::number(f.z, 'f', 2), QString::number(f.s2, 'f', 3), QString::number(f.q, 'f', 3), QString::number(f.tributary, 'f', 2),
                                       QString::number(f.force, 'f', 2)};
            for (int col = 0; col < cells.size(); ++col) {
                m_table->setItem(row, col, new QTableWidgetItem(cells[col]));
            }
        }
    }
}

} // namespace Structura::UI
