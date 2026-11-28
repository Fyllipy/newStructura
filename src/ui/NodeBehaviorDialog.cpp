#include "ui/NodeBehaviorDialog.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace Structura::UI {

namespace JE = Structura::Design::JointEngine;
namespace JS = Structura::Design::JointClass;
namespace AS = Structura::App::Services;

namespace {

QString txt(const std::string &s) { return QString::fromStdString(s); }

QString number(double v, int digits = 5)
{
    return std::isnan(v) ? QObject::tr("n/d") : QString::number(v, 'g', digits);
}

QString yesNo(bool v) { return v ? QObject::tr("Sim") : QObject::tr("Não"); }

QString mechanicalText(JE::ValidityStatus s, const std::string &model)
{
    if (model == "NbrResistanceOnly" && (s == JE::ValidityStatus::ValidWithCaveats || s == JE::ValidityStatus::Valid)) {
        return QObject::tr("Só resistência (NBR, nível C)");   // nao e modelo mecanico por componentes: sem rigidez nem curva
    }
    if (s == JE::ValidityStatus::ValidWithCaveats || s == JE::ValidityStatus::Valid) {
        return QObject::tr("Disponível (%1)").arg(txt(model));
    }
    if (s == JE::ValidityStatus::NotYetAvailable) {
        return QObject::tr("Indisponível");
    }
    return txt(JE::statusName(s));
}

/// Cor e rótulo do resultado da comparação (a cor nunca é o único indicador: o texto diz o resultado).
QString outcomeColor(JE::ComparisonOutcome o)
{
    switch (o) {
    case JE::ComparisonOutcome::Meets: return QStringLiteral("#2e7d32");
    case JE::ComparisonOutcome::DoesNotMeet: return QStringLiteral("#c62828");
    case JE::ComparisonOutcome::Indeterminate: return QStringLiteral("#b26a00");
    case JE::ComparisonOutcome::NotApplicable: return QStringLiteral("#607d8b");
    }
    return QStringLiteral("#607d8b");
}

QString outcomeText(JE::ComparisonOutcome o)
{
    return o == JE::ComparisonOutcome::DoesNotMeet ? QObject::tr("NÃO ATENDE") : txt(JE::comparisonOutcomeName(o));
}

JE::ComparisonOutcome entryOutcome(const NodeBehaviorDialog::MechanicalEntry &e)
{
    return e.hasNegative ? e.directional.overall : e.comparison.outcome;
}

QString html(const std::string &s)
{
    return QString::fromStdString(s).toHtmlEscaped();
}

QString entryOutcomeText(const NodeBehaviorDialog::MechanicalEntry &e)
{
    QString text = outcomeText(entryOutcome(e));
    if (e.hasNegative) {
        text += QObject::tr(" (+: %1; −: %2)").arg(outcomeText(e.comparison.outcome), outcomeText(e.comparisonNegative.outcome));
    }
    return text;
}

/// Frase do resultado sem repetir o rotulo do veredito (o quadro ja o mostra em destaque).
QString detailOf(const JE::ConnectionRequirementComparison &c)
{
    const QString prefix = txt(JE::comparisonOutcomeName(c.outcome)) + QStringLiteral(": ");
    const QString s = txt(c.summary);
    return (s.startsWith(prefix) ? s.mid(prefix.size()) : s).toHtmlEscaped();
}

/// Curva(s) M–θ do método dos componentes: eixos, curva, tangente inicial (Sj,ini) e os pontos de 2/3 Mj,Rd e Mj,Rd. Somente leitura. No modo DUPLO (Incremento 5) o
/// sentido positivo ocupa o 1º quadrante e o negativo o 3º, cada um só se foi CALCULADO; um sentido indisponível mostra a mensagem e nenhuma curva (nada é espelhado).
class CurveWidget final : public QWidget
{
public:
    struct Series {
        std::vector<std::pair<double, double>> points;
        double sIni {0.0};
        double mjRd {0.0};
        bool available {false};
        QString message;
    };
    explicit CurveWidget(QWidget *parent) : QWidget(parent) { setMinimumHeight(210); }
    void setData(const std::vector<std::pair<double, double>> &points, double sIni, double mjRd, const QString &message)
    {
        m_dual = false;
        m_pos = {points, sIni, mjRd, points.size() >= 2, message};
        update();
    }
    void setDual(const Series &positive, const Series &negative)
    {
        m_dual = true;
        m_pos = positive;
        m_neg = negative;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), palette().base());
        const QRectF plot(70, 14, width() - 90, height() - 40);
        p.setPen(palette().color(QPalette::Mid));
        p.drawRect(plot);
        if (!m_dual) {
            paintSingle(p, plot);
            return;
        }
        double maxTheta = 1e-12, maxM = 1e-12;
        for (const Series *s : {&m_pos, &m_neg}) {
            if (s->available) {
                for (const auto &pt : s->points) {
                    maxTheta = std::max(maxTheta, pt.first);
                    maxM = std::max(maxM, pt.second);
                }
            }
        }
        const QPointF o = plot.center();
        const bool anySeries = m_pos.available || m_neg.available;
        const double sx = plot.width() / 2.0 * 0.96 / maxTheta, sy = plot.height() / 2.0 * 0.96 / maxM;
        p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
        p.drawLine(QPointF(plot.left(), o.y()), QPointF(plot.right(), o.y()));
        p.drawLine(QPointF(o.x(), plot.top()), QPointF(o.x(), plot.bottom()));
        p.setPen(palette().color(QPalette::Text));
        if (anySeries) {   // sem nenhuma curva calculada nao ha escala a mostrar
            p.drawText(QRectF(2, plot.top() - 4, 66, 18), Qt::AlignRight, QStringLiteral("+%1 kN·m").arg(maxM, 0, 'g', 4));
            p.drawText(QRectF(2, plot.bottom() - 12, 66, 18), Qt::AlignRight, QStringLiteral("−%1").arg(maxM, 0, 'g', 4));
            p.drawText(QRectF(plot.right() - 130, plot.bottom() + 4, 130, 18), Qt::AlignRight, QStringLiteral("+%1 rad").arg(maxTheta, 0, 'g', 4));
            p.drawText(QRectF(plot.left(), plot.bottom() + 4, 130, 18), Qt::AlignLeft, QStringLiteral("−%1 rad").arg(maxTheta, 0, 'g', 4));
        }
        for (int k = 0; k < 2; ++k) {
            const Series &s = k == 0 ? m_pos : m_neg;
            const double sign = k == 0 ? 1.0 : -1.0;
            const QColor color = k == 0 ? QColor(30, 100, 190) : QColor(217, 130, 43);
            auto map = [&](double theta, double m) { return QPointF(o.x() + sign * theta * sx, o.y() - sign * m * sy); };
            if (!s.available) {
                p.setPen(palette().color(QPalette::Text));
                const QRectF quadrant = k == 0 ? QRectF(o.x(), plot.top(), plot.width() / 2, plot.height() / 2) : QRectF(plot.left(), o.y(), plot.width() / 2, plot.height() / 2);
                p.drawText(quadrant.adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap,
                           (k == 0 ? QObject::tr("Sentido positivo INDISPONÍVEL") : QObject::tr("Sentido negativo INDISPONÍVEL")) + QStringLiteral("\n") + s.message);
                continue;
            }
            p.setPen(QPen(QColor(120, 120, 120), 1, Qt::DashLine));
            const double thetaMax = std::min(maxTheta, maxM / std::max(s.sIni, 1e-12));
            p.drawLine(map(0, 0), map(thetaMax, thetaMax * s.sIni));
            p.setPen(QPen(color, 2));
            QPolygonF poly;
            for (const auto &pt : s.points) {
                poly << map(pt.first, pt.second);
            }
            p.drawPolyline(poly);
            p.setPen(QPen(color, 1));
            p.setBrush(color);
            for (const double ratio : {2.0 / 3.0, 1.0}) {
                const double m = ratio * s.mjRd;
                double theta = 0.0;
                for (std::size_t i = 1; i < s.points.size(); ++i) {
                    if (s.points[i].second >= m) {
                        const double t = (m - s.points[i - 1].second) / std::max(s.points[i].second - s.points[i - 1].second, 1e-12);
                        theta = s.points[i - 1].first + t * (s.points[i].first - s.points[i - 1].first);
                        break;
                    }
                }
                p.drawEllipse(map(theta, m), 3.5, 3.5);
            }
            const QPointF end = map(s.points.back().first, s.points.back().second);
            p.drawText(end + (k == 0 ? QPointF(-96, 16) : QPointF(6, -6)), QStringLiteral("Mj,Rd%1 = %2").arg(k == 0 ? QStringLiteral("+") : QStringLiteral("−")).arg(s.mjRd, 0, 'g', 4));
        }
    }

