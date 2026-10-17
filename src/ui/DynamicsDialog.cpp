#include "DynamicsDialog.h"

#include "BusyProgress.h"
#include "LineChartWidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::UI {

using Structura::App::Services::DynamicsService;

namespace {

QDoubleSpinBox *spin(double min, double max, double value, int decimals, double step, const QString &suffix = QString())
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

QTableWidget *makeTable(const QStringList &headers)
{
    auto *table = new QTableWidget(0, headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(22);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    return table;
}

void setRow(QTableWidget *table, int row, const QStringList &cells)
{
    for (int c = 0; c < cells.size(); ++c) {
        auto *item = new QTableWidgetItem(cells[c]);
        item->setTextAlignment(c == 0 ? Qt::AlignCenter : (Qt::AlignRight | Qt::AlignVCenter));
        table->setItem(row, c, item);
    }
}

QString num(double v, int decimals = 3)
{
    return QString::number(v, 'f', decimals);
}

QString badge(bool ok, const QString &yes, const QString &no)
{
    return ok ? QStringLiteral("<span style='color:#1b7f3b;font-weight:600;'>✔ %1</span>").arg(yes)
              : QStringLiteral("<span style='color:#c0392b;font-weight:600;'>✖ %1</span>").arg(no);
}

} // namespace

DynamicsDialog::DynamicsDialog(const QStringList &massSources, std::shared_ptr<DynamicsData> data, ComputeModes compute, QWidget *parent)
    : QDialog(parent)
    , m_data(std::move(data))
    , m_compute(std::move(compute))
{
    setWindowTitle(tr("Dinâmica"));
    resize(1100, 780);
    auto *root = new QVBoxLayout(this);

    auto *modesBox = new QGroupBox(tr("1. Modos de vibração"));
    auto *modesLayout = new QVBoxLayout(modesBox);
    auto *row = new QHBoxLayout;
    m_source = new QComboBox;
    m_source->addItem(tr("Somente massa dos perfis"), QString());
    for (const QString &name : massSources) {
        m_source->addItem(name, name);
    }
    for (int i = 1; i < m_source->count(); ++i) {   // prefere a combinacao quase permanente
        if (m_source->itemText(i).startsWith(QLatin1String("ELS-QP"))) {
            m_source->setCurrentIndex(i);
            break;
        }
        if (m_source->itemText(i).startsWith(QLatin1String("ELS")) && !m_source->itemText(m_source->currentIndex()).startsWith(QLatin1String("ELS"))) {
            m_source->setCurrentIndex(i);
        }
    }
    m_modeCount = new QSpinBox;
    m_modeCount->setRange(1, 60);
    m_modeCount->setValue(12);
    auto *computeButton = new QPushButton(tr("Calcular modos"));
    computeButton->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 14px; border-radius: 4px; }"));
    row->addWidget(new QLabel(tr("Fonte de massa:")));
    row->addWidget(m_source, 1);
    row->addWidget(new QLabel(tr("Modos:")));
    row->addWidget(m_modeCount);
    row->addWidget(computeButton);
    modesLayout->addLayout(row);
    m_modeTable = makeTable({tr("Modo"), tr("T (s)"), tr("f (Hz)"), tr("Massa X (%)"), tr("Massa Y (%)"), tr("Massa Z (%)"), tr("Acum. X"), tr("Acum. Y"), tr("Acum. Z")});
    m_modeTable->setMinimumHeight(150);
    m_modeTable->setMaximumHeight(190);
    modesLayout->addWidget(m_modeTable);
    m_modeStatus = new QLabel(tr("Calcule os modos para habilitar as análises."));
    m_modeStatus->setStyleSheet(QStringLiteral("color:#5a6472;"));
    modesLayout->addWidget(m_modeStatus);
    root->addWidget(modesBox);

    m_tabs = new QTabWidget;
    root->addWidget(m_tabs, 1);

    // ── Espectro de resposta ─────────────────────────────────────────────
    {
        auto *page = new QWidget;
        auto *layout = new QHBoxLayout(page);
        auto *left = new QVBoxLayout;
        auto *specBox = new QGroupBox(tr("Espectro de projeto Sa(T)/g"));
        auto *specLayout = new QVBoxLayout(specBox);
        m_parametric = new QRadioButton(tr("Paramétrico (patamar, 1/T e 1/T²)"));
        m_parametric->setChecked(true);
        auto *tableRadio = new QRadioButton(tr("Tabela T (s) × Sa/g"));
        specLayout->addWidget(m_parametric);
        auto *form = new QFormLayout;
        m_plateau = spin(0.001, 5.0, 0.25, 3, 0.01);
        m_sa1 = spin(0.001, 5.0, 0.10, 3, 0.01);
        m_tl = spin(0.5, 20.0, 4.0, 1, 0.5, tr(" s"));
        form->addRow(tr("Sa/g no patamar"), m_plateau);
        form->addRow(tr("Sa/g em T = 1 s"), m_sa1);
        form->addRow(tr("Período longo TL"), m_tl);
        specLayout->addLayout(form);
        specLayout->addWidget(tableRadio);
        m_table = new QPlainTextEdit;
        m_table->setPlaceholderText(tr("Uma linha por ponto:  T  Sa/g\n0,00  0,10\n0,10  0,25\n0,40  0,25\n1,00  0,10\n4,00  0,025"));
        m_table->setMaximumHeight(110);
        specLayout->addWidget(m_table);
        auto *note = new QLabel(tr("O espectro é de pseudoaceleração (em g) já com o fator de solo/zona aplicado. Informe os valores do espectro de projeto "
                                   "da norma que se aplica (a NBR 15421 não está em normas/)."));
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral("color:#5a6472;"));
        specLayout->addWidget(note);
        left->addWidget(specBox);

