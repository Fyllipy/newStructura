#include "TransformDialogs.h"

#include <QCheckBox>
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