private:
    void paintSingle(QPainter &p, const QRectF &plot)
    {
        if (m_pos.points.size() < 2) {
            p.setPen(palette().color(QPalette::Text));
            p.drawText(plot, Qt::AlignCenter | Qt::TextWordWrap, m_pos.message.isEmpty() ? QObject::tr("Curva indisponível") : m_pos.message);
            return;
        }
        double maxTheta = 0.0, maxM = 0.0;
        for (const auto &pt : m_pos.points) {
            maxTheta = std::max(maxTheta, pt.first);
            maxM = std::max(maxM, pt.second);
        }
        maxTheta = std::max(maxTheta, 1e-12);
        maxM = std::max(maxM, 1e-12);
        auto map = [&](double theta, double m) { return QPointF(plot.left() + theta / maxTheta * plot.width(), plot.bottom() - m / maxM * plot.height()); };
        p.setPen(QPen(palette().color(QPalette::Text), 1));
        p.drawText(QRectF(2, plot.top() - 4, 66, 18), Qt::AlignRight, QStringLiteral("%1 kN·m").arg(maxM, 0, 'g', 4));
        p.drawText(QRectF(2, plot.bottom() - 10, 66, 18), Qt::AlignRight, QStringLiteral("0"));
        p.drawText(QRectF(plot.right() - 120, plot.bottom() + 4, 120, 18), Qt::AlignRight, QStringLiteral("%1 rad").arg(maxTheta, 0, 'g', 4));
        p.drawText(QRectF(plot.left(), plot.bottom() + 4, 60, 18), Qt::AlignLeft, QStringLiteral("0"));
        p.setPen(QPen(QColor(120, 120, 120), 1, Qt::DashLine));
        const double thetaMax = std::min(maxTheta, maxM / std::max(m_pos.sIni, 1e-12));
        p.drawLine(map(0, 0), map(thetaMax, thetaMax * m_pos.sIni));
        p.setPen(QPen(QColor(30, 100, 190), 2));
        QPolygonF poly;
        for (const auto &pt : m_pos.points) {
            poly << map(pt.first, pt.second);
        }
        p.drawPolyline(poly);
        p.setPen(QPen(QColor(200, 60, 40), 1));
        p.setBrush(QColor(200, 60, 40));
        for (const double ratio : {2.0 / 3.0, 1.0}) {
            const double m = ratio * m_pos.mjRd;
            double theta = 0.0;
            for (std::size_t i = 1; i < m_pos.points.size(); ++i) {
                if (m_pos.points[i].second >= m) {
                    const double t = (m - m_pos.points[i - 1].second) / std::max(m_pos.points[i].second - m_pos.points[i - 1].second, 1e-12);
                    theta = m_pos.points[i - 1].first + t * (m_pos.points[i].first - m_pos.points[i - 1].first);
                    break;
                }
            }
            p.drawEllipse(map(theta, m), 3.5, 3.5);
            p.drawText(map(theta, m) + (ratio < 1.0 ? QPointF(6, 14) : QPointF(-52, 16)), ratio < 1.0 ? QObject::tr("2/3 Mj,Rd") : QObject::tr("Mj,Rd"));
        }
    }

    bool m_dual {false};
    Series m_pos;
    Series m_neg;
};

QTableWidget *table(QWidget *parent, const QStringList &headers, int rows)
{
    auto *t = new QTableWidget(rows, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);   // nenhum cabecalho ou valor truncado
    t->horizontalHeader()->setStretchLastSection(true);
    t->setWordWrap(false);
    return t;
}

} // namespace

