#include "TemplateDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::ModelTemplateService;

namespace {

QDoubleSpinBox *dspin(double min, double max, double value, double step, const QString &suffix, int decimals = 2)
{
    auto *s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setValue(value);
    s->setSingleStep(step);
    s->setDecimals(decimals);
    s->setSuffix(suffix);
    return s;
}

QSpinBox *ispin(int min, int max, int value)
{
    auto *s = new QSpinBox;
    s->setRange(min, max);
    s->setValue(value);
    return s;
}

QComboBox *profileCombo(const QString &defaultName)
{
    auto *c = new QComboBox;
    c->addItems(ModelTemplateService::catalogNames(QStringLiteral("W Laminado")));
    c->setEditable(true);
    c->setCurrentText(defaultName);
    return c;
}

} // namespace

TemplateDialog::TemplateDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Modelo pronto"));
    setMinimumWidth(520);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Gera um modelo paramétrico completo (nós, barras, perfis, grupos, travamentos em tirante, casos de carga e combinações). "
                                "O modelo atual será substituído; use depois as ferramentas normais para ajustar, carregar o vento e dimensionar."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_kind = new QComboBox;
    m_kind->addItem(tr("Galpão de duas águas (pórticos de alma cheia, terças e contraventamento)"), 0);
    m_kind->addItem(tr("Edifício de pórticos (pilares, vigas em X e Y, contraventamento)"), 1);
    root->addWidget(m_kind);
    m_stack = new QStackedWidget;
    root->addWidget(m_stack, 1);

    // ---- galpao -------------------------------------------------------------------------------------------
    auto *shed = new QWidget;
    auto *sf = new QFormLayout(shed);
    m_span = dspin(4, 100, 20, 1, tr(" m"));
    m_eave = dspin(2, 30, 6, 0.5, tr(" m"));
    m_slope = dspin(2, 60, 10, 1, tr(" %"));
    m_frames = ispin(2, 40, 6);
    m_frameSpacing = dspin(2, 20, 6, 0.5, tr(" m"));
    m_panels = ispin(2, 20, 6);
    m_shedColumn = profileCombo(QStringLiteral("W 410 x 53.0"));
    m_shedRafter = profileCombo(QStringLiteral("W 360 x 44.6"));
    m_pinned = new QCheckBox(tr("Bases dos pilares articuladas"));
    m_roofBracing = new QCheckBox(tr("Contraventamento em X da cobertura (tirantes) nos vãos extremos"));
    m_roofBracing->setChecked(true);
    m_roofLive = dspin(0, 10, 0.25, 0.05, tr(" kN/m²"));
    m_roofDead = dspin(0, 10, 0.15, 0.05, tr(" kN/m²"));
    sf->addRow(tr("Vão"), m_span);
    sf->addRow(tr("Altura dos pilares"), m_eave);
    sf->addRow(tr("Inclinação da cobertura"), m_slope);
    sf->addRow(tr("Número de pórticos"), m_frames);
    sf->addRow(tr("Espaçamento entre pórticos"), m_frameSpacing);
    sf->addRow(tr("Trechos por água (terças nos nós)"), m_panels);
    sf->addRow(tr("Perfil dos pilares"), m_shedColumn);
    sf->addRow(tr("Perfil das vigas de cobertura"), m_shedRafter);
    sf->addRow(m_pinned);
    sf->addRow(m_roofBracing);
    sf->addRow(tr("Sobrecarga da cobertura (em planta)"), m_roofLive);
    sf->addRow(tr("Carga permanente da cobertura"), m_roofDead);
    m_stack->addWidget(shed);

    // ---- edificio -----------------------------------------------------------------------------------------
    auto *building = new QWidget;
    auto *bf = new QFormLayout(building);
    m_baysX = ispin(1, 20, 2);
    m_baysY = ispin(1, 30, 4);
    m_stories = ispin(1, 40, 3);
    m_bayX = dspin(2, 20, 6, 0.5, tr(" m"));
    m_bayY = dspin(2, 20, 6, 0.5, tr(" m"));
    m_storyHeight = dspin(2, 8, 3.6, 0.1, tr(" m"));
    m_column = profileCombo(QStringLiteral("W 310 x 74.0 (H)"));
    m_beamX = profileCombo(QStringLiteral("W 410 x 53.0"));
    m_beamY = profileCombo(QStringLiteral("W 310 x 32.7"));
    m_bracing = new QCheckBox(tr("Contraventamento em X (tirantes) nos vãos extremos das fachadas"));
    m_bracing->setChecked(true);
    m_floorDead = dspin(0, 30, 4, 0.5, tr(" kN/m²"));
    m_floorLive = dspin(0, 30, 3, 0.5, tr(" kN/m²"));
    bf->addRow(tr("Vãos em X"), m_baysX);
    bf->addRow(tr("Vãos em Y"), m_baysY);
    bf->addRow(tr("Andares"), m_stories);
    bf->addRow(tr("Vão em X"), m_bayX);
    bf->addRow(tr("Vão em Y"), m_bayY);
    bf->addRow(tr("Altura do andar"), m_storyHeight);
    bf->addRow(tr("Perfil dos pilares"), m_column);
    bf->addRow(tr("Perfil das vigas em X"), m_beamX);
    bf->addRow(tr("Perfil das vigas em Y"), m_beamY);
    bf->addRow(m_bracing);
    bf->addRow(tr("Carga permanente dos pisos"), m_floorDead);
    bf->addRow(tr("Sobrecarga dos pisos"), m_floorLive);
    m_stack->addWidget(building);

    connect(m_kind, &QComboBox::currentIndexChanged, m_stack, &QStackedWidget::setCurrentIndex);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto *ok = buttons->addButton(tr("Gerar modelo"), QDialogButtonBox::AcceptRole);
    ok->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

QString TemplateDialog::suggestedName() const
{
    return m_kind->currentIndex() == 0 ? tr("Galpão modelo") : tr("Edifício modelo");
}

QString TemplateDialog::generate(QString *error) const
{
    if (m_kind->currentIndex() == 0) {
        ModelTemplateService::ShedParams p;
        p.span = m_span->value();
        p.eaveHeight = m_eave->value();
        p.slopePercent = m_slope->value();
        p.frames = m_frames->value();
        p.frameSpacing = m_frameSpacing->value();
        p.panelsPerSlope = m_panels->value();
        p.columnProfile = m_shedColumn->currentText();
        p.rafterProfile = m_shedRafter->currentText();
        p.pinnedBases = m_pinned->isChecked();
        p.roofBracing = m_roofBracing->isChecked();
        p.roofLiveLoad = m_roofLive->value();
        p.roofDeadLoad = m_roofDead->value();
        return ModelTemplateService::buildShed(p, error);
    }
    ModelTemplateService::BuildingParams p;
    p.baysX = m_baysX->value();
    p.baysY = m_baysY->value();
    p.stories = m_stories->value();
    p.bayX = m_bayX->value();
    p.bayY = m_bayY->value();
    p.storyHeight = m_storyHeight->value();
    p.columnProfile = m_column->currentText();
    p.beamXProfile = m_beamX->currentText();
    p.beamYProfile = m_beamY->currentText();
    p.bracing = m_bracing->isChecked();
    p.floorDeadLoad = m_floorDead->value();
    p.floorLiveLoad = m_floorLive->value();
    return ModelTemplateService::buildBuilding(p, error);
}

} // namespace Structura::UI