        auto *optBox = new QGroupBox(tr("Parâmetros"));
        auto *optForm = new QFormLayout(optBox);
        m_reduction = spin(1.0, 10.0, 1.0, 2, 0.5);
        m_importance = spin(0.5, 2.0, 1.0, 2, 0.05);
        m_damping = spin(0.001, 0.3, 0.05, 3, 0.005);
        m_combine = new QComboBox;
        m_combine->addItem(tr("CQC (correlação modal)"), 1);
        m_combine->addItem(tr("SRSS"), 0);
        m_directional = new QComboBox;
        m_directional->addItem(tr("SRSS entre direções"), 0);
        m_directional->addItem(tr("100% + 30%"), 1);
        auto *dirRow = new QHBoxLayout;
        const QStringList dirNames {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
        for (int d = 0; d < 3; ++d) {
            m_dir[d] = new QCheckBox(dirNames[d]);
            m_dir[d]->setChecked(d == 0);
            dirRow->addWidget(m_dir[d]);
        }
        m_usedModes = new QSpinBox;
        m_usedModes->setRange(0, 60);
        m_usedModes->setSpecialValueText(tr("todos"));
        optForm->addRow(tr("R (modificação de resposta)"), m_reduction);
        optForm->addRow(tr("I (importância)"), m_importance);
        optForm->addRow(tr("Amortecimento ζ"), m_damping);
        optForm->addRow(tr("Combinação modal"), m_combine);
        optForm->addRow(tr("Direções"), dirRow);
        optForm->addRow(tr("Combinação direcional"), m_directional);
        optForm->addRow(tr("Modos usados"), m_usedModes);
        left->addWidget(optBox);
        auto *run = new QPushButton(tr("Calcular espectro"));
        run->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 14px; border-radius: 4px; }"));
        auto *exportButton = new QPushButton(tr("Exportar CSV…"));
        left->addWidget(run);
        left->addWidget(exportButton);
        left->addStretch(1);
        auto *leftWidget = new QWidget;
        leftWidget->setLayout(left);
        leftWidget->setFixedWidth(360);
        layout->addWidget(leftWidget);

        auto *right = new QVBoxLayout;
        m_spectrumChart = new LineChartWidget;
        m_spectrumChart->setTitles(tr("Espectro de resposta"), tr("T (s)"), tr("Sa / g"));
        right->addWidget(m_spectrumChart, 2);
        m_spectrumSummary = new QLabel;
        m_spectrumSummary->setWordWrap(true);
        m_spectrumSummary->setTextFormat(Qt::RichText);
        right->addWidget(m_spectrumSummary);
        auto *resultTabs = new QTabWidget;
        m_contribTable = makeTable({tr("Dir."), tr("Modo"), tr("T (s)"), tr("Sa (m/s²)"), tr("Γ"), tr("Cortante basal (kN)")});
        m_dispTable = makeTable({tr("Nó"), tr("Ux (mm)"), tr("Uy (mm)"), tr("Uz (mm)")});
        m_forceTable = makeTable({tr("Barra"), tr("N (kN)"), tr("Vy (kN)"), tr("Vz (kN)"), tr("T (kN·m)"), tr("My (kN·m)"), tr("Mz (kN·m)")});
        resultTabs->addTab(m_contribTable, tr("Contribuição dos modos"));
        resultTabs->addTab(m_dispTable, tr("Deslocamentos"));
        resultTabs->addTab(m_forceTable, tr("Esforços nas barras"));
        right->addWidget(resultTabs, 2);
        layout->addLayout(right, 1);
        m_tabs->addTab(page, tr("Espectro de resposta"));

