#include "TransformDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Structura::UI {

namespace {

QDoubleSpinBox *distanceSpin(QWidget *parent)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(-1e6, 1e6);
    spin->setDecimals(3);
    spin->setSingleStep(0.5);
    spin->setSuffix(QStringLiteral(" m"));
    return spin;
}

void addButtons(QDialog *dialog, QVBoxLayout *layout)
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttons);
}

} // namespace

SerialCopyDialog::SerialCopyDialog(QWidget *parent)
    : QDialog(parent)
    , m_count(new QSpinBox(this))
    , m_dx(distanceSpin(this))
    , m_dy(distanceSpin(this))
    , m_dz(distanceSpin(this))
{
    setWindowTitle(tr("Copia em serie"));
    setModal(true);
    m_count->setRange(1, 500);
    m_count->setValue(3);
    m_dz->setValue(3.0);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Cria N copias dos elementos selecionados, cada uma deslocada\n"
                                    "do passo informado em relacao a anterior."), this));
    auto *form = new QFormLayout();
    form->addRow(tr("Numero de copias"), m_count);
    form->addRow(tr("Passo em X"), m_dx);
    form->addRow(tr("Passo em Y"), m_dy);
    form->addRow(tr("Passo em Z"), m_dz);
    layout->addLayout(form);
    addButtons(this, layout);
}

int SerialCopyDialog::count() const { return m_count->value(); }
double SerialCopyDialog::dx() const { return m_dx->value(); }
double SerialCopyDialog::dy() const { return m_dy->value(); }
double SerialCopyDialog::dz() const { return m_dz->value(); }

ArrayCopyDialog::ArrayCopyDialog(QWidget *parent)
    : QDialog(parent)
    , m_axis1(new QComboBox(this))
    , m_axis2(new QComboBox(this))
    , m_spacing1(distanceSpin(this))
    , m_spacing2(distanceSpin(this))
    , m_count1(new QSpinBox(this))
    , m_count2(new QSpinBox(this))
    , m_summary(new QLabel(this))
{
    setWindowTitle(tr("Copia em matriz"));
    setModal(true);
    for (QComboBox *axis : {m_axis1, m_axis2}) {
        axis->addItem(tr("X"), 0);
        axis->addItem(tr("Y"), 1);
        axis->addItem(tr("Z"), 2);
    }
    m_axis2->setCurrentIndex(1);
    for (QSpinBox *count : {m_count1, m_count2}) {
        count->setRange(0, 500);
    }
    m_count1->setValue(3);
    m_count2->setValue(2);
    for (QDoubleSpinBox *spacing : {m_spacing1, m_spacing2}) {
        spacing->setValue(3.0);
        spacing->setToolTip(tr("Distancia entre copias consecutivas (entre eixos). Valor negativo copia no sentido oposto do eixo."));
    }
    m_summary->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Copia os elementos selecionados em uma grade nas duas direcoes: cada direcao tem um eixo global,\n"
                                    "um espacamento (entre eixos) e o numero de copias adicionais. Informe 0 copias para desativar uma direcao."), this));
    auto *form = new QFormLayout();
    form->addRow(tr("Direcao 1: eixo"), m_axis1);
    form->addRow(tr("Direcao 1: espacamento"), m_spacing1);
    form->addRow(tr("Direcao 1: numero de copias"), m_count1);
    form->addRow(tr("Direcao 2: eixo"), m_axis2);
    form->addRow(tr("Direcao 2: espacamento"), m_spacing2);
    form->addRow(tr("Direcao 2: numero de copias"), m_count2);
    layout->addLayout(form);
    layout->addWidget(m_summary);
    addButtons(this, layout);

    const auto changed = [this]() { refresh(); };
    connect(m_axis1, &QComboBox::currentIndexChanged, this, changed);
    connect(m_axis2, &QComboBox::currentIndexChanged, this, changed);
    connect(m_count1, &QSpinBox::valueChanged, this, changed);
    connect(m_count2, &QSpinBox::valueChanged, this, changed);
    connect(m_spacing1, &QDoubleSpinBox::valueChanged, this, changed);
    connect(m_spacing2, &QDoubleSpinBox::valueChanged, this, changed);
    refresh();
}