NodeBehaviorDialog::NodeBehaviorDialog(const JE::RequiredBehaviorAssessment &assessment, const Context &context, QWidget *parent)
    : QDialog(parent), m_assessment(assessment), m_context(context)
{
    setWindowTitle(tr("Comportamento requerido do nó %1").arg(assessment.nodeId));
    resize(1180, 1000);
    auto *layout = new QVBoxLayout(this);
    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setText(tr("Nó %1 — %2 %3 — estado: %4. O comportamento requerido orienta ANTES de definir a ligação; depois, a ligação efetivamente definida é classificada e comparada com ele (coluna \"Ligação definida\" abaixo e aba \"Resposta mecânica\").")
                           .arg(assessment.nodeId).arg(txt(assessment.standard)).arg(txt(assessment.edition)).arg(txt(JE::statusLabelPt(assessment.status))));
    layout->addWidget(m_summary);
    auto *tabs = new QTabWidget(this);
    m_tabs = tabs;
    layout->addWidget(tabs, 1);

    // ------------------------------------------------------------------------------------------------ aba 1: comportamento requerido
    auto *first = new QWidget(this);
    auto *l1 = new QVBoxLayout(first);
    m_members = table(first, {tr("Barra"), tr("Papel (pela inclinação)"), tr("Rótulo"), tr("L (m)"), tr("I plano (m⁴)"), tr("E (kN/m²)"), tr("Rotação na extremidade")}, static_cast<int>(assessment.members.size()));
    for (int r = 0; r < m_members->rowCount(); ++r) {
        const JE::NodeMemberFacts &m = assessment.members[static_cast<std::size_t>(r)];
        m_members->setItem(r, 0, new QTableWidgetItem(QString::number(m.memberId)));
        m_members->setItem(r, 1, new QTableWidgetItem(txt(JE::nodeMemberRoleName(m.role))));
        m_members->setItem(r, 2, new QTableWidgetItem(txt(m.label)));
        m_members->setItem(r, 3, new QTableWidgetItem(number(m.lengthM, 4)));
        m_members->setItem(r, 4, new QTableWidgetItem(number(m.inertiaM4)));
        m_members->setItem(r, 5, new QTableWidgetItem(number(m.youngKnM2)));
        QString cond = txt(JE::rotationalConditionName(m.condition));
        if (m.condition == JE::RotationalCondition::Spring) {
            cond += QStringLiteral(" k = %1 kN·m/rad").arg(number(m.springKnm));
        }
        m_members->setItem(r, 6, new QTableWidgetItem(cond));
    }
    m_members->setMinimumHeight(140);
    m_members->setMaximumHeight(170);
    l1->addWidget(new QLabel(tr("Barras que chegam ao nó"), first));
    l1->addWidget(m_members);
    m_results = table(first, {tr("Par"), tr("Tipo"), tr("Comportamento requerido"), tr("Base"), tr("Meta rotulada: Si ≤ (kN·m/rad)"), tr("Meta rígida: Si ≥ (kN·m/rad)"), tr("Kv/Kp → meta rígida"), tr("Sistema/contraventamento"), tr("Ligação definida: resultado")},
                      static_cast<int>(assessment.items.size()));
    for (int r = 0; r < m_results->rowCount(); ++r) {
        const JE::RequiredBehaviorItem &i = assessment.items[static_cast<std::size_t>(r)];
        m_results->setItem(r, 0, new QTableWidgetItem(QStringLiteral("%1–%2").arg(i.beamId).arg(i.otherId)));
        m_results->setItem(r, 1, new QTableWidgetItem(txt(JS::kindName(i.kind))));
        m_results->setItem(r, 2, new QTableWidgetItem(txt(JE::requiredKindName(i.required))));
        m_results->setItem(r, 3, new QTableWidgetItem(txt(JE::requirementBasisName(i.basis))));
        m_results->setItem(r, 4, new QTableWidgetItem(number(i.pinnedMaxKnm)));
        m_results->setItem(r, 5, new QTableWidgetItem(number(i.rigidMinKnm)));
        m_results->setItem(r, 6, new QTableWidgetItem(QStringLiteral("%1 → %2").arg(number(i.kvKp, 4), txt(JE::rigidAdmissibilityName(i.rigidAdmissibility)))));
        m_results->setItem(r, 7, new QTableWidgetItem(txt(JE::systemConsistencyName(i.systemConsistency))));
        QString definedText = tr("sem ligação definida");
        QColor definedColor;
        for (const MechanicalEntry &e : context.mechanical) {
            if (e.comparison.beamId == i.beamId && e.comparison.otherId == i.otherId) {
                definedText = entryOutcomeText(e);
                if (!e.hasNegative && e.comparison.real.available) {
                    definedText += tr(" (classe real: %1)").arg(txt(e.comparison.real.classLabel));
                } else if (e.hasNegative && (e.comparison.real.available || e.comparisonNegative.real.available)) {
                    definedText += tr(" [classes: +%1 / −%2]").arg(e.comparison.real.available ? txt(e.comparison.real.classLabel) : tr("—"), e.comparisonNegative.real.available ? txt(e.comparisonNegative.real.classLabel) : tr("—"));
                }
                definedColor = QColor(outcomeColor(entryOutcome(e)));
                break;
            }
        }
        auto *definedItem = new QTableWidgetItem(definedText);
        if (definedColor.isValid()) {
            definedItem->setForeground(definedColor);
            QFont f = definedItem->font();
            f.setBold(true);
            definedItem->setFont(f);
        }
        m_results->setItem(r, 8, definedItem);
    }
    m_results->horizontalHeader()->moveSection(8, 3);   // o resultado da ligacao definida aparece logo apos o comportamento requerido (indice logico 8)
    m_results->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);   // "Base" e longa: largura fixa, o texto completo esta na memoria
    m_results->setColumnWidth(3, 250);
    m_results->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Interactive);
    m_results->setColumnWidth(6, 230);
    m_results->setMinimumHeight(150);
    m_results->setMaximumHeight(190);
    l1->addWidget(new QLabel(tr("Resultado por ligação viga–pilar (cada par é uma ligação distinta)"), first));
    l1->addWidget(m_results);
    m_memory = new QPlainTextEdit(first);
    m_memory->setReadOnly(true);
    m_memory->setPlainText(txt(assessment.memory()));
    l1->addWidget(new QLabel(tr("Memória de classificação: dados, critérios, hipóteses e lacunas"), first));
    l1->addWidget(m_memory, 1);
    tabs->addTab(first, tr("Comportamento requerido"));

    // ------------------------------------------------------------------------------------------------ aba 2: definir a ligação
    auto *second = new QWidget(this);
    auto *l2 = new QVBoxLayout(second);
    auto *note = new QLabel(tr("A escolha da tipologia NÃO é restringida pelo comportamento requerido: a compatibilidade efetiva só se determina depois, com a análise mecânica e a "
                               "classificação normativa da ligação definida. As quatro colunas de capacidade são independentes: uma tipologia pode ser configurável e ter "
                               "dimensionamento normativo sem ter modelo mecânico nem curva M–θ."), second);
    note->setWordWrap(true);
    l2->addWidget(note);
    auto *pairRow = new QHBoxLayout;
    pairRow->addWidget(new QLabel(tr("Ligação viga–pilar:"), second));
    m_pair = new QComboBox(second);
    for (const JE::RequiredBehaviorItem &i : assessment.items) {
        if (i.kind == JS::JointKind::BeamColumn) {
            m_pair->addItem(tr("viga %1 – barra %2 (requerido: %3)").arg(i.beamId).arg(i.otherId).arg(txt(JE::requiredKindName(i.required))), QVariant(i.beamId * 100000 + i.otherId));
        } else if (i.kind == JS::JointKind::BeamBeam) {   // Incremento 5: emendas viga-viga (o criterio de rigidez de 6.1.2 nao se aplica; a resposta mecanica sim)
            m_pair->addItem(tr("viga %1 – viga %2 (emenda; requerido: não aplicável)").arg(i.beamId).arg(i.otherId), QVariant(i.beamId * 100000 + i.otherId));
        }
    }
    pairRow->addWidget(m_pair, 1);
    l2->addLayout(pairRow);
    m_typologies = table(second, {tr("Tipologia"), tr("Grupo"), tr("Configurável"), tr("Dimensionamento normativo"), tr("Modelo mecânico"), tr("Curva M–θ"), tr("Observação")},
                         static_cast<int>(context.typologies.size()));
    for (int r = 0; r < m_typologies->rowCount(); ++r) {
        const AS::TypologyRow &t = context.typologies[static_cast<std::size_t>(r)];
        auto *name = new QTableWidgetItem(t.name);
        name->setData(Qt::UserRole, t.type);
        m_typologies->setItem(r, 0, name);
        m_typologies->setItem(r, 1, new QTableWidgetItem(t.group));
        m_typologies->setItem(r, 2, new QTableWidgetItem(yesNo(t.configurable)));
        m_typologies->setItem(r, 3, new QTableWidgetItem(yesNo(t.normativeDesign)));
        m_typologies->setItem(r, 4, new QTableWidgetItem(mechanicalText(t.mechanical, t.mechanicalModelId)));
        m_typologies->setItem(r, 5, new QTableWidgetItem(yesNo(t.momentRotation)));
        m_typologies->setItem(r, 6, new QTableWidgetItem(txt(t.justification)));
    }
    m_typologies->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_typologies->setColumnWidth(0, 330);
    m_typologies->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_typologies->setColumnWidth(1, 190);
    m_typologies->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Interactive);   // observacao: elidida (texto completo na dica)
    m_typologies->setColumnWidth(6, 260);
    m_typologies->setTextElideMode(Qt::ElideRight);
    for (int r = 0; r < m_typologies->rowCount(); ++r) {
        if (auto *item = m_typologies->item(r, 6)) {
            item->setToolTip(item->text());
        }
    }
    l2->addWidget(m_typologies, 1);
    m_configure = new QPushButton(tr("Configurar a ligação selecionada…"), second);
    m_configure->setToolTip(tr("Abre o configurador de ligações existente para o par e a tipologia escolhidos."));
    connect(m_configure, &QPushButton::clicked, this, &NodeBehaviorDialog::acceptSelection);
    l2->addWidget(m_configure);
    m_defined = table(second, {tr("Ligação"), tr("Barra"), tr("Topologia (JME)"), tr("Lacunas"), tr("Modelo mecânico"), tr("Curva M–θ"), tr("Fingerprint"), tr("Atende ao requerido")}, static_cast<int>(context.defined.size()));
    for (int r = 0; r < m_defined->rowCount(); ++r) {
        const AS::DefinedConnection &d = context.defined[static_cast<std::size_t>(r)];
        m_defined->setItem(r, 0, new QTableWidgetItem(d.typeName));
        m_defined->setItem(r, 1, new QTableWidgetItem(QString::number(d.memberId)));
        m_defined->setItem(r, 2, new QTableWidgetItem(txt(d.topologyStatus)));
        m_defined->setItem(r, 3, new QTableWidgetItem(QString::number(d.topologyGaps)));
        m_defined->setItem(r, 4, new QTableWidgetItem(mechanicalText(d.mechanical, d.mechanicalModelId)));
        m_defined->setItem(r, 5, new QTableWidgetItem(yesNo(d.momentRotation)));
        m_defined->setItem(r, 6, new QTableWidgetItem(txt(d.fingerprint)));
        QString atende = tr("—");
        QColor atendeColor;
        for (const MechanicalEntry &e : context.mechanical) {
            if (e.connectionId == d.id) {
                atende = entryOutcomeText(e);
                atendeColor = QColor(outcomeColor(entryOutcome(e)));
            }
        }
        auto *atendeItem = new QTableWidgetItem(atende);
        if (atendeColor.isValid()) {
            atendeItem->setForeground(atendeColor);
        }
        m_defined->setItem(r, 7, atendeItem);
    }
    m_defined->setMaximumHeight(130);
    l2->addWidget(new QLabel(tr("Ligações já definidas neste nó (módulo de ligações) e o que o motor mecânico deriva delas"), second));
    l2->addWidget(m_defined);
    tabs->addTab(second, tr("Definir ligação"));
    m_configure->setEnabled(m_pair->count() > 0 && m_typologies->rowCount() > 0);

    // ------------------------------------------------------------------------------------------------ aba 3: resposta mecanica
    auto *third = new QWidget;
    auto *l3 = new QVBoxLayout(third);
    auto *note3 = new QLabel(tr("Resposta mecânica da ligação DEFINIDA no nó, pelo modelo mecânico existente (método dos componentes, EN 1993-1-8), nos DOIS sentidos do momento: "
                                "positivo = mesa superior tracionada; negativo = mesa inferior tracionada. Cada sentido é calculado com as fileiras e componentes que ele realmente mobiliza (a curva "
                                "negativa não é a positiva com o sinal trocado). Onde só há formulação de resistência (perfis T), aparece apenas Mj,Rd pelas verificações da NBR (nível C); onde não há modelo ou dado, o sentido é indisponível e não tem números. O quadro \"Classificação normativa e comparação\" "
                                "aplica a NBR 8800:2024, 6.1.2, à rigidez inicial do sentido selecionado e compara com o comportamento requerido."), third);
    note3->setWordWrap(true);
    l3->addWidget(note3);
    m_mechCombo = new QComboBox(third);
    for (const MechanicalEntry &e : context.mechanical) {
        const bool both = e.hasNegative;
        auto dirState = [](const AS::ConnectionMechanicalResult &x) { return x.available ? QObject::tr("calculado") : (x.resistanceOnly ? QObject::tr("só resistência") : QObject::tr("indisponível")); };
        const QString state = both ? tr("+ %1 / − %2").arg(dirState(e.result), dirState(e.negative))
                                   : (e.result.available ? tr("curva M–θ disponível") : (e.result.partial ? tr("parcial (só rigidez)") : tr("indisponível")));
        m_mechCombo->addItem(tr("%1 — viga %2 — %3 — requerido: %4").arg(e.typeName).arg(e.memberId).arg(state, entryOutcomeText(e)));
    }
    l3->addWidget(m_mechCombo);
    m_directional = new QLabel(third);
    m_directional->setWordWrap(true);
    m_directional->setTextFormat(Qt::RichText);
    m_directional->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_directional->setStyleSheet(QStringLiteral("QLabel { border: 1px solid palette(mid); border-radius: 4px; padding: 6px; }"));
    l3->addWidget(m_directional);
    auto *curve = new CurveWidget(third);
    m_curve = curve;
    l3->addWidget(curve);
    m_directions = table(third, {tr("Sentido"), tr("Resposta"), tr("Sj,ini (kN·m/rad)"), tr("Mj,Rd (kN·m)"), tr("Componente de teto"), tr("Fileiras tracionadas participantes"), tr("Classe da ligação"), tr("Comparação com o requerido")}, 0);
    m_directions->setMinimumHeight(86);
    m_directions->setMaximumHeight(96);
    l3->addWidget(m_directions);
    m_mechSummary = new QLabel(third);
    m_mechSummary->setWordWrap(true);
    l3->addWidget(m_mechSummary);
    m_mechNumbers = new QLabel(third);
    m_mechNumbers->setWordWrap(true);
    m_mechNumbers->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l3->addWidget(m_mechNumbers);
    m_cmpLabel = new QLabel(third);
    m_cmpLabel->setWordWrap(true);
    m_cmpLabel->setTextFormat(Qt::RichText);
    m_cmpLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_cmpLabel->setStyleSheet(QStringLiteral("QLabel { border: 1px solid palette(mid); border-radius: 4px; padding: 6px; }"));
    l3->addWidget(m_cmpLabel);
    m_levels = table(third, {tr("M / Mj,Rd"), tr("M (kN·m)"), tr("θ (rad)"), tr("Rigidez secante (kN·m/rad)"), tr("Estado do nó")}, 0);
    m_levels->setMinimumHeight(110);
    m_levels->setMaximumHeight(130);
    l3->addWidget(m_levels);
    m_mechComponents = table(third, {tr("Componente"), tr("Símbolo"), tr("Fileira"), tr("k (mm)"), tr("F,Rd (kN)"), tr("Modo de ruptura"), tr("Flexibilidade 1/k (mm⁻¹)"), tr("Fração da flexibilidade"), tr("Governa a flexibilidade")}, 0);
    m_mechComponents->setMinimumHeight(140);
    l3->addWidget(m_mechComponents);
    m_mechNotes = new QPlainTextEdit(third);
    m_mechNotes->setReadOnly(true);
    m_mechNotes->setMinimumHeight(190);
    l3->addWidget(m_mechNotes);
    m_mechScroll = new QScrollArea(this);
    m_mechScroll->setWidgetResizable(true);
    m_mechScroll->setFrameShape(QFrame::NoFrame);
    m_mechScroll->setWidget(third);
    tabs->addTab(m_mechScroll, tr("Resposta mecânica"));
    connect(m_mechCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
        m_direction = 0;
        showMechanical(i);
    });
    connect(m_directions, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row >= 0 && row != m_direction && row < m_directions->rowCount()) {
            m_direction = row;
            showMechanical(m_mechCombo->currentIndex());
        }
    });
    showMechanical(m_mechCombo->count() > 0 ? 0 : -1);

    // botao de declaracao do sistema estrutural (abre a configuracao existente em Parametros; sem duplicar campos)
    auto *declare = new QPushButton(tr("Declarar sistema estrutural e contraventamento…"), this);
    declare->setToolTip(tr("Abre os Parâmetros (dimensionamento), onde o sistema estrutural e o contraventamento são declarados; depois de salvar, os resultados do nó são atualizados."));
    connect(declare, &QPushButton::clicked, this, [this]() { requestDeclare(); });
    layout->insertWidget(1, declare);

    // aviso no topo: o projetista identifica de imediato uma ligacao que NAO atende ao comportamento requerido
    m_alert = new QLabel(this);
    m_alert->setWordWrap(true);
    m_alert->setTextFormat(Qt::RichText);
    QStringList lines;
    bool anyFail = false, anyIndet = false;
    for (const MechanicalEntry &e : context.mechanical) {
        const JE::ComparisonOutcome o = entryOutcome(e);
        if (o == JE::ComparisonOutcome::DoesNotMeet) {
            anyFail = true;
            lines << tr("<b>%1</b> (viga %2): %3").arg(outcomeText(o).toHtmlEscaped(), QString::number(e.memberId), e.hasNegative ? html(e.directional.summary) : detailOf(e.comparison));
        } else if (o == JE::ComparisonOutcome::Indeterminate) {
            anyIndet = true;
        }
    }
    if (anyFail) {
        m_alert->setText(tr("<b>ATENÇÃO — ligação que NÃO atende ao comportamento requerido do nó:</b><br>") + lines.join(QStringLiteral("<br>")));
        m_alert->setStyleSheet(QStringLiteral("QLabel { background: rgba(198,40,40,0.14); border: 1px solid #c62828; border-radius: 4px; padding: 6px; }"));
    } else if (!context.mechanical.empty()) {
        QStringList parts;
        for (const MechanicalEntry &e : context.mechanical) {
            parts << tr("viga %1: %2").arg(e.memberId).arg(entryOutcomeText(e));
        }
        m_alert->setText(tr("Ligações definidas e comparação com o requerido — ") + parts.join(QStringLiteral("; ")) + (anyIndet ? tr(". Há resultado indeterminado: veja as lacunas na aba \"Resposta mecânica\".") : QStringLiteral(".")));
        m_alert->setStyleSheet(QStringLiteral("QLabel { background: rgba(120,120,120,0.12); border: 1px solid palette(mid); border-radius: 4px; padding: 6px; }"));
    } else {
        m_alert->hide();
    }
    layout->insertWidget(2, m_alert);
}