        connect(run, &QPushButton::clicked, this, [this]() { runSpectrum(); });
        connect(exportButton, &QPushButton::clicked, this, [this]() { exportSpectrum(); });
        for (QDoubleSpinBox *box : {m_plateau, m_sa1, m_tl}) {
            connect(box, &QDoubleSpinBox::valueChanged, this, [this]() { updateSpectrumChart(); });
        }
        connect(m_parametric, &QRadioButton::toggled, this, [this]() { updateSpectrumChart(); });
        connect(m_table, &QPlainTextEdit::textChanged, this, [this]() { updateSpectrumChart(); });
    }

    // ── Vibracao de pisos ────────────────────────────────────────────────
    {
        auto *page = new QWidget;
        auto *layout = new QHBoxLayout(page);
        auto *left = new QVBoxLayout;
        auto *box = new QGroupBox(tr("Caminhar de pessoas — NBR 8800:2024, Anexo I"));
        auto *form = new QFormLayout(box);
        m_floorMode = new QComboBox;
        m_occupancy = new QComboBox;
        for (const auto &occ : DynamicsService::floorOccupancies()) {
            m_occupancy->addItem(occ.name);
        }
        m_ceiling = new QCheckBox(tr("Forros e instalações suspensos no piso (+0,010)"));
        m_partitions = spin(0.0, 0.05, 0.0, 3, 0.005);
        m_partitions->setToolTip(tr("Paredes divisórias de altura do piso ao teto: 0,020 a 0,050 (Tabela I.2)"));
        m_floorDamping = spin(0.001, 0.2, 0.02, 3, 0.005);
        m_person = spin(100.0, 2000.0, 750.0, 0, 50.0, tr(" N"));
        m_peakCriterion = new QRadioButton(tr("Aceleração de pico"));
        auto *rmsCriterion = new QRadioButton(tr("Valor eficaz ponderado (fator de resposta R)"));
        m_peakCriterion->setChecked(true);
        form->addRow(tr("Modo (vertical)"), m_floorMode);
        form->addRow(tr("Ocupação (Tabela I.3)"), m_occupancy);
        form->addRow(m_ceiling);
        form->addRow(tr("Divisórias (Tabela I.2)"), m_partitions);
        form->addRow(tr("Amortecimento β (soma)"), m_floorDamping);
        form->addRow(tr("Peso de uma pessoa P₀"), m_person);
        form->addRow(tr("Critério"), m_peakCriterion);
        form->addRow(QString(), rmsCriterion);
        left->addWidget(box);
        auto *note = new QLabel(tr("β = sistema estrutural (0,010) + componente da ocupação + forros + divisórias (Tabela I.2); o valor pode ser editado. "
                                   "A frequência e a massa modal vêm do modo de vibração calculado (avaliação precisa, I.3, com as fórmulas de I.2.4): "
                                   "forma normalizada com valor 1 no nó de maior deslocamento vertical. "
                                   "Para esta verificação a norma manda usar a combinação frequente com ψ = 0,1 (I.1.5): escolha a fonte de massa correspondente."));
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral("color:#5a6472;"));
        left->addWidget(note);
        auto *run = new QPushButton(tr("Verificar piso"));
        run->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 14px; border-radius: 4px; }"));
        left->addWidget(run);
        left->addStretch(1);
        auto *leftWidget = new QWidget;
        leftWidget->setLayout(left);
        leftWidget->setFixedWidth(420);
        layout->addWidget(leftWidget);
        m_floorResult = new QLabel(tr("Escolha o modo vertical do piso e verifique."));
        m_floorResult->setTextFormat(Qt::RichText);
        m_floorResult->setWordWrap(true);
        m_floorResult->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        layout->addWidget(m_floorResult, 1);
        m_tabs->addTab(page, tr("Vibração de piso"));

        connect(run, &QPushButton::clicked, this, [this]() { runFloor(); });
        connect(m_occupancy, &QComboBox::currentIndexChanged, this, [this]() { updateDamping(); });
        connect(m_ceiling, &QCheckBox::toggled, this, [this]() { updateDamping(); });
        connect(m_partitions, &QDoubleSpinBox::valueChanged, this, [this]() { updateDamping(); });
        updateDamping();
    }

    // ── Resposta no tempo ────────────────────────────────────────────────
    {
        auto *page = new QWidget;
        auto *layout = new QHBoxLayout(page);
        auto *left = new QVBoxLayout;
        auto *box = new QGroupBox(tr("Excitação"));
        auto *form = new QFormLayout(box);
        m_kind = new QComboBox;
        m_kind->addItem(tr("Harmônica  F·sen(2π f t)"), 0);
        m_kind->addItem(tr("Pulso de meio seno"), 1);
        m_kind->addItem(tr("Tabela t × F"), 2);
        m_amp = spin(-1e7, 1e7, 1.0, 3, 0.1, tr(" kN"));
        m_freq = spin(0.01, 100.0, 2.0, 3, 0.1, tr(" Hz"));
        m_pulse = spin(0.001, 10.0, 0.2, 3, 0.05, tr(" s"));
        m_forceTableText = new QPlainTextEdit;
        m_forceTableText->setPlaceholderText(tr("t (s)  F (kN)\n0   0\n0,1 5\n0,2 0"));
        m_forceTableText->setMaximumHeight(80);
        m_duration = spin(0.1, 600.0, 10.0, 1, 1.0, tr(" s"));
        m_timeDamping = spin(0.001, 0.3, 0.02, 3, 0.005);
        m_timeModes = new QSpinBox;
        m_timeModes->setRange(0, 60);
        m_timeModes->setSpecialValueText(tr("todos"));
        m_loadNode = new QComboBox;
        m_respNode = new QComboBox;
        m_loadDir = new QComboBox;
        m_respDir = new QComboBox;
        for (QComboBox *combo : {m_loadDir, m_respDir}) {
            combo->addItem(QStringLiteral("X"), 0);
            combo->addItem(QStringLiteral("Y"), 1);
            combo->addItem(QStringLiteral("Z"), 2);
            combo->setCurrentIndex(2);
        }
        form->addRow(tr("Tipo"), m_kind);
        form->addRow(tr("Amplitude F₀"), m_amp);
        form->addRow(tr("Frequência"), m_freq);
        form->addRow(tr("Duração do pulso"), m_pulse);
        form->addRow(tr("Tabela"), m_forceTableText);
        form->addRow(tr("Duração da análise"), m_duration);
        form->addRow(tr("Amortecimento ζ"), m_timeDamping);
        form->addRow(tr("Modos usados"), m_timeModes);
        form->addRow(tr("Nó carregado"), m_loadNode);
        form->addRow(tr("Direção da carga"), m_loadDir);
        form->addRow(tr("Nó de resposta"), m_respNode);
        form->addRow(tr("Direção da resposta"), m_respDir);
        left->addWidget(box);
        auto *run = new QPushButton(tr("Calcular resposta"));
        run->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 14px; border-radius: 4px; }"));
        left->addWidget(run);
        left->addStretch(1);
        auto *leftWidget = new QWidget;
        leftWidget->setLayout(left);
        leftWidget->setFixedWidth(360);
        layout->addWidget(leftWidget);

        auto *right = new QVBoxLayout;
        auto *quantityRow = new QHBoxLayout;
        m_quantity = new QComboBox;
        m_quantity->addItem(tr("Deslocamento (mm)"), 0);
        m_quantity->addItem(tr("Velocidade (mm/s)"), 1);
        m_quantity->addItem(tr("Aceleração (m/s²)"), 2);
        m_quantity->addItem(tr("Aceleração (% g)"), 3);
        m_quantity->setCurrentIndex(2);
        quantityRow->addWidget(new QLabel(tr("Mostrar:")));
        quantityRow->addWidget(m_quantity);
        quantityRow->addStretch(1);
        right->addLayout(quantityRow);
        m_timeChart = new LineChartWidget;
        right->addWidget(m_timeChart, 1);
        m_timeSummary = new QLabel;
        m_timeSummary->setTextFormat(Qt::RichText);
        m_timeSummary->setWordWrap(true);
        right->addWidget(m_timeSummary);
        layout->addLayout(right, 1);
        m_tabs->addTab(page, tr("Resposta no tempo"));

        connect(run, &QPushButton::clicked, this, [this]() { runTime(); });
        connect(m_quantity, &QComboBox::currentIndexChanged, this, [this]() { updateTimeChart(); });
        auto syncKind = [this]() {
            const int kind = m_kind->currentData().toInt();
            m_freq->setEnabled(kind == 0);
            m_pulse->setEnabled(kind == 1);
            m_forceTableText->setEnabled(kind == 2);
        };
        connect(m_kind, &QComboBox::currentIndexChanged, this, syncKind);
        syncKind();
    }

    // ── Vento dinamico ───────────────────────────────────────────────────
    {
        auto *page = new QWidget;
        auto *layout = new QHBoxLayout(page);
        auto *box = new QGroupBox(tr("Modelo discreto — NBR 6123:2023, 9.4"));
        auto *form = new QFormLayout(box);
        m_windMode = new QComboBox;
        m_windDir = new QComboBox;
        m_windDir->addItem(tr("Vento em X"), 0);
        m_windDir->addItem(tr("Vento em Y"), 1);
        m_windCategory = new QComboBox;
        for (const QString &c : {QStringLiteral("I"), QStringLiteral("II"), QStringLiteral("III"), QStringLiteral("IV"), QStringLiteral("V")}) {
            m_windCategory->addItem(tr("Categoria %1").arg(c));
        }
        m_windCategory->setCurrentIndex(1);
        m_windV0 = spin(20.0, 60.0, 35.0, 1, 1.0, tr(" m/s"));
        m_windS1 = spin(0.5, 1.5, 1.0, 2, 0.05);
        m_windS3 = spin(0.5, 1.5, 1.0, 2, 0.05);
        m_windWidth = spin(1.0, 500.0, 20.0, 1, 1.0, tr(" m"));
        m_windWidth->setToolTip(tr("Largura da fachada normal ao vento (l1 da Figura 19)"));
        m_windCa = spin(0.2, 3.0, 1.2, 2, 0.05);
        m_windXi = spin(0.5, 4.0, 1.5, 3, 0.05);
        m_windXi->setToolTip(tr("Fator dinâmico ξ lido nos ábacos das Figuras 20 a 24 (função de h, L, ζ e V̄p/(f L)). No 2º modo, multiplique por C₂ (Figura 25)."));
        m_windFluctOnly = new QCheckBox(tr("Somente a parcela flutuante (use no 2º modo)"));
        m_windCross = new QCheckBox(tr("Criar também o caso transversal (Y = X/3, item 9.5)"));
        m_windCross->setChecked(true);
        form->addRow(tr("Modo de flexão"), m_windMode);
        form->addRow(tr("Direção"), m_windDir);
        form->addRow(tr("Rugosidade"), m_windCategory);
        form->addRow(tr("V₀"), m_windV0);
        form->addRow(tr("S₁"), m_windS1);
        form->addRow(tr("S₃"), m_windS3);
        form->addRow(tr("Largura da fachada"), m_windWidth);
        form->addRow(tr("Coeficiente de arrasto Cₐ"), m_windCa);
        form->addRow(tr("Fator dinâmico ξ"), m_windXi);
        form->addRow(m_windFluctOnly);
        form->addRow(m_windCross);
        auto *left = new QVBoxLayout;
        left->addWidget(box);
        auto *note = new QLabel(tr("V̄p = 0,69 V₀ S₁ S₃. Os níveis são os planos de nós de mesma cota (cota 0 = menor Z do modelo); a forma modal e a massa de cada nível vêm do modo escolhido. "
                                   "A norma aplica o método a edifícios de flexão predominante, f ≥ 0,2 Hz e h ≤ 200 m; as forças são estáticas equivalentes (média + amplitude flutuante) e entram em um caso de carga do modelo. "
                                   "Com dois modos, crie o caso do 1º modo completo e o do 2º só com a flutuante e combine pela raiz da soma dos quadrados (9.4.4)."));
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral("color:#5a6472;"));
        left->addWidget(note);
        auto *run = new QPushButton(tr("Calcular"));
        auto *create = new QPushButton(tr("Criar caso(s) de carga"));
        run->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 14px; border-radius: 4px; }"));
        left->addWidget(run);
        left->addWidget(create);
        left->addStretch(1);
        auto *leftWidget = new QWidget;
        leftWidget->setLayout(left);
        leftWidget->setFixedWidth(420);
        layout->addWidget(leftWidget);
        auto *right = new QVBoxLayout;
        m_windSummary = new QLabel(tr("Calcule os modos e escolha o modo de flexão na direção do vento."));
        m_windSummary->setTextFormat(Qt::RichText);
        m_windSummary->setWordWrap(true);
        right->addWidget(m_windSummary);
        m_windTable = makeTable({tr("Cota (m)"), tr("Massa (t)"), tr("Forma xᵢ"), tr("Área Aᵢ (m²)"), tr("Média (kN)"), tr("Flutuante (kN)"), tr("Total (kN)")});
        right->addWidget(m_windTable, 1);
        layout->addLayout(right, 1);
        m_tabs->addTab(page, tr("Vento dinâmico"));
        connect(run, &QPushButton::clicked, this, [this]() { runWind(); });
        connect(create, &QPushButton::clicked, this, [this]() { createWindCases(); });
    }

    connect(computeButton, &QPushButton::clicked, this, [this]() { computeModes(); });
    updateSpectrumChart();
}