int ArrayCopyDialog::axis1() const { return m_axis1->currentData().toInt(); }
int ArrayCopyDialog::axis2() const { return m_axis2->currentData().toInt(); }
int ArrayCopyDialog::count1() const { return m_count1->value(); }
int ArrayCopyDialog::count2() const { return m_count2->value(); }

std::array<double, 3> ArrayCopyDialog::offset(int i, int j) const
{
    std::array<double, 3> d {0.0, 0.0, 0.0};
    d[static_cast<std::size_t>(axis1())] += m_spacing1->value() * i;
    d[static_cast<std::size_t>(axis2())] += m_spacing2->value() * j;
    return d;
}

QString ArrayCopyDialog::validationError() const
{
    if (count1() == 0 && count2() == 0) {
        return tr("Informe ao menos uma copia em uma das direcoes.");
    }
    if (count1() > 0 && m_spacing1->value() == 0.0) {
        return tr("O espacamento da direcao 1 deve ser diferente de zero.");
    }
    if (count2() > 0 && m_spacing2->value() == 0.0) {
        return tr("O espacamento da direcao 2 deve ser diferente de zero.");
    }
    if (count1() > 0 && count2() > 0 && axis1() == axis2()) {
        return tr("As duas direcoes devem usar eixos diferentes (senao as copias se sobrepoem).");
    }
    return {};
}

void ArrayCopyDialog::refresh()
{
    const QString error = validationError();
    if (!error.isEmpty()) {
        m_summary->setText(error);
        m_summary->setStyleSheet(QStringLiteral("color:#b3261e;"));
        return;
    }
    m_summary->setStyleSheet(QString());
    const auto last = offset(count1(), count2());
    m_summary->setText(tr("Grade de %1 x %2 posicoes (%3 copia(s) novas, alem do original). Canto oposto: dX = %4 m, dY = %5 m, dZ = %6 m.")
                           .arg(count1() + 1).arg(count2() + 1).arg(newPositions())
                           .arg(last[0], 0, 'f', 3).arg(last[1], 0, 'f', 3).arg(last[2], 0, 'f', 3));
}

RotateDialog::RotateDialog(QWidget *parent)
    : QDialog(parent)
    , m_angle(new QDoubleSpinBox(this))
    , m_copy(new QCheckBox(tr("Manter os originais (rotacionar uma copia)"), this))
{
    setWindowTitle(tr("Rotacionar elementos"));
    setModal(true);
    m_angle->setRange(-360.0, 360.0);
    m_angle->setDecimals(3);
    m_angle->setValue(90.0);
    m_angle->setSuffix(QStringLiteral(" °"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Rotacao em torno do eixo normal ao plano de construcao ativo,\n"
                                    "passando pelo ponto clicado (sentido anti-horario positivo)."), this));
    auto *form = new QFormLayout();
    form->addRow(tr("Angulo"), m_angle);
    layout->addLayout(form);
    layout->addWidget(m_copy);
    addButtons(this, layout);
}

double RotateDialog::angleDegrees() const { return m_angle->value(); }
bool RotateDialog::copy() const { return m_copy->isChecked(); }

MirrorDialog::MirrorDialog(QWidget *parent)
    : QDialog(parent)
    , m_keep(new QCheckBox(tr("Manter os elementos originais (espelhar uma copia)"), this))
{
    setWindowTitle(tr("Espelhar elementos"));
    setModal(true);
    m_keep->setChecked(true);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Espelha em relacao a reta definida pelos dois pontos clicados,\n"
                                    "no plano de construcao ativo."), this));
    layout->addWidget(m_keep);
    addButtons(this, layout);
}

bool MirrorDialog::keepOriginals() const { return m_keep->isChecked(); }

} // namespace Structura::UI