void NodeBehaviorDialog::requestDeclare()
{
    m_declareRequested = true;
    accept();
}

void NodeBehaviorDialog::fillDirections(int index)
{
    m_directions->blockSignals(true);
    m_directions->setRowCount(0);
    if (index >= 0 && index < static_cast<int>(m_context.mechanical.size())) {
        const MechanicalEntry &e = m_context.mechanical[static_cast<std::size_t>(index)];
        const int rows = e.hasNegative ? 2 : 1;
        m_directions->setRowCount(rows);
        for (int k = 0; k < rows; ++k) {
            const AS::ConnectionMechanicalResult &r = k == 0 ? e.result : e.negative;
            const JE::ConnectionRequirementComparison &c = k == 0 ? e.comparison : e.comparisonNegative;
            const QString state = r.available ? tr("Calculada (%1)").arg(txt(JE::statusLabelPt(r.status))) : (r.resistanceOnly ? tr("Parcial: SÓ resistência (NBR, nível C)") : (r.partial ? tr("Parcial: só rigidez") : tr("INDISPONÍVEL")));
            const QStringList cells = {k == 0 ? tr("Positivo (mesa superior tracionada)") : tr("Negativo (mesa inferior tracionada)"), state,
                                       r.available || r.partial ? number(r.initialStiffnessKnm, 6) : tr("—"), r.available || r.resistanceOnly ? number(r.resistanceKnm, 5) : tr("—"),
                                       r.available || r.resistanceOnly ? txt(r.response.governingMechanism) : tr("—"), r.available || r.partial || r.resistanceOnly ? txt(r.participants) : tr("—"),
                                       c.real.available ? txt(c.real.classLabel) : tr("—"), outcomeText(c.outcome)};
            for (int col = 0; col < cells.size(); ++col) {
                auto *item = new QTableWidgetItem(cells[col]);
                if (col == 7) {
                    item->setForeground(QColor(outcomeColor(c.outcome)));
                    QFont f = item->font();
                    f.setBold(true);
                    item->setFont(f);
                } else if (!r.available && !r.partial && !r.resistanceOnly && col >= 1) {
                    item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
                }
                m_directions->setItem(k, col, item);
            }
        }
        m_directions->setCurrentCell(std::min(m_direction, rows - 1), 0);
    }
    m_directions->blockSignals(false);
}

