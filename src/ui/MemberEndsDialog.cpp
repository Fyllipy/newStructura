#include "MemberEndsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

namespace Structura::UI {

namespace PD = Structura::App::ProjectData;

namespace {

QDoubleSpinBox *makeSpin(QWidget *parent, double max, int decimals, double value)
{
    auto *s = new QDoubleSpinBox(parent);
    s->setRange(0.0, max);
    s->setDecimals(decimals);
    s->setValue(value);
    s->setSingleStep(max > 10.0 ? 100.0 : 0.01);
    s->setAccelerated(true);
    return s;
}

} // namespace

MemberEndsDialog::MemberEndsDialog(const PD::MemberConnectionSpec &initial, const Defaults &defaults, int selectedCount, QWidget *parent)
    : QDialog(parent)
    , m_defaults(defaults)
{
    setWindowTitle(tr("Ligações de extremidade e rótulas plásticas"));
    auto *root = new QVBoxLayout(this);
    auto *selection = new QLabel(selectedCount == 1 ? tr("1 barra selecionada") : tr("%1 barras selecionadas (os valores abaixo substituem os de todas)").arg(selectedCount), this);
    selection->setStyleSheet(QStringLiteral("font-weight: 500; color: #1f2530;"));
    root->addWidget(selection);

    const QString axisNames[3] = {tr("Torção (rx)"), tr("Flexão em torno de y local (ry)"), tr("Flexão em torno de z local (rz)")};
    for (int end = 0; end < 2; ++end) {
        auto *group = new QGroupBox(end == 0 ? tr("Extremidade I (início da barra)") : tr("Extremidade J (fim da barra)"), this);
        auto *grid = new QGridLayout(group);
        grid->addWidget(new QLabel(tr("<b>Ligação</b>"), group), 0, 0);
        grid->addWidget(new QLabel(tr("<b>Rigidez rotacional (kN·m/rad)</b>"), group), 0, 2);
        for (int axis = 0; axis < 3; ++axis) {
            Row &row = m_rows[end][axis];
            row.mode = new QComboBox(group);
            row.mode->addItem(tr("Contínua (rígida)"), 0);
            row.mode->addItem(tr("Semirrígida (mola)"), 1);
            row.stiffness = makeSpin(group, 1e12, 3, 0.0);
            row.stiffness->setEnabled(false);
            connect(row.mode, qOverload<int>(&QComboBox::currentIndexChanged), this, [&row](int index) { row.stiffness->setEnabled(index == 1); });
            const double k = initial.springK[static_cast<std::size_t>(end * 6 + 3 + axis)];
            if (k > 0.0) {
                row.mode->setCurrentIndex(1);
                row.stiffness->setValue(k);
            }
            grid->addWidget(new QLabel(axisNames[axis], group), axis + 1, 0);
            grid->addWidget(row.mode, axis + 1, 1);
            grid->addWidget(row.stiffness, axis + 1, 2);
        }
        root->addWidget(group);
    }

    auto *plastic = new QGroupBox(tr("Rótulas plásticas (análise não linear incremental)"), this);
    auto *pGrid = new QGridLayout(plastic);
    const QStringList heads = {tr("Rótula"), tr("Mp (kN·m)"), tr("k elástico (kN·m/rad)"), tr("Enrijecimento"), tr("Ny (kN, 0 = sem N-M)"), tr("Expoente N-M")};
    for (int c = 0; c < heads.size(); ++c) {
        pGrid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(heads[c]), plastic), 0, c);
    }
    int line = 1;
    for (int end = 0; end < 2; ++end) {
        for (int axis = 0; axis < 2; ++axis) {   // 0 = ry (dof 4), 1 = rz (dof 5)
            HingeRow &h = m_hinges[end][axis];
            const int dof = end * 6 + 4 + axis;
            h.enabled = new QCheckBox(tr("%1 — %2").arg(end == 0 ? tr("I") : tr("J")).arg(axis == 0 ? tr("ry") : tr("rz")), plastic);
            h.yield = makeSpin(plastic, 1e9, 3, axis == 0 ? defaults.yieldMomentY : defaults.yieldMomentZ);
            h.stiffness = makeSpin(plastic, 1e15, 1, axis == 0 ? defaults.stiffnessY : defaults.stiffnessZ);
            h.ratio = makeSpin(plastic, 0.99, 4, 0.01);
            h.axial = makeSpin(plastic, 1e9, 2, defaults.axialYield);
            h.exponent = makeSpin(plastic, 10.0, 2, 2.0);
            h.exponent->setMinimum(0.5);
            h.exponent->setValue(2.0);
            for (QWidget *w : {static_cast<QWidget *>(h.yield), static_cast<QWidget *>(h.stiffness), static_cast<QWidget *>(h.ratio), static_cast<QWidget *>(h.axial),
                               static_cast<QWidget *>(h.exponent)}) {
                w->setEnabled(false);
            }
            connect(h.enabled, &QCheckBox::toggled, this, [&h](bool on) {
                for (QWidget *w : {static_cast<QWidget *>(h.yield), static_cast<QWidget *>(h.stiffness), static_cast<QWidget *>(h.ratio), static_cast<QWidget *>(h.axial),
                                   static_cast<QWidget *>(h.exponent)}) {
                    w->setEnabled(on);
                }
            });
            for (const auto &spec : initial.hinges) {
                if (spec.dof == dof) {
                    h.enabled->setChecked(true);
                    h.yield->setValue(spec.yieldMoment);
                    h.stiffness->setValue(spec.kElastic);
                    h.ratio->setValue(spec.hardeningRatio);
                    h.axial->setValue(spec.axialYield);
                    h.exponent->setValue(spec.exponent);
                }
            }
            pGrid->addWidget(h.enabled, line, 0);
            pGrid->addWidget(h.yield, line, 1);
            pGrid->addWidget(h.stiffness, line, 2);
            pGrid->addWidget(h.ratio, line, 3);
            pGrid->addWidget(h.axial, line, 4);
            pGrid->addWidget(h.exponent, line, 5);
            ++line;
        }
    }
    root->addWidget(plastic);
    auto *note = new QLabel(tr("Mp = fy·Z (módulo plástico da seção) e k elástico = 1000·EI/L são sugeridos a partir do perfil e do material. A rótula é uma mola em série com a barra: "
                               "elástica até Mp (reduzido por N: Mp,ef = Mp·(1 − |N/Ny|^expoente)) e, depois, com rigidez pós-escoamento = enrijecimento × k. "
                               "Rótulas plásticas ativam a análise não linear incremental; as combinações passam a ser resolvidas uma a uma (sem superposição). "
                               "Ligação semirrígida e articulação (Propriedades da barra) não podem coexistir no mesmo grau de liberdade: a articulação prevalece."), this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #5a6472;"));
    root->addWidget(note);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

void MemberEndsDialog::accept()
{
    // uma rotula marcada com Mp nulo seria descartada em silencio (material sem fy/secao sem modulo plastico): pede o valor
    for (int end = 0; end < 2; ++end) {
        for (int axis = 0; axis < 2; ++axis) {
            const HingeRow &h = m_hinges[end][axis];
            if (h.enabled->isChecked() && h.yield->value() <= 0.0) {
                QMessageBox::warning(this, tr("Rótula plástica"),
                                     tr("Informe o momento plástico Mp (maior que zero) da rótula marcada. Sem fy no material ou sem módulo plástico da seção não há valor sugerido."));
                h.yield->setFocus();
                return;
            }
        }
    }
    QDialog::accept();
}

PD::MemberConnectionSpec MemberEndsDialog::spec() const
{
    PD::MemberConnectionSpec out;
    for (int end = 0; end < 2; ++end) {
        for (int axis = 0; axis < 3; ++axis) {
            const Row &row = m_rows[end][axis];
            if (row.mode->currentData().toInt() == 1 && row.stiffness->value() > 0.0) {
                out.springK[static_cast<std::size_t>(end * 6 + 3 + axis)] = row.stiffness->value();
            }
        }
        for (int axis = 0; axis < 2; ++axis) {
            const HingeRow &h = m_hinges[end][axis];
            if (!h.enabled->isChecked() || h.yield->value() <= 0.0) {
                continue;
            }
            PD::PlasticHingeSpec spec;
            spec.dof = end * 6 + 4 + axis;
            spec.yieldMoment = h.yield->value();
            spec.kElastic = h.stiffness->value() > 0.0 ? h.stiffness->value() : (axis == 0 ? m_defaults.stiffnessY : m_defaults.stiffnessZ);
            spec.hardeningRatio = h.ratio->value();
            spec.axialYield = h.axial->value();
            spec.exponent = h.exponent->value();
            out.hinges.append(spec);
        }
    }
    return out;
}

} // namespace Structura::UI