void DynamicsDialog::updateDamping()
{
    const auto occupancies = DynamicsService::floorOccupancies();
    const double occ = occupancies[static_cast<std::size_t>(std::max(0, m_occupancy->currentIndex()))].dampingBase;
    m_floorDamping->setValue(0.010 + occ + (m_ceiling->isChecked() ? 0.010 : 0.0) + m_partitions->value());
}

bool DynamicsDialog::requireModes()
{
    if (!m_data || !m_data->valid || m_data->modal.modalOmega.empty()) {
        QMessageBox::information(this, tr("Dinâmica"), tr("Calcule os modos de vibração primeiro."));
        return false;
    }
    return true;
}

void DynamicsDialog::computeModes()
{
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = m_compute && m_compute(m_source->currentData().toString(), m_modeCount->value(), &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        m_modeStatus->setText(error.isEmpty() ? tr("Não foi possível calcular os modos.") : error);
        m_modeStatus->setStyleSheet(QStringLiteral("color:#c0392b;"));
        return;
    }
    refreshModes();
}

void DynamicsDialog::refreshModes()
{
    const auto info = DynamicsService::modes(m_data->basis());
    m_modeTable->setRowCount(static_cast<int>(info.size()));
    std::array<double, 3> acc {{0, 0, 0}};
    m_floorMode->clear();
    const auto basis = m_data->basis();
    for (std::size_t i = 0; i < info.size(); ++i) {
        const auto &m = info[i];
        for (std::size_t d = 0; d < 3; ++d) {
            acc[d] += m.effectiveRatio[d];
        }
        setRow(m_modeTable, static_cast<int>(i),
               {QString::number(i + 1), num(m.period, 4), num(m.frequency, 3), num(m.effectiveRatio[0] * 100, 1), num(m.effectiveRatio[1] * 100, 1),
                num(m.effectiveRatio[2] * 100, 1), num(acc[0] * 100, 1), num(acc[1] * 100, 1), num(acc[2] * 100, 1)});
        m_floorMode->addItem(tr("Modo %1 — %2 Hz — massa Z %3%").arg(i + 1).arg(m.frequency, 0, 'f', 3).arg(m.effectiveRatio[2] * 100.0, 0, 'f', 1), static_cast<int>(i));
    }
    m_windMode->clear();
    for (std::size_t i = 0; i < info.size(); ++i) {
        m_windMode->addItem(tr("Modo %1 — %2 Hz — massa X %3% / Y %4%").arg(i + 1).arg(info[i].frequency, 0, 'f', 3).arg(info[i].effectiveRatio[0] * 100.0, 0, 'f', 1)
                                .arg(info[i].effectiveRatio[1] * 100.0, 0, 'f', 1),
                            static_cast<int>(i));
    }
    const int vertical = DynamicsService::firstVerticalMode(basis);
    if (vertical >= 0 && vertical < m_floorMode->count()) {
        m_floorMode->setCurrentIndex(vertical);
    }
    m_loadNode->clear();
    m_respNode->clear();
    for (const auto &node : m_data->model.nodes) {
        m_loadNode->addItem(QString::number(node.id), node.id);
        m_respNode->addItem(QString::number(node.id), node.id);
    }
    if (m_loadNode->count() > 0) {
        m_loadNode->setCurrentIndex(m_loadNode->count() - 1);
        m_respNode->setCurrentIndex(m_respNode->count() - 1);
    }
    QVector<double> periods;
    for (const auto &m : info) {
        periods.append(m.period);
    }
    m_spectrumChart->setVerticalMarkers(periods);
    m_modeStatus->setStyleSheet(QStringLiteral("color:#1b7f3b;"));
    m_modeStatus->setText(tr("%1 modo(s) calculado(s). Massa total: %2 t (peso %3 kN). Massa efetiva acumulada: X %4%, Y %5%, Z %6%.")
                              .arg(info.size())
                              .arg(DynamicsService::totalMass(basis), 0, 'f', 2)
                              .arg(DynamicsService::totalMass(basis) * DynamicsService::kGravity, 0, 'f', 1)
                              .arg(acc[0] * 100.0, 0, 'f', 1)
                              .arg(acc[1] * 100.0, 0, 'f', 1)
                              .arg(acc[2] * 100.0, 0, 'f', 1));
    updateSpectrumChart();
}