void NodeBehaviorDialog::showMechanical(int index)
{
    m_levels->setRowCount(0);
    m_mechComponents->setRowCount(0);
    auto *curve = static_cast<CurveWidget *>(m_curve);
    fillDirections(index);
    if (index < 0 || index >= static_cast<int>(m_context.mechanical.size())) {
        m_mechSummary->setText(tr("Nenhuma ligação definida neste nó. Defina uma na aba \"Definir ligação\"."));
        m_mechNumbers->clear();
        m_cmpLabel->setText(tr("Sem ligação definida: não há classificação nem comparação."));
        m_directional->setText(QString());
        m_directional->hide();
        m_mechNotes->clear();
        curve->setData({}, 0.0, 0.0, tr("Sem ligação definida"));
        return;
    }
    const MechanicalEntry &entry = m_context.mechanical[static_cast<std::size_t>(index)];
    const bool negativeShown = entry.hasNegative && m_direction == 1;
    const AS::ConnectionMechanicalResult &r = negativeShown ? entry.negative : entry.result;
    const JE::ConnectionRequirementComparison &cmp = negativeShown ? entry.comparisonNegative : entry.comparison;
    const QString dirName = negativeShown ? tr("NEGATIVO (mesa inferior tracionada)") : (entry.hasNegative ? tr("POSITIVO (mesa superior tracionada)") : tr("positivo"));
    // resultado global dos dois sentidos
    if (entry.hasNegative) {
        QString g = tr("<b>Resultado global (dois sentidos, regra conservadora do programa):</b> <span style=\"color:%1; font-weight:bold\">%2</span> — %3<br>")
                        .arg(outcomeColor(entry.directional.overall), outcomeText(entry.directional.overall).toHtmlEscaped(), html(entry.directional.summary));
        if (!entry.symmetryNote.isEmpty()) {
            g += tr("<b>Simetria:</b> %1<br>").arg(entry.symmetryNote.toHtmlEscaped());
        }
        g += tr("<small>%1</small>").arg(html(entry.directional.rule));
        m_directional->setText(g);
        m_directional->show();
    } else {
        m_directional->setText(QString());
        m_directional->hide();
    }
    {   // quadro: requerido, rigidez inicial, criterio, classe e resultado do sentido selecionado
        QString box = tr("<b>Classificação normativa e comparação com o requerido — sentido %1</b><br>").arg(dirName.toHtmlEscaped());
        box += tr("<b>Comportamento requerido:</b> %1 &nbsp; (base: %2)<br>").arg(html(JE::requiredKindName(cmp.required)), html(JE::requirementBasisName(cmp.basis)));
        if (cmp.real.available) {
            box += tr("<b>Rigidez inicial (Sj,ini):</b> %1 kN·m/rad &nbsp; | &nbsp; <b>Si a 2/3 Mj,Rd:</b> %2 kN·m/rad &nbsp; | &nbsp; η = Si/(EIv/Lv) = %3<br>")
                       .arg(number(cmp.real.initialStiffnessKnm, 6), number(cmp.real.stiffnessKnm, 6), number(cmp.real.eta, 4));
            box += tr("<b>Critério:</b> %1 %2, 6.1.2.1 e 6.1.2.2 — rotulada: Si ≤ %3; rígida: Si ≥ %4 kN·m/rad (EIv/Lv = %5; Kv/Kp = %6)<br>")
                       .arg(html(cmp.real.standard), html(cmp.real.edition), number(cmp.real.limits.sMin, 5), number(cmp.real.limits.sMax, 5), number(cmp.real.limits.referenceStiffness, 5), number(cmp.real.kvKp, 4));
            box += tr("<b>Classificação da ligação:</b> %1<br>").arg(html(cmp.real.classLabel));
        } else {
            box += tr("<b>Rigidez inicial / classificação:</b> indisponíveis (ver lacunas)<br>");
        }
        box += tr("<b>Resultado da comparação (este sentido):</b> <span style=\"color:%1; font-weight:bold\">%2</span> — %3").arg(outcomeColor(cmp.outcome), outcomeText(cmp.outcome).toHtmlEscaped(), detailOf(cmp));
        m_cmpLabel->setText(box);
    }
    QString status = tr("Sentido %1: ").arg(dirName) + (r.available ? tr("Curva M–θ calculada (%1)").arg(txt(JE::statusLabelPt(r.status))) : (r.resistanceOnly ? tr("Resultado parcial: SÓ a resistência Mj,Rd (NBR 8800:2024, nível C); sem rigidez e sem curva M–θ") : (r.partial ? tr("Resultado parcial: só a rigidez inicial") : tr("Resposta mecânica INDISPONÍVEL"))));
    if (!r.message.empty()) {
        status += QStringLiteral(" — ") + txt(r.message);
    }
    m_mechSummary->setText(status);
    QStringList notes;
    if (entry.hasNegative) {
        notes << tr("RESULTADO GLOBAL DOS DOIS SENTIDOS");
        notes << QStringLiteral("  - ") + txt(entry.directional.summary);
        notes << QStringLiteral("  - ") + txt(entry.directional.rule);
    }
    notes << tr("COMPARAÇÃO COM O REQUERIDO (sentido %1) — JUSTIFICATIVAS").arg(dirName);
    for (const std::string &x : cmp.reasons) {
        notes << QStringLiteral("  - ") + txt(x);
    }
    if (!cmp.caveats.empty()) {
        notes << tr("RESSALVAS");
        for (const std::string &x : cmp.caveats) {
            notes << QStringLiteral("  - ") + txt(x);
        }
    }
    if (!cmp.gaps.empty()) {
        notes << tr("LACUNAS");
        for (const std::string &x : cmp.gaps) {
            notes << QStringLiteral("  - ") + txt(x);
        }
    }
    notes << tr("HIPÓTESES DA CLASSIFICAÇÃO");
    for (const std::string &x : cmp.hypotheses) {
        notes << QStringLiteral("  - ") + txt(x);
    }
    notes << tr("HIPÓTESES DA RESPOSTA MECÂNICA");
    for (const std::string &h : r.hypotheses) {
        notes << QStringLiteral("  - ") + txt(h);
    }
    notes << tr("LIMITAÇÕES");
    for (const std::string &l : r.limitations) {
        notes << QStringLiteral("  - ") + txt(l);
    }
    m_mechNotes->setPlainText(notes.join(QLatin1Char('\n')));
    // curvas: modo duplo mostra as duas (so as calculadas)
    auto seriesOf = [](const AS::ConnectionMechanicalResult &x) {
        CurveWidget::Series s;
        s.available = x.available && !x.response.curves.empty();
        s.message = x.message.empty() ? QObject::tr("sem modelo mecânico ou dado") : QString::fromStdString(x.message);
        if (s.available) {
            s.points = x.response.curves.front().points;
            s.sIni = x.initialStiffnessKnm;
            s.mjRd = x.resistanceKnm;
        }
        return s;
    };
    if (entry.hasNegative) {
        curve->setDual(seriesOf(entry.result), seriesOf(entry.negative));
    }
    if (r.resistanceOnly) {
        m_mechNumbers->setText(tr("Resistência Mj,Rd = %1 kN·m (%2) — verificações da NBR 8800:2024 da ligação (nível C), NÃO método dos componentes   |   Rigidez inicial, curva M–θ e capacidade de rotação: INDISPONÍVEIS\nFingerprint da configuração: %3   Fingerprint da análise (deste sentido): %4")
                                  .arg(number(r.resistanceKnm, 5), txt(r.response.governingMechanism.empty() ? std::string("verificação governante n/d") : r.response.governingMechanism), txt(r.configurationFingerprint), txt(r.analysisFingerprint)));
        if (!entry.hasNegative) {
            curve->setData({}, 0.0, 0.0, tr("Só resistência: sem curva M–θ"));
        }
        return;
    }
    if (!r.available && !r.partial) {
        m_mechNumbers->setText(tr("Sem números: o modelo mecânico não está disponível para este sentido."));
        if (!entry.hasNegative) {
            curve->setData({}, 0.0, 0.0, txt(r.message));
        }
        return;
    }
    QString numbers = tr("Rigidez inicial Sj,ini = %1 kN·m/rad").arg(number(r.initialStiffnessKnm, 6));
    if (r.available) {
        const auto &resp = r.response;
        numbers += tr("   |   Resistência Mj,Rd = %1 kN·m (%2)").arg(number(r.resistanceKnm, 5), txt(resp.governingMechanism.empty() ? std::string("componente governante n/d") : resp.governingMechanism));
        if (resp.secantStiffness && resp.tangentStiffness) {
            numbers += tr("   |   Em Mj,Rd: secante = %1; tangente = %2 kN·m/rad").arg(number(resp.secantStiffness->value, 5), number(resp.tangentStiffness->value, 5));
        }
        numbers += tr("   |   Braço de alavanca z = %1 mm   |   Capacidade de rotação: indisponível   |   Convergência: não aplicável (forma fechada)").arg(number(r.leverArmMm, 5));
        if (r.nbrBelowCurve) {
            numbers += tr("   |   ATENÇÃO: a ligação cadastrada não sustenta o Mj,Rd da curva (NBR: %1 kN·m)").arg(number(r.nbrResistanceKnm, 5));
        }
        numbers += tr("\nFlexibilidade total: fileiras tracionadas %1 %; maior contribuinte: %2").arg(r.rowsShareOfTotal * 100.0, 0, 'f', 1).arg(txt(r.governingContribution));
        numbers += tr("\nFingerprint da configuração: %1   Fingerprint da análise (deste sentido): %2").arg(txt(r.configurationFingerprint), txt(r.analysisFingerprint));
        if (!entry.hasNegative) {
            curve->setData(resp.curves.front().points, r.initialStiffnessKnm, r.resistanceKnm, QString());
        }
    } else if (!entry.hasNegative) {
        curve->setData({}, 0.0, 0.0, tr("Mj,Rd não determinado: sem curva"));
    }
    m_mechNumbers->setText(numbers);
    m_levels->setRowCount(static_cast<int>(r.levels.size()));
    for (int i = 0; i < m_levels->rowCount(); ++i) {
        const auto &l = r.levels[static_cast<std::size_t>(i)];
        m_levels->setItem(i, 0, new QTableWidgetItem(number(l.ratioToResistance, 3)));
        m_levels->setItem(i, 1, new QTableWidgetItem(number(l.momentKnm, 5)));
        m_levels->setItem(i, 2, new QTableWidgetItem(number(l.rotationRad, 5)));
        m_levels->setItem(i, 3, new QTableWidgetItem(number(l.secantKnmPerRad, 5)));
        m_levels->setItem(i, 4, new QTableWidgetItem(txt(l.state)));
    }
    m_mechComponents->setRowCount(static_cast<int>(r.components.size()));
    for (int i = 0; i < m_mechComponents->rowCount(); ++i) {
        const auto &c = r.components[static_cast<std::size_t>(i)];
        m_mechComponents->setItem(i, 0, new QTableWidgetItem(txt(c.name)));
        m_mechComponents->setItem(i, 1, new QTableWidgetItem(txt(c.symbol)));
        m_mechComponents->setItem(i, 2, new QTableWidgetItem(c.row == 0 ? tr("nó") : QString::number(c.row)));
        m_mechComponents->setItem(i, 3, new QTableWidgetItem(c.rigid ? tr("rígido") : number(c.stiffnessMm, 5)));
        m_mechComponents->setItem(i, 4, new QTableWidgetItem(c.resistanceKn < 0.0 ? tr("sem resistência própria") : number(c.resistanceKn, 5)));
        m_mechComponents->setItem(i, 5, new QTableWidgetItem(txt(c.failureMode)));
        m_mechComponents->setItem(i, 6, new QTableWidgetItem(c.rigid ? QStringLiteral("0") : number(c.flexibilityPerMm, 4)));
        m_mechComponents->setItem(i, 7, new QTableWidgetItem(c.row == 0 ? (c.shareOfTotal > 0.0 ? tr("%1 % do total").arg(c.shareOfTotal * 100.0, 0, 'f', 1) : QString())
                                                                         : (c.shareInRow > 0.0 ? tr("%1 % da fileira").arg(c.shareInRow * 100.0, 0, 'f', 1) : QString())));
        m_mechComponents->setItem(i, 8, new QTableWidgetItem(c.governingInRow ? tr("sim") : QString()));
    }
}

