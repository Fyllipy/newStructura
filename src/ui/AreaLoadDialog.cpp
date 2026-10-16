#include "AreaLoadDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::AreaLoadService;

namespace {

QDoubleSpinBox *spin(double min, double max, double value, int decimals, double step, const QString &suffix = {})
{
    auto *box = new QDoubleSpinBox;
    box->setRange(min, max);
    box->setDecimals(decimals);
    box->setSingleStep(step);
    box->setValue(value);
    if (!suffix.isEmpty()) {
        box->setSuffix(suffix);
    }
    return box;
}

} // namespace

AreaLoadDialog::AreaLoadDialog(Mode mode, const QStringList &caseNames, const QString &defaultCase, double structureHeight,
                               int selectedBars, QWidget *parent)
    : QDialog(parent)
    , m_mode(mode)
{
    const bool wind = mode == Mode::Wind;
    setWindowTitle(wind ? tr("Vento (NBR 6123)") : tr("Carga de área"));
    setMinimumWidth(560);
    resize(600, wind ? 760 : 520);

    auto *outer = new QVBoxLayout(this);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
    auto *root = new QVBoxLayout(content);

    auto *intro = new QLabel(wind ? tr("Gera cargas distribuídas nas barras selecionadas (terças, travessas ou vigas de uma face) a partir da pressão do vento "
                                       "q = 0,613 Vk² e do coeficiente de pressão da face. Selecione as barras de UMA face por vez.")
                                  : tr("Distribui uma pressão (kN/m²) nas barras selecionadas, usando a largura de influência de cada uma "
                                       "(metade da distância às barras paralelas vizinhas). Selecione apenas as barras que recebem a carga."));
    intro->setWordWrap(true);
    root->addWidget(intro);
    auto *selection = new QLabel(tr("Barras selecionadas: %1").arg(selectedBars));
    selection->setStyleSheet(QStringLiteral("color: #5a6472;"));
    root->addWidget(selection);

    if (wind) {
        auto *params = new QGroupBox(tr("Velocidade característica"));
        auto *form = new QFormLayout(params);
        m_v0 = spin(10, 80, 35, 1, 1, tr(" m/s"));
        m_s1 = spin(0.5, 1.5, 1.0, 2, 0.05);
        m_s3 = spin(0.5, 1.5, 1.0, 2, 0.05);
        m_category = new QComboBox;
        m_category->addItems({tr("I — superfície lisa, grandes dimensões (mar, lagos)"), tr("II — terreno aberto, poucos obstáculos"),
                              tr("III — terreno plano ou ondulado, obstáculos isolados"), tr("IV — terreno com obstáculos numerosos (subúrbios)"),
                              tr("V — obstáculos numerosos, grandes e altos (centros urbanos)")});
        m_category->setCurrentIndex(1);
        m_class = new QComboBox;
        m_class->addItems({tr("A — maior dimensão até 20 m"), tr("B — maior dimensão de 20 a 50 m"), tr("C — maior dimensão acima de 50 m")});
        m_class->setCurrentIndex(1);
        m_height = spin(1, 500, std::max(1.0, structureHeight), 1, 1, tr(" m"));
        form->addRow(tr("V0 — velocidade básica (isopletas)"), m_v0);
        form->addRow(tr("S1 — fator topográfico"), m_s1);
        form->addRow(tr("S3 — fator estatístico"), m_s3);
        form->addRow(tr("Categoria de rugosidade (S2)"), m_category);
        form->addRow(tr("Classe da edificação (S2)"), m_class);
        form->addRow(tr("Altura de referência z"), m_height);
        m_windResult = new QLabel;
        m_windResult->setStyleSheet(QStringLiteral("font-weight: 600; color: #0b6fa3;"));
        form->addRow(m_windResult);
        auto *note = new QLabel(tr("Os parâmetros meteorológicos (b, p, Fr) seguem a tabela da NBR 6123:1988. Confira com a edição vigente da norma "
                                   "(a revisão de 2023 pode alterar valores) e ajuste V0, S1 e S3 ao local."));
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral("color: #7a4b00; background: #fff4e0; border: 1px solid #f0c987; border-radius: 4px; padding: 6px;"));
        form->addRow(note);
        root->addWidget(params);

        auto *face = new QGroupBox(tr("Face e coeficientes de pressão"));
        auto *faceForm = new QFormLayout(face);
        m_windDirection = new QComboBox;
        m_windDirection->addItems({tr("Vento em +X"), tr("Vento em −X"), tr("Vento em +Y"), tr("Vento em −Y")});
        m_facePreset = new QComboBox;
        m_facePreset->addItems({tr("Parede a barlavento (Ce ≈ +0,7)"), tr("Parede a sotavento (Ce ≈ −0,5)"), tr("Paredes laterais (Ce ≈ −0,8)"),
                                tr("Cobertura — sucção (Ce ≈ −0,8)"), tr("Personalizado")});
        m_cpe = spin(-3, 3, 0.7, 2, 0.05);
        m_ci = spin(-1, 1, 0.0, 2, 0.05);
        m_ci->setToolTip(tr("Coeficiente de pressão interna: +0,2 ou −0,3 conforme as aberturas (NBR 6123, 6.2)"));
        faceForm->addRow(tr("Direção do vento"), m_windDirection);
        faceForm->addRow(tr("Face (sugestão de Ce)"), m_facePreset);
        faceForm->addRow(tr("Ce — coeficiente de pressão externa"), m_cpe);
        faceForm->addRow(tr("Ci — coeficiente de pressão interna"), m_ci);
        auto *sign = new QLabel(tr("Convenção: coeficiente > 0 é sobrepressão (empurra a face para dentro); < 0 é sucção (puxa para fora). "
                                   "Os valores sugeridos são típicos; use as tabelas de Ce da NBR 6123 para a geometria real."));
        sign->setWordWrap(true);
        sign->setStyleSheet(QStringLiteral("color: #5a6472;"));
        faceForm->addRow(sign);
        root->addWidget(face);

        connect(m_facePreset, &QComboBox::currentIndexChanged, this, &AreaLoadDialog::applyFacePreset);
        for (auto *w : {m_v0, m_s1, m_s3, m_height, m_cpe, m_ci}) {
            connect(w, &QDoubleSpinBox::valueChanged, this, [this](double) { updateWindLabels(); });
        }
        connect(m_category, &QComboBox::currentIndexChanged, this, [this](int) { updateWindLabels(); });
        connect(m_class, &QComboBox::currentIndexChanged, this, [this](int) { updateWindLabels(); });
    } else {
        auto *load = new QGroupBox(tr("Pressão"));
        auto *form = new QFormLayout(load);
        m_pressure = spin(-1e4, 1e4, 1.0, 3, 0.1, tr(" kN/m²"));
        m_direction = new QComboBox;
        m_direction->addItem(tr("−Z (gravidade)"), 0);
        m_direction->addItem(tr("+Z"), 1);
        m_direction->addItem(tr("+X"), 2);
        m_direction->addItem(tr("−X"), 3);
        m_direction->addItem(tr("+Y"), 4);
        m_direction->addItem(tr("−Y"), 5);
        m_direction->addItem(tr("Normal à superfície, para dentro da estrutura"), 6);
        m_direction->addItem(tr("Normal à superfície, para fora da estrutura"), 7);
        m_projected = new QCheckBox(tr("Pressão sobre a projeção ortogonal à direção (ex.: sobrecarga em telhado inclinado)"));
        form->addRow(tr("Pressão"), m_pressure);
        form->addRow(tr("Direção da força"), m_direction);
        form->addRow(m_projected);
        root->addWidget(load);
    }

    auto *distribution = new QGroupBox(tr("Distribuição nas barras"));
    auto *dform = new QFormLayout(distribution);
    m_manualWidth = spin(0.0, 1000.0, 0.0, 3, 0.25, tr(" m"));
    m_manualWidth->setSpecialValueText(tr("automática"));
    m_manualWidth->setToolTip(tr("Largura de influência fixa para todas as barras; automática usa a distância entre barras paralelas"));
    m_overhang = spin(0.0, 100.0, 0.0, 3, 0.25, tr(" m"));
    m_overhang->setToolTip(tr("Prolongamento da superfície além das barras das extremidades (beiral)"));
    m_case = new QComboBox;
    m_case->setEditable(true);
    m_case->addItems(caseNames);
    if (wind) {
        m_case->setCurrentText(tr("Vento"));
    } else {
        const int index = m_case->findText(defaultCase);
        m_case->setCurrentIndex(std::max(0, index));
    }
    m_case->setToolTip(tr("Caso de carga; digite um nome novo para criar o caso"));
    dform->addRow(tr("Largura de influência"), m_manualWidth);
    dform->addRow(tr("Prolongamento nas extremidades"), m_overhang);
    dform->addRow(tr("Caso de carga"), m_case);
    root->addWidget(distribution);
    root->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    if (wind) {
        updateWindLabels();
    }
}