// ── Vento dinamico ───────────────────────────────────────────────────────

void DynamicsDialog::runWind()
{
    if (!requireModes()) {
        return;
    }
    Structura::App::Services::WindDynamicsService::Input in;
    in.mode = m_windMode->currentData().toInt();
    in.direction = m_windDir->currentData().toInt();
    in.category = m_windCategory->currentIndex();
    in.v0 = m_windV0->value();
    in.s1 = m_windS1->value();
    in.s3 = m_windS3->value();
    in.width = m_windWidth->value();
    in.drag = m_windCa->value();
    in.xi = m_windXi->value();
    in.fluctuatingOnly = m_windFluctOnly->isChecked();
    m_windInput = in;
    m_windResult = Structura::App::Services::WindDynamicsService::compute(m_data->basis(), in);
    m_windTable->setRowCount(0);
    if (!m_windResult.ok) {
        m_windSummary->setText(QStringLiteral("<span style='color:#c0392b;'>%1</span>").arg(m_windResult.message));
        return;
    }
    m_windTable->setRowCount(static_cast<int>(m_windResult.levels.size()));
    for (std::size_t i = 0; i < m_windResult.levels.size(); ++i) {
        const auto &lv = m_windResult.levels[m_windResult.levels.size() - 1 - i];
        setRow(m_windTable, static_cast<int>(i),
               {num(lv.z, 2), num(lv.mass, 2), num(lv.shape, 4), num(lv.area, 1), num(lv.mean, 1), num(lv.fluctuating, 1), num(lv.mean + lv.fluctuating, 1)});
    }
    QString text = tr("f = %1 Hz · V̄p = %2 m/s · q₀ = %3 kN/m² · p = %4 · b̄m = %5 · F_H = %6 kN<br>Cortante na base: média %7 kN + flutuante %8 kN = <b>%9 kN</b>")
                       .arg(m_windResult.frequency, 0, 'f', 3).arg(m_windResult.vp, 0, 'f', 1).arg(m_windResult.q0, 0, 'f', 3).arg(m_windResult.p, 0, 'f', 3)
                       .arg(m_windResult.bm, 0, 'f', 2).arg(m_windResult.fh, 0, 'f', 1).arg(m_windResult.baseShearMean, 0, 'f', 1)
                       .arg(m_windResult.baseShearFluctuating, 0, 'f', 1).arg(m_windResult.baseShearMean + m_windResult.baseShearFluctuating, 0, 'f', 1);
    if (m_windResult.frequency < 0.2) {
        text += tr("<br><span style='color:#c0392b;'>f < 0,2 Hz: fora do campo de aplicação da subseção 9 da NBR 6123.</span>");
    }
    m_windSummary->setText(text);
}