int NodeBehaviorDialog::mechanicalCount() const { return static_cast<int>(m_context.mechanical.size()); }
QString NodeBehaviorDialog::mechanicalSummary(int index) const
{
    (void)index;
    return m_mechSummary->text() + QLatin1Char('\n') + m_mechNumbers->text();
}
int NodeBehaviorDialog::directionRowCount() const { return m_directions->rowCount(); }
QString NodeBehaviorDialog::directionCell(int row, int column) const
{
    const auto *item = m_directions->item(row, column);
    return item ? item->text() : QString();
}
void NodeBehaviorDialog::selectDirection(int row)
{
    if (row >= 0 && row < m_directions->rowCount()) {
        m_directions->setCurrentCell(row, 0);
    }
}
QString NodeBehaviorDialog::directionalText() const { return m_directional->isHidden() ? QString() : m_directional->text(); }
QString NodeBehaviorDialog::comparisonText() const { return m_cmpLabel->text(); }
QString NodeBehaviorDialog::comparisonNotesText() const { return m_mechNotes->toPlainText(); }
QString NodeBehaviorDialog::alertText() const { return m_alert->isHidden() ? QString() : m_alert->text(); }
int NodeBehaviorDialog::mechanicalLevelRows() const { return m_levels->rowCount(); }
int NodeBehaviorDialog::mechanicalComponentRows() const { return m_mechComponents->rowCount(); }
QString NodeBehaviorDialog::mechanicalComponentCell(int row, int column) const
{
    const auto *item = m_mechComponents->item(row, column);
    return item ? item->text() : QString();
}
void NodeBehaviorDialog::selectMechanical(int index)
{
    if (index >= 0 && index < m_mechCombo->count()) {
        m_mechCombo->setCurrentIndex(index);
        showMechanical(index);
    }
}

