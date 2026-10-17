#include "NewProjectWizard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWizardPage>

namespace Structura::UI {

namespace {

QWizardPage *makePage(const QString &title, const QString &subtitle)
{
    auto *p = new QWizardPage;
    p->setTitle(title);
    p->setSubTitle(subtitle);
    return p;
}

} // namespace

NewProjectWizard::NewProjectWizard(const QStringList &steels, const QString &defaultSteel, QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("Novo projeto"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setButtonText(QWizard::FinishButton, tr("Criar projeto"));
    setButtonText(QWizard::NextButton, tr("Avançar >"));
    setButtonText(QWizard::BackButton, tr("< Voltar"));
    setButtonText(QWizard::CancelButton, tr("Cancelar"));
    resize(640, 480);

    // 1. identificacao
    auto *p1 = makePage(tr("Identificação"), tr("Os dados alimentam o carimbo das pranchas e o relatório; podem ser editados depois."));
    auto *f1 = new QFormLayout(p1);
    m_name = new QLineEdit;
    m_name->setPlaceholderText(tr("Ex.: Galpão industrial"));
    m_company = new QLineEdit;
    m_client = new QLineEdit;
    m_site = new QLineEdit;
    m_author = new QLineEdit;
    m_registration = new QLineEdit;
    m_registration->setPlaceholderText(tr("CREA / CAU"));
    f1->addRow(tr("Nome do projeto"), m_name);
    f1->addRow(tr("Empresa"), m_company);
    f1->addRow(tr("Cliente"), m_client);
    f1->addRow(tr("Obra / local"), m_site);
    f1->addRow(tr("Responsável técnico"), m_author);
    f1->addRow(tr("Registro profissional"), m_registration);
    addPage(p1);

    // 2. aco
    auto *p2 = makePage(tr("Material"), tr("Aço estrutural padrão do projeto (cria o material com E = 200 GPa, G = 77 GPa e peso específico 78,5 kN/m³)."));
    auto *f2 = new QFormLayout(p2);
    m_steel = new QComboBox;
    m_steel->addItems(steels);
    const int index = m_steel->findText(defaultSteel);
    m_steel->setCurrentIndex(index >= 0 ? index : 0);
    f2->addRow(tr("Aço"), m_steel);
    auto *note = new QLabel(tr("Unidades do projeto: kN e m. Outros materiais podem ser criados em Propriedades > Novo material."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:#5a6472;"));
    f2->addRow(note);
    addPage(p2);

    // 3. casos de carga
    auto *p3 = makePage(tr("Casos de carga"), tr("Casos criados vazios; aplique as cargas depois."));
    auto *l3 = new QVBoxLayout(p3);
    m_dead = new QCheckBox(tr("Permanente (com peso próprio das barras)"));
    m_dead->setChecked(true);
    m_live = new QCheckBox(tr("Sobrecarga"));
    m_live->setChecked(true);
    m_windX = new QCheckBox(tr("Vento em X"));
    m_windY = new QCheckBox(tr("Vento em Y"));
    m_combinations = new QCheckBox(tr("Abrir o gerador de combinações (NBR 8800) ao criar o projeto"));
    m_combinations->setChecked(true);
    for (auto *box : {m_dead, m_live, m_windX, m_windY, m_combinations}) {
        l3->addWidget(box);
    }
    l3->addStretch(1);
    addPage(p3);

    // 4. ponto de partida
    auto *p4 = makePage(tr("Ponto de partida"), tr("Comece de um desenho em branco ou de um modelo paramétrico pronto."));
    auto *l4 = new QVBoxLayout(p4);
    m_blank = new QRadioButton(tr("Desenho em branco"));
    m_blank->setChecked(true);
    m_ready = new QRadioButton(tr("Modelo pronto (galpão de duas águas ou edifício de pórticos)…"));
    l4->addWidget(m_blank);
    l4->addWidget(m_ready);
    auto *hint = new QLabel(tr("Nos modelos prontos, os perfis, casos de carga e combinações vêm do modelo; do assistente valem a identificação do projeto."));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#5a6472;"));
    l4->addWidget(hint);
    l4->addStretch(1);
    addPage(p4);
}

NewProjectSettings NewProjectWizard::settings() const
{
    NewProjectSettings s;
    s.name = m_name->text().trimmed();
    s.company = m_company->text().trimmed();
    s.client = m_client->text().trimmed();
    s.site = m_site->text().trimmed();
    s.author = m_author->text().trimmed();
    s.registration = m_registration->text().trimmed();
    s.steel = m_steel->currentText();
    s.selfWeightCase = m_dead->isChecked();
    s.liveCase = m_live->isChecked();
    s.windX = m_windX->isChecked();
    s.windY = m_windY->isChecked();
    s.openCombinationGenerator = m_combinations->isChecked();
    s.start = m_ready->isChecked() ? NewProjectSettings::Start::ReadyModel : NewProjectSettings::Start::Blank;
    return s;
}

} // namespace Structura::UI