void DynamicsDialog::createWindCases()
{
    runWind();
    if (!m_windResult.ok) {
        return;
    }
    const QString dir = m_windInput.direction == 0 ? QStringLiteral("X") : QStringLiteral("Y");
    const QString crossDir = m_windInput.direction == 0 ? QStringLiteral("Y") : QStringLiteral("X");
    const QString mode = QString::number(m_windInput.mode + 1);
    auto build = [&](const QString &name, const std::vector<std::array<double, 3>> &forces) {
        WindCaseRequest request;
        request.name = name;
        for (std::size_t i = 0; i < forces.size(); ++i) {
            if (std::abs(forces[i][0]) + std::abs(forces[i][1]) + std::abs(forces[i][2]) > 1e-9) {
                request.nodeIds.push_back(m_data->model.nodes[i].id);
                request.forces.push_back(forces[i]);
            }
        }
        emit windCaseRequested(request);
    };
    build(QStringLiteral("VENTO DIN %1 M%2").arg(dir, mode), m_windResult.along);
    if (m_windCross->isChecked()) {
        build(QStringLiteral("VENTO DIN %1 M%2 TRANSV").arg(crossDir, mode), m_windResult.cross);
    }
    m_windSummary->setText(m_windSummary->text() + tr("<br><b>Casos de carga criados.</b> Monte as combinações (ex.: 1,4·vento) e recalcule."));
}

// ── Espectro ─────────────────────────────────────────────────────────────

void DynamicsDialog::updateSpectrumChart()
{
    if (!m_spectrumChart) {
        return;
    }
    DynamicsService::Spectrum spectrum;
    if (m_parametric->isChecked()) {
        spectrum = DynamicsService::Spectrum::parametric(m_plateau->value(), m_sa1->value(), m_tl->value());
    } else {
        DynamicsService::Spectrum::parse(m_table->toPlainText(), spectrum);
    }
    LineChartWidget::Series curve;
    curve.name = tr("Sa/g");
    for (const auto &p : spectrum.points) {
        curve.points.append(QPointF(p.first, p.second));
    }
    QVector<LineChartWidget::Series> all {curve};
    if (m_data && m_data->valid) {
        LineChartWidget::Series modes;
        modes.name = tr("modos");
        modes.color = QColor(QStringLiteral("#d9822b"));
        modes.markers = true;
        for (const auto &m : DynamicsService::modes(m_data->basis())) {
            modes.points.append(QPointF(m.period, spectrum.sa(m.period)));
        }
        all.append(modes);
    }
    m_spectrumChart->setSeries(all);
}

void DynamicsDialog::runSpectrum()
{
    if (!requireModes()) {
        return;
    }
    DynamicsService::SpectrumOptions options;
    if (m_parametric->isChecked()) {
        options.spectrum = DynamicsService::Spectrum::parametric(m_plateau->value(), m_sa1->value(), m_tl->value());
    } else if (!DynamicsService::Spectrum::parse(m_table->toPlainText(), options.spectrum)) {
        QMessageBox::warning(this, tr("Espectro"), tr("A tabela precisa de ao menos dois pontos válidos (T, Sa/g)."));
        return;
    }
    options.reduction = m_reduction->value();
    options.importance = m_importance->value();
    options.damping = m_damping->value();
    options.cqc = m_combine->currentData().toInt() == 1;
    options.thirtyPercent = m_directional->currentData().toInt() == 1;
    for (int d = 0; d < 3; ++d) {
        options.directions[static_cast<std::size_t>(d)] = m_dir[d]->isChecked();
    }
    options.modeCount = m_usedModes->value();
    m_spectrumResult = runBusy(this, tr("Espectro de resposta"), tr("Resolvendo os modos…"), [&](BusyProgress &progress) {
        auto local = options;
        local.progress = [&progress](int done, int total) { progress.report(done, total, QObject::tr("Modo %1 de %2 (forças equivalentes)").arg(done).arg(total)); };
        return DynamicsService::responseSpectrum(m_data->basis(), local);
    });
    QApplication::restoreOverrideCursor();
    const auto &r = m_spectrumResult;
    if (r.lines.empty()) {
        m_spectrumSummary->setText(QStringLiteral("<span style='color:#c0392b;'>%1</span>").arg(r.warning.toHtmlEscaped()));
        return;
    }
    const double weight = DynamicsService::totalMass(m_data->basis()) * DynamicsService::kGravity;
    QString summary = tr("<b>Cortante basal combinado</b> (%1): ").arg(options.cqc ? QStringLiteral("CQC") : QStringLiteral("SRSS"));
    const char *names[3] = {"X", "Y", "Z"};
    for (int d = 0; d < 3; ++d) {
        if (options.directions[static_cast<std::size_t>(d)]) {
            summary += tr("%1 = %2 kN (%3% do peso, massa efetiva %4%)   ")
                           .arg(QLatin1String(names[d]))
                           .arg(r.baseShear[static_cast<std::size_t>(d)], 0, 'f', 1)
                           .arg(weight > 0.0 ? r.baseShear[static_cast<std::size_t>(d)] / weight * 100.0 : 0.0, 0, 'f', 1)
                           .arg(r.massCovered[static_cast<std::size_t>(d)] * 100.0, 0, 'f', 1);
        }
    }
    if (!r.warning.isEmpty()) {
        summary += QStringLiteral("<br><span style='color:#b9770e;'>⚠ %1</span>").arg(r.warning.toHtmlEscaped());
    }
    summary += tr("<br><span style='color:#5a6472;'>Os resultados combinados são valores absolutos (envoltória ±); o sinal se perde na combinação modal.</span>");
    m_spectrumSummary->setText(summary);

    m_contribTable->setRowCount(static_cast<int>(r.lines.size()));
    for (std::size_t i = 0; i < r.lines.size(); ++i) {
        const auto &l = r.lines[i];
        setRow(m_contribTable, static_cast<int>(i), {QLatin1String(names[l.direction]), QString::number(l.mode + 1), num(l.period, 4), num(l.sa, 3), num(l.gamma, 3), num(l.baseShear, 2)});
    }
    const auto &nodes = m_data->model.nodes;
    m_dispTable->setRowCount(static_cast<int>(nodes.size()));
    for (std::size_t i = 0; i < nodes.size() && i < r.displacements.size(); ++i) {
        setRow(m_dispTable, static_cast<int>(i),
               {QString::number(nodes[i].id), num(r.displacements[i][0] * 1000.0, 2), num(r.displacements[i][1] * 1000.0, 2), num(r.displacements[i][2] * 1000.0, 2)});
    }
    m_forceTable->setRowCount(static_cast<int>(r.members.size()));
    for (std::size_t i = 0; i < r.members.size(); ++i) {
        const auto &f = r.members[i].localForces;
        auto big = [&](int c) { return num(std::max(std::abs(f[static_cast<std::size_t>(c)]), std::abs(f[static_cast<std::size_t>(c + 6)])), 2); };
        setRow(m_forceTable, static_cast<int>(i), {QString::number(r.members[i].memberId), big(0), big(1), big(2), big(3), big(4), big(5)});
    }
}