void NodeBehaviorDialog::acceptSelection()
{
    const int row = m_typologies->currentRow();
    if (row < 0 || m_pair->currentIndex() < 0) {
        return;
    }
    const int key = m_pair->currentData().toInt();
    m_request.valid = true;
    m_request.nodeId = m_assessment.nodeId;
    m_request.beamId = key / 100000;
    m_request.otherId = key % 100000;
    m_request.type = m_typologies->item(row, 0)->data(Qt::UserRole).toInt();
    accept();
}

bool NodeBehaviorDialog::requestDefinition(int pairIndex, int typeId)
{
    if (pairIndex < 0 || pairIndex >= m_pair->count()) {
        return false;
    }
    for (int r = 0; r < m_typologies->rowCount(); ++r) {
        if (m_typologies->item(r, 0)->data(Qt::UserRole).toInt() == typeId) {
            m_pair->setCurrentIndex(pairIndex);
            m_typologies->setCurrentCell(r, 0);
            acceptSelection();
            return m_request.valid;
        }
    }
    return false;
}

bool NodeBehaviorDialog::saveScreenshots(const QString &prefix)
{
    bool ok = true;
    for (int i = 0; i < m_tabs->count(); ++i) {
        m_tabs->setCurrentIndex(i);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 200);
        ok = grab().save(QStringLiteral("%1_aba%2.png").arg(prefix).arg(i + 1)) && ok;
        if (i == 2 && m_mechScroll) {   // a aba 3 e rolavel: segunda imagem com o fim do conteudo
            m_mechScroll->verticalScrollBar()->setValue(m_mechScroll->verticalScrollBar()->maximum());
            QCoreApplication::processEvents(QEventLoop::AllEvents, 200);
            ok = grab().save(QStringLiteral("%1_aba3b.png").arg(prefix)) && ok;
            m_mechScroll->verticalScrollBar()->setValue(0);
        }
    }
    m_tabs->setCurrentIndex(0);
    return ok;
}

QString NodeBehaviorDialog::memoryText() const { return m_memory->toPlainText(); }
int NodeBehaviorDialog::resultRowCount() const { return m_results->rowCount(); }
QString NodeBehaviorDialog::resultCell(int row, int column) const
{
    const auto *item = m_results->item(row, column);
    return item ? item->text() : QString();
}
QString NodeBehaviorDialog::summaryText() const { return m_summary->text(); }
int NodeBehaviorDialog::typologyRowCount() const { return m_typologies->rowCount(); }
QString NodeBehaviorDialog::typologyCell(int row, int column) const
{
    const auto *item = m_typologies->item(row, column);
    return item ? item->text() : QString();
}
int NodeBehaviorDialog::definedRowCount() const { return m_defined->rowCount(); }
QString NodeBehaviorDialog::definedCell(int row, int column) const
{
    const auto *item = m_defined->item(row, column);
    return item ? item->text() : QString();
}

} // namespace Structura::UI