void AreaLoadDialog::applyFacePreset(int index)
{
    static const double ce[4] = {0.7, -0.5, -0.8, -0.8};
    if (index >= 0 && index < 4) {
        m_cpe->setValue(ce[index]);
    }
}

void AreaLoadDialog::updateWindLabels()
{
    AreaLoadService::WindParameters w;
    w.v0 = m_v0->value();
    w.s1 = m_s1->value();
    w.s3 = m_s3->value();
    w.category = m_category->currentIndex() + 1;
    w.cls = static_cast<char>('A' + m_class->currentIndex());
    w.height = m_height->value();
    const double q = AreaLoadService::dynamicPressure(w);
    const double net = m_cpe->value() - m_ci->value();
    m_windResult->setText(tr("S2 = %1   Vk = %2 m/s   q = %3 kN/m²   →   pressão na face = q (Ce − Ci) = %4 kN/m²")
                              .arg(AreaLoadService::s2(w), 0, 'f', 3)
                              .arg(AreaLoadService::characteristicSpeed(w), 0, 'f', 1)
                              .arg(q, 0, 'f', 3)
                              .arg(q * net, 0, 'f', 3));
}

AreaLoadRequest AreaLoadDialog::request() const
{
    AreaLoadRequest r;
    r.caseName = m_case->currentText().trimmed();
    r.manualWidth = m_manualWidth->value();
    r.overhang = m_overhang->value();
    if (m_mode == Mode::Wind) {
        AreaLoadService::WindParameters w;
        w.v0 = m_v0->value();
        w.s1 = m_s1->value();
        w.s3 = m_s3->value();
        w.category = m_category->currentIndex() + 1;
        w.cls = static_cast<char>('A' + m_class->currentIndex());
        w.height = m_height->value();
        r.wind = true;
        r.pressure = AreaLoadService::dynamicPressure(w) * (m_cpe->value() - m_ci->value());
        r.directionKind = 2;   // pressao positiva empurra a face para dentro
        r.description = tr("%1: q = %2 kN/m², Ce = %3, Ci = %4")
                            .arg(m_windDirection->currentText())
                            .arg(AreaLoadService::dynamicPressure(w), 0, 'f', 3)
                            .arg(m_cpe->value(), 0, 'f', 2)
                            .arg(m_ci->value(), 0, 'f', 2);
        return r;
    }
    r.pressure = m_pressure->value();
    r.projected = m_projected->isChecked();
    switch (m_direction->currentData().toInt()) {
    case 0: r.direction = {0, 0, -1}; break;
    case 1: r.direction = {0, 0, 1}; break;
    case 2: r.direction = {1, 0, 0}; break;
    case 3: r.direction = {-1, 0, 0}; break;
    case 4: r.direction = {0, 1, 0}; break;
    case 5: r.direction = {0, -1, 0}; break;
    case 6: r.directionKind = 2; break;
    default: r.directionKind = 1; break;
    }
    r.description = tr("pressão %1 kN/m²").arg(r.pressure, 0, 'f', 3);
    return r;
}

} // namespace Structura::UI