void DynamicsDialog::exportSpectrum()
{
    if (m_spectrumResult.members.empty()) {
        QMessageBox::information(this, tr("Espectro"), tr("Calcule o espectro antes de exportar."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar resultados do espectro"), QStringLiteral("espectro.csv"), tr("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Espectro"), tr("Não foi possível gravar o arquivo."));
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "Modos\nDirecao;Modo;T (s);Sa (m/s2);Gamma;Cortante basal (kN)\n";
    const char *names[3] = {"X", "Y", "Z"};
    for (const auto &l : m_spectrumResult.lines) {
        out << names[l.direction] << ';' << l.mode + 1 << ';' << l.period << ';' << l.sa << ';' << l.gamma << ';' << l.baseShear << '\n';
    }
    out << "\nDeslocamentos (m)\nNo;Ux;Uy;Uz\n";
    const auto &nodes = m_data->model.nodes;
    for (std::size_t i = 0; i < nodes.size() && i < m_spectrumResult.displacements.size(); ++i) {
        out << nodes[i].id << ';' << m_spectrumResult.displacements[i][0] << ';' << m_spectrumResult.displacements[i][1] << ';' << m_spectrumResult.displacements[i][2] << '\n';
    }
    out << "\nEsforcos locais combinados (absolutos)\nBarra;N_i;Vy_i;Vz_i;T_i;My_i;Mz_i;N_j;Vy_j;Vz_j;T_j;My_j;Mz_j\n";
    for (const auto &m : m_spectrumResult.members) {
        out << m.memberId;
        for (const double v : m.localForces) {
            out << ';' << v;
        }
        out << '\n';
    }
}

// ── Piso ─────────────────────────────────────────────────────────────────

void DynamicsDialog::runFloor()
{
    if (!requireModes()) {
        return;
    }
    DynamicsService::FloorInput input;
    input.mode = m_floorMode->currentData().toInt();
    input.damping = m_floorDamping->value();
    input.personWeight = m_person->value();
    input.occupancy = m_occupancy->currentIndex();
    input.useRms = !m_peakCriterion->isChecked();
    const auto r = DynamicsService::floorVibration(m_data->basis(), input);
    if (!r.ok) {
        m_floorResult->setText(QStringLiteral("<span style='color:#c0392b;'>%1</span>").arg(r.message.toHtmlEscaped()));
        return;
    }
    if (m_respNode->count() > 0) {
        const int idx = m_respNode->findData(r.controlNode);
        if (idx >= 0) {
            m_respNode->setCurrentIndex(idx);
            m_loadNode->setCurrentIndex(idx);
        }
    }
    QString html = tr("<h3>Resultado</h3><table cellpadding='3'>");
    auto row = [&](const QString &name, const QString &value) { html += QStringLiteral("<tr><td>%1</td><td align='right'><b>%2</b></td></tr>").arg(name, value); };
    row(tr("Frequência fundamental f₀"), tr("%1 Hz").arg(r.frequency, 0, 'f', 3));
    row(tr("Massa modal M (forma = 1 no nó %1)").arg(r.controlNode), tr("%1 kg").arg(r.modalMass, 0, 'f', 0));
    row(tr("Fração vertical da energia do modo"), tr("%1%").arg(r.verticalShare * 100.0, 0, 'f', 0));
    row(tr("Amortecimento β"), num(input.damping, 3));
    row(tr("Coeficiente dinâmico ζ"), num(r.zeta, 2));
    row(tr("Fator de ponderação W"), num(r.weighting, 3));
    row(tr("Aceleração de pico a_p = 0,55·P₀·e^(−0,35 f₀)/(2·M·β)"), tr("%1 m/s²").arg(r.peak, 0, 'f', 4));
    row(tr("Aceleração eficaz ponderada a_w,rms = ζ·P₀·W/(2√2·M·β)"), tr("%1 m/s²").arg(r.rms, 0, 'f', 4));
    row(tr("Fator de resposta R = a_w,rms / 0,005"), num(r.responseFactor, 2));
    html += QStringLiteral("</table>");
    html += tr("<h3>Verificação (Tabela I.3)</h3><p>%1</p><p>%2</p>")
                .arg(badge(r.frequencyOk, tr("f₀ = %1 Hz ≥ %2 Hz (I.1.6)").arg(r.frequency, 0, 'f', 2).arg(input.fMin, 0, 'f', 1),
                           tr("f₀ = %1 Hz < %2 Hz: exige avaliação precisa (I.1.6)").arg(r.frequency, 0, 'f', 2).arg(input.fMin, 0, 'f', 1)))
                .arg(input.useRms ? badge(r.accelerationOk, tr("R = %1 ≤ %2").arg(r.responseFactor, 0, 'f', 2).arg(r.limitFactor, 0, 'f', 0),
                                          tr("R = %1 > %2").arg(r.responseFactor, 0, 'f', 2).arg(r.limitFactor, 0, 'f', 0))
                                  : badge(r.accelerationOk, tr("a_p = %1 ≤ %2 m/s²").arg(r.peak, 0, 'f', 4).arg(r.limitPeak, 0, 'f', 3),
                                          tr("a_p = %1 > %2 m/s²").arg(r.peak, 0, 'f', 4).arg(r.limitPeak, 0, 'f', 3)));
    if (!r.message.isEmpty()) {
        html += QStringLiteral("<p style='color:#b9770e;'>⚠ %1</p>").arg(r.message.toHtmlEscaped());
    }
    m_floorResult->setText(html);
}

// ── Tempo ────────────────────────────────────────────────────────────────

void DynamicsDialog::runTime()
{
    if (!requireModes()) {
        return;
    }
    DynamicsService::TimeInput input;
    input.kind = static_cast<DynamicsService::TimeInput::Kind>(m_kind->currentData().toInt());
    input.amplitude = m_amp->value();
    input.frequency = m_freq->value();
    input.pulse = m_pulse->value();
    input.duration = m_duration->value();
    input.damping = m_timeDamping->value();
    input.loadNode = m_loadNode->currentData().toInt();
    input.responseNode = m_respNode->currentData().toInt();
    input.loadDirection = m_loadDir->currentData().toInt();
    input.responseDirection = m_respDir->currentData().toInt();
    input.modeCount = m_timeModes->value();
    if (input.kind == DynamicsService::TimeInput::Kind::Table) {
        DynamicsService::Spectrum parsed;
        if (!DynamicsService::Spectrum::parse(m_forceTableText->toPlainText(), parsed)) {
            QMessageBox::warning(this, tr("Resposta no tempo"), tr("A tabela precisa de ao menos dois pontos (t, F)."));
            return;
        }
        input.table = parsed.points;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_timeResult = runBusy(this, tr("Resposta no tempo"), tr("Integrando as equações modais (Newmark)…"),
                           [&]() { return DynamicsService::timeHistory(m_data->basis(), input); });
    QApplication::restoreOverrideCursor();
    if (!m_timeResult.ok) {
        m_timeSummary->setText(QStringLiteral("<span style='color:#c0392b;'>%1</span>").arg(m_timeResult.message.toHtmlEscaped()));
        return;
    }
    const auto &r = m_timeResult;
    const double dynamicFactor = std::abs(r.staticDisplacement) > 1e-12 ? r.peakDisplacement / std::abs(r.staticDisplacement) : 0.0;
    m_timeSummary->setText(tr("Pico de deslocamento: <b>%1 mm</b> (estático sob F₀: %2 mm, fator dinâmico %3) · pico de aceleração: <b>%4 m/s²</b> (%5% g) · aceleração RMS: %6 m/s²")
                               .arg(r.peakDisplacement * 1000.0, 0, 'f', 3)
                               .arg(r.staticDisplacement * 1000.0, 0, 'f', 3)
                               .arg(dynamicFactor, 0, 'f', 2)
                               .arg(r.peakAcceleration, 0, 'f', 4)
                               .arg(r.peakAcceleration / DynamicsService::kGravity * 100.0, 0, 'f', 2)
                               .arg(r.rmsAcceleration, 0, 'f', 4));
    updateTimeChart();
}

void DynamicsDialog::updateTimeChart()
{
    const auto &r = m_timeResult;
    if (!r.ok) {
        return;
    }
    const int which = m_quantity->currentData().toInt();
    const std::vector<double> &values = which == 0 ? r.displacement : which == 1 ? r.velocity : r.acceleration;
    const double scale = which == 0 || which == 1 ? 1000.0 : (which == 3 ? 100.0 / DynamicsService::kGravity : 1.0);
    LineChartWidget::Series series;
    series.name = m_quantity->currentText();
    series.color = QColor(QStringLiteral("#0d7fb9"));
    series.points.reserve(static_cast<int>(r.t.size()));
    for (std::size_t i = 0; i < r.t.size(); ++i) {
        series.points.append(QPointF(r.t[i], values[i] * scale));
    }
    LineChartWidget::Series force;
    force.name = tr("força (kN)");
    force.color = QColor(QStringLiteral("#c0392b"));
    force.dashed = true;
    // a forca entra na escala do grafico apenas como referencia de fase (normalizada ao pico da resposta)
    double fMax = 0.0, vMax = 0.0;
    for (std::size_t i = 0; i < r.t.size(); ++i) {
        fMax = std::max(fMax, std::abs(r.force[i]));
        vMax = std::max(vMax, std::abs(values[i] * scale));
    }
    if (fMax > 0.0) {
        for (std::size_t i = 0; i < r.t.size(); i += std::max<std::size_t>(1, r.t.size() / 2000)) {
            force.points.append(QPointF(r.t[i], r.force[i] / fMax * vMax * 0.6));
        }
        force.name = tr("força (esc. relativa)");
    }
    m_timeChart->setTitles(tr("Resposta no nó %1").arg(m_respNode->currentText()), tr("t (s)"), m_quantity->currentText());
    m_timeChart->setSeries(fMax > 0.0 ? QVector<LineChartWidget::Series> {series, force} : QVector<LineChartWidget::Series> {series});
}

} // namespace Structura::UI
