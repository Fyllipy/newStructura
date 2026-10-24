#include "DesignDialogs.h"

#include <QBrush>
#include <QCheckBox>
#include <cmath>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::ProjectData::DesignMemberParams;
using Structura::App::ProjectData::DesignSettings;
using Structura::App::Services::DesignService;

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

// ---------------------------------------------------------------------------------------------------------------------
// DesignSettingsDialog
// ---------------------------------------------------------------------------------------------------------------------

QWidget *DesignSettingsDialog::buildParamForm(ParamEditors &e, bool allowInheritSteel)
{
    auto *widget = new QWidget;
    auto *form = new QFormLayout(widget);
    form->setContentsMargins(0, 0, 0, 0);
    e.steel = new QComboBox;
    if (allowInheritSteel) {
        e.steel->addItem(tr("(padrão do projeto)"), QString());
    }
    for (const auto &steel : DesignService::steelGrades()) {
        e.steel->addItem(QStringLiteral("%1  (fy %2 / fu %3 MPa)").arg(steel.name).arg(steel.fy).arg(steel.fu), steel.name);
    }
    e.kx = spin(0.05, 10.0, 1.0, 2, 0.1);
    e.ky = spin(0.05, 10.0, 1.0, 2, 0.1);
    e.kz = spin(0.05, 10.0, 1.0, 2, 0.1);
    for (QDoubleSpinBox **box : {&e.lx, &e.ly, &e.lz}) {
        *box = spin(0.0, 1000.0, 0.0, 3, 0.5, tr(" m"));
        (*box)->setSpecialValueText(tr("usar K · L"));
        (*box)->setToolTip(tr("Comprimento de flambagem informado (substitui K · L). Na NBR 8800:2024 é o comprimento destravado; "
                              "na NBR 14762 é o comprimento efetivo KL."));
    }
    e.segments = new QSpinBox;
    e.segments->setRange(1, 50);
    e.lb = spin(0.0, 1000.0, 0.0, 3, 0.5, tr(" m"));
    e.lb->setSpecialValueText(tr("L / trechos"));
    e.braced = new QCheckBox(tr("Mesa comprimida contida continuamente (sem flambagem lateral)"));
    e.cbOne = new QCheckBox(tr("Adotar Cb = 1,0"));
    e.cbOne->setToolTip(tr("Balanços com a extremidade livre sem contenção já adotam Cb = 1,0 automaticamente (NBR 8800:2024, 5.4.2.3-b)."));
    e.oneFlange = new QComboBox;
    e.oneFlange->addItem(tr("Nenhuma"), 0);
    e.oneFlange->addItem(tr("Mesa superior (lado +) contida continuamente"), 1);
    e.oneFlange->addItem(tr("Mesa inferior (lado −) contida continuamente"), 2);
    e.oneFlange->setToolTip(tr("NBR 8800:2024, 5.4.2.4 (I, U, H, caixão e tubulares retangulares): uma mesa livre para se deslocar lateralmente e a outra com contenção "
                               "contínua (ex.: terça com telha na mesa superior). O Cb passa a seguir 5.4.2.4 e os momentos que comprimem a mesa contida não sofrem FLT."));
    e.forceAway = new QCheckBox(tr("Forças transversais orientadas no sentido oposto à mesa livre (ex.: sucção do vento)"));
    e.forceAway->setToolTip(tr("Desmarcado: forças no sentido da mesa livre (ex.: gravidade com a mesa contida em cima) — 5.4.2.4-a."));
    e.b2 = spin(1.0, 5.0, 1.0, 3, 0.05);
    e.netArea = spin(0.1, 1.0, 1.0, 3, 0.05);
    e.defl = new QSpinBox;
    e.defl->setRange(0, 2000);
    e.defl->setSpecialValueText(tr("automático"));
    e.defl->setPrefix(QStringLiteral("L / "));
    e.fatigueCategory = new QComboBox;
    e.fatigueCategory->addItem(tr("Não verificar"), 0);
    e.fatigueCategory->addItem(tr("A – metal-base afastado de solda (165 MPa)"), 1);
    e.fatigueCategory->addItem(tr("B – soldas contínuas, seção bruta de juntas por atrito (110 MPa)"), 2);
    e.fatigueCategory->addItem(tr("B' – soldas contínuas com chapa de espera (83 MPa)"), 3);
    e.fatigueCategory->addItem(tr("C – furos/ligações parafusadas, enrijecedores soldados (69 MPa)"), 4);
    e.fatigueCategory->addItem(tr("D – acessórios longos soldados (48 MPa)"), 5);
    e.fatigueCategory->addItem(tr("E – acessórios soldados, chapa de cobertura (31 MPa)"), 6);
    e.fatigueCategory->addItem(tr("E' – extremidade de cobrejunta, cantos reentrantes (18 MPa)"), 7);
    e.fatigueCategory->addItem(tr("F – cisalhamento na garganta de filete (55 MPa)"), 8);
    e.fatigueCategory->addItem(tr("G – parafusos e barras rosqueadas em tração (48 MPa)"), 9);
    e.fatigueCategory->setToolTip(tr("Tabela H.1: escolha o caso do detalhe. A faixa de tensões é a diferença entre a maior e a menor tensão normal (N/A ± M/W) "
                                     "nas combinações selecionadas: selecione as combinações de fadiga (H.2.1, ações variáveis com ψ1 = 1,0). Dispensada para N < 20 000."));
    e.fatigueCycles = spin(0.0, 1e12, 0.0, 0, 100000.0);
    e.fatigueCycles->setSpecialValueText(tr("infinito (usa σTH)"));
    e.openingD = spin(0.0, 5000.0, 0.0, 0, 10.0, tr(" mm"));
    e.openingD->setSpecialValueText(tr("sem abertura"));
    e.openingD->setToolTip(tr("NBR 8800:2024, Anexo F.3: aberturas circulares sem reforço em vigas I biapoiadas, prismáticas, com carga uniformemente distribuída, "
                              "dentro do terço médio da altura e dos dois quartos centrais do vão, afastadas ≥ 2,5 D, com cortante nos apoios ≤ 50 % de V_Rd."));
    e.openingPos = spin(0.0, 1e6, 0.0, 0, 50.0, tr(" mm"));
    e.openingOffset = spin(-5000.0, 5000.0, 0.0, 0, 10.0, tr(" mm"));
    e.openingNeighbourD = spin(0.0, 5000.0, 0.0, 0, 10.0, tr(" mm"));
    e.openingNeighbourD->setSpecialValueText(tr("não há"));
    e.openingSpacing = spin(0.0, 1e6, 0.0, 0, 50.0, tr(" mm"));
    e.ctMode = new QComboBox;
    e.ctMode->addItem(tr("Ct = 1,0 e An/Ag informado"), 0);
    e.ctMode->addItem(tr("5.2.5-c: Ct = 1 − ec/ℓc (parafusos ou soldas longitudinais em parte dos elementos)"), 1);
    e.ctMode->addItem(tr("5.2.5-b: soldas transversais, Ct = Ac/Ag"), 2);
    e.ctMode->setToolTip(tr("Coeficiente de redução da área líquida (NBR 8800:2024, 5.2.5). Ct = 1,0 quando a força é transmitida diretamente a todos os elementos da seção."));
    e.ctEc = spin(0.0, 1e5, 0.0, 1, 5.0, tr(" mm"));
    e.ctLc = spin(0.0, 1e6, 0.0, 0, 10.0, tr(" mm"));
    e.ctAc = spin(0.0, 1.0, 0.0, 3, 0.05);
    e.ctAc->setSpecialValueText(tr("não informado"));
    e.holeDb = spin(0.0, 100.0, 0.0, 1, 1.0, tr(" mm"));
    e.holeDb->setSpecialValueText(tr("sem furos (usa An/Ag)"));
    e.holeDb->setToolTip(tr("Diâmetro dos parafusos da cadeia de ruptura: largura do furo = db + 1,5 mm (furo padrão, 6.3.6) + 2,0 mm (5.2.4.1-a)."));
    e.holeCount = new QSpinBox;
    e.holeCount->setRange(0, 200);
    e.holeT = spin(0.0, 500.0, 0.0, 2, 0.5, tr(" mm"));
    e.staggerS = spin(0.0, 1e4, 0.0, 0, 5.0, tr(" mm"));
    e.staggerG = spin(0.0, 1e4, 0.0, 0, 5.0, tr(" mm"));
    e.staggerLinks = new QSpinBox;
    e.staggerLinks->setRange(0, 200);
    e.compositeSpacing = spin(0.0, 1e5, 0.0, 0, 10.0, tr(" mm"));
    e.compositeSpacing->setSpecialValueText(tr("duas ligações intermediárias (ℓ = L/3)"));
    e.compositeSpacing->setToolTip(tr("Barras compostas (NBR 8800:2024, 5.3.6): distância ℓ entre as ligações (chapas espaçadoras com parafusos ou soldas) que unem os dois perfis. "
                                      "Exige ℓ/rmin ≤ 3/4 (L/r)0 e pelo menos duas ligações intermediárias."));
    e.compositeWelded = new QCheckBox(tr("Ligações intermediárias soldadas ou com parafusos protendidos (5.3.6.3-b)"));
    e.compositeWelded->setToolTip(tr("Desmarcado: parafusos com aperto normal (5.3.6.3-a, penaliza mais a flambagem)."));
    e.threadedEnds = new QCheckBox(tr("Barra redonda com extremidades rosqueadas (5.2.7)"));
    e.threadedEnds->setToolTip(tr("Tração: menor entre o escoamento da seção bruta e a ruptura da parte rosqueada, Abe fu/γa2 com Abe = 0,75 Ab (6.3.2.2 e 6.3.3.1)."));
    e.webWeldLeg = spin(0.0, 50.0, 0.0, 1, 0.5, tr(" mm"));
    e.webWeldLeg->setSpecialValueText(tr("não verificar"));
    e.webWeldLeg->setToolTip(tr("NBR 8800:2024, 5.4.5: perfil soldado, perna do filete mesa–alma (dois lados). Verifica o cisalhamento horizontal da flexão (V Q / I) somado às forças aplicadas diretamente na mesa."));
    e.webWeldFw = spin(300.0, 700.0, 485.0, 0, 5.0, tr(" MPa"));
    e.flangeLineLoad = spin(0.0, 1e5, 0.0, 1, 1.0, tr(" N/mm"));
    e.flangeLineLoad->setToolTip(tr("Força aplicada diretamente na mesa por unidade de comprimento (carga distribuída sobre a mesa superior), transmitida à alma pela solda."));
    e.coverB = spin(0.0, 2000.0, 0.0, 0, 5.0, tr(" mm"));
    e.coverB->setSpecialValueText(tr("sem lamela"));
    e.coverB->setToolTip(tr("NBR 8800:2024, 5.4.4: chapa de reforço sobreposta à(s) mesa(s) com comprimento inferior ao vão. A seção de transição e o prolongamento são verificados; as propriedades da seção da barra "
                            "são as do perfil sem a lamela, e a lamela entra na tensão da seção de transição."));
    e.coverT = spin(0.0, 200.0, 0.0, 1, 0.5, tr(" mm"));
    e.coverStart = spin(0.0, 1e6, 0.0, 0, 50.0, tr(" mm"));
    e.coverEnd = spin(0.0, 1e6, 0.0, 0, 50.0, tr(" mm"));
    e.coverFlanges = new QComboBox;
    e.coverFlanges->addItem(tr("Duas mesas"), 2);
    e.coverFlanges->addItem(tr("Só a mesa superior"), 1);
    e.coverWeldLeg = spin(0.0, 50.0, 0.0, 1, 0.5, tr(" mm"));
    e.coverEndWeld = new QCheckBox(tr("Solda de filete contínua também na extremidade da lamela (5.4.4.2)"));
    e.coverEndWeld->setChecked(true);
    e.plasticAnalysis = new QCheckBox(tr("Viga contínua com análise rígido-plástica (4.11)"));
    e.plasticAnalysis->setToolTip(tr("Verifica o comprimento destravado da mesa comprimida entre rótulas plásticas (4.11.5): Lpd = [0,12 + 0,076 M1/M2] E/fy ry em seções I, ≥ nos casos de simetria exigida. "
                                     "Exige seção compacta e contenção lateral adequada (4.11.3)."));
    e.mixType = new QComboBox;
    e.mixType->addItem(tr("Barra de aço"), 0);
    e.mixType->addItem(tr("Pilar misto: perfil I/H totalmente revestido"), 1);
    e.mixType->addItem(tr("Pilar misto: perfil I/H parcialmente revestido"), 2);
    e.mixType->addItem(tr("Pilar misto: tubular preenchido com concreto"), 3);
    e.mixType->setToolTip(tr("NBR 8800:2024, Anexo M (método simplificado). O perfil de aço é o da seção da barra; informe o concreto e a armadura abaixo."));
    e.mixModel = new QComboBox;
    e.mixModel->addItem(tr("Modelo I (M.5.2): interação de 5.5.1"), 1);
    e.mixModel->addItem(tr("Modelo II (M.5.3): seções compactas, momentos totais"), 2);
    e.mixModel->addItem(tr("Modelo III (M.5.4): tubulares preenchidos"), 3);
    e.mixHc = spin(0.0, 5000.0, 0.0, 0, 10.0, tr(" mm"));
    e.mixHc->setSpecialValueText(tr("altura do perfil"));
    e.mixBc = spin(0.0, 5000.0, 0.0, 0, 10.0, tr(" mm"));
    e.mixBc->setSpecialValueText(tr("largura do perfil"));
    e.mixFck = spin(20.0, 90.0, 30.0, 0, 5.0, tr(" MPa"));
    e.mixFsk = spin(250.0, 600.0, 500.0, 0, 50.0, tr(" MPa"));
    e.mixBarArea = spin(0.0, 5000.0, 0.0, 1, 10.0, tr(" mm²"));
    e.mixBarArea->setSpecialValueText(tr("sem armadura"));
    e.mixBarsH = new QSpinBox;
    e.mixBarsH->setRange(0, 64);
    e.mixBarsH->setToolTip(tr("Revestidos: barras em cada face superior e inferior (com os cantos). Tubo circular: total de barras na circunferência."));
    e.mixBarsV = new QSpinBox;
    e.mixBarsV->setRange(0, 64);
    e.mixCover = spin(0.0, 500.0, 0.0, 0, 5.0, tr(" mm"));
    e.mixCover->setToolTip(tr("Distância do centro das barras à face do concreto (revestidos) ou à face interna do tubo (preenchidos)."));
    e.mixAlphaE = spin(0.5, 1.5, 1.0, 2, 0.1, QString());
    e.mixAlphaE->setToolTip(tr("αE do agregado (NBR 6118): 1,2 basalto/diabásio; 1,0 granito/gnaisse; 0,9 calcário; 0,7 arenito."));
    e.bearing = spin(0.0, 1000.0, 0.0, 0, 5.0, tr(" mm"));
    e.bearing->setSpecialValueText(tr("só aviso (50 mm)"));
    e.bearing->setToolTip(tr("NBR 14762, Anexo D (perfis U, Z e cartola): com o comprimento de apoio c informado (mínimo 20 mm), a reação de extremidade "
                             "passa a ser verificada contra F_Rd; em 0 só aparece um aviso calculado para 50 mm."));
    e.coldWork = new QCheckBox(tr("Considerar o aumento de fy pelo trabalho a frio nas dobras (NBR 14762, Anexo B)"));
    e.coldWork->setToolTip(tr("fya = C·fyc + (1 − C)·fy ≤ fu, aplicável só a seções totalmente efetivas (λp ≤ 0,673) em tração, compressão e flexão; "
                              "exige fu/fy ≥ 1,2, ri/t ≤ 7 e dobras ≤ 120°. Usa ri = t."));
    e.panel = new QComboBox;
    e.panel->addItem(tr("Não"), 0);
    e.panel->addItem(tr("Sim – barra biapoiada"), 1);
    e.panel->addItem(tr("Sim – barra contínua"), 2);
    e.panel->setToolTip(tr("Terça U enrijecido ou Z enrijecido com telha parafusada na mesa tracionada (ação de sucção): o momento resistente é "
                           "R · Wef · fy / 1,10, com R da Tabela F.1, em lugar da flambagem lateral com torção. As demais condições do Anexo F "
                           "(vão < 10 m, conectores, painel) são de responsabilidade do projetista."));
    e.stiffener = spin(0.0, 20000.0, 0.0, 0, 50.0, tr(" mm"));
    e.stiffener->setSpecialValueText(tr("sem enrijecedores"));
    e.stiffener->setToolTip(tr("Distância entre os eixos de enrijecedores transversais adjacentes, a (NBR 8800:2024, 5.4.3.1.1): "
                               "kv = 5 + 5/(a/h)² para a/h ≤ 3; sem enrijecedores (ou a/h > 3) kv = 5,34. Só vale para I, H e U."));
    form->addRow(tr("Aço"), e.steel);
    form->addRow(tr("Kx (flexão em torno de X, plano da altura)"), e.kx);
    form->addRow(tr("Ky (flexão em torno de Y)"), e.ky);
    form->addRow(tr("Kz (torção)"), e.kz);
    form->addRow(tr("Lx informado (substitui Kx · L)"), e.lx);
    form->addRow(tr("Ly informado (substitui Ky · L)"), e.ly);
    form->addRow(tr("Lz informado (substitui Kz · L)"), e.lz);
    auto *note = new QLabel(tr("A NBR 8800:2024 não usa K: o esforço vem de análise de 2ª ordem com imperfeições e Ne usa o comprimento destravado "
                               "(use Lx/Ly/Lz para travamentos intermediários, K = 1). A NBR 14762 usa o comprimento efetivo K·L (K ou comprimento informado)."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:#5a6472;"));
    form->addRow(note);
    form->addRow(tr("Trechos destravados (mesa comprimida)"), e.segments);
    form->addRow(tr("Lb informado"), e.lb);
    form->addRow(QString(), e.braced);
    form->addRow(QString(), e.cbOne);
    form->addRow(tr("Uma mesa com contenção lateral contínua (5.4.2.4)"), e.oneFlange);
    form->addRow(QString(), e.forceAway);
    form->addRow(tr("Amplificação global B2"), e.b2);
    form->addRow(tr("An / Ag na ligação (tração)"), e.netArea);
    form->addRow(tr("Limite de flecha (ELS)"), e.defl);
    form->addRow(tr("Espaçamento dos enrijecedores de alma (a)"), e.stiffener);
    form->addRow(tr("Mesa tracionada ligada a painel (NBR 14762, Anexo F)"), e.panel);
    form->addRow(QString(), e.coldWork);
    form->addRow(tr("Comprimento de apoio das reações (esmagamento da alma, Anexo D)"), e.bearing);
    form->addRow(tr("Fadiga (NBR 8800, Anexo H): categoria do detalhe"), e.fatigueCategory);
    form->addRow(tr("Fadiga: número de ciclos N"), e.fatigueCycles);
    form->addRow(tr("Abertura circular na alma (Anexo F): diâmetro D"), e.openingD);
    form->addRow(tr("Abertura: distância do centro ao apoio esquerdo"), e.openingPos);
    form->addRow(tr("Abertura: excentricidade em relação à meia altura"), e.openingOffset);
    form->addRow(tr("Abertura adjacente: diâmetro"), e.openingNeighbourD);
    form->addRow(tr("Abertura adjacente: distância entre centros"), e.openingSpacing);
    form->addRow(tr("Tração: coeficiente Ct (5.2.5)"), e.ctMode);
    form->addRow(tr("Ct: excentricidade da ligação ec"), e.ctEc);
    form->addRow(tr("Ct: comprimento da ligação ℓc"), e.ctLc);
    form->addRow(tr("Ct: área dos elementos conectados / Ag"), e.ctAc);
    form->addRow(tr("Área líquida (5.2.4): diâmetro dos parafusos db"), e.holeDb);
    form->addRow(tr("Área líquida: furos na cadeia de ruptura"), e.holeCount);
    form->addRow(tr("Área líquida: espessura do elemento furado"), e.holeT);
    form->addRow(tr("Área líquida, zigue-zague: espaçamento s"), e.staggerS);
    form->addRow(tr("Área líquida, zigue-zague: gabarito g"), e.staggerG);
    form->addRow(tr("Área líquida, zigue-zague: linhas entre furos (s²/4g)"), e.staggerLinks);
    form->addRow(tr("Barra composta: espaçamento ℓ entre as ligações dos perfis"), e.compositeSpacing);
    form->addRow(QString(), e.compositeWelded);
    form->addRow(QString(), e.threadedEnds);
    form->addRow(tr("Perfil soldado: perna do filete mesa–alma (5.4.5)"), e.webWeldLeg);
    form->addRow(tr("Perfil soldado: fw do filete mesa–alma"), e.webWeldFw);
    form->addRow(tr("Perfil soldado: força aplicada diretamente na mesa"), e.flangeLineLoad);
    form->addRow(tr("Lamela (5.4.4): largura"), e.coverB);
    form->addRow(tr("Lamela: espessura"), e.coverT);
    form->addRow(tr("Lamela: extremidade inicial (a partir do início da barra)"), e.coverStart);
    form->addRow(tr("Lamela: extremidade final"), e.coverEnd);
    form->addRow(tr("Lamela: mesas reforçadas"), e.coverFlanges);
    form->addRow(tr("Lamela: perna do filete"), e.coverWeldLeg);
    form->addRow(QString(), e.coverEndWeld);
    form->addRow(QString(), e.plasticAnalysis);
    form->addRow(tr("Pilar misto (Anexo M)"), e.mixType);
    form->addRow(tr("Pilar misto: modelo de cálculo"), e.mixModel);
    form->addRow(tr("Pilar misto: altura do concreto hc"), e.mixHc);
    form->addRow(tr("Pilar misto: largura do concreto bc"), e.mixBc);
    form->addRow(tr("Pilar misto: fck"), e.mixFck);
    form->addRow(tr("Pilar misto: fyk da armadura"), e.mixFsk);
    form->addRow(tr("Pilar misto: área de cada barra"), e.mixBarArea);
    form->addRow(tr("Pilar misto: barras por face horizontal"), e.mixBarsH);
    form->addRow(tr("Pilar misto: barras intermediárias por face lateral"), e.mixBarsV);
    form->addRow(tr("Pilar misto: centro das barras à face"), e.mixCover);
    form->addRow(tr("Pilar misto: αE do agregado"), e.mixAlphaE);
    return widget;
}

void DesignSettingsDialog::load(ParamEditors &e, const DesignMemberParams &p)
{
    const int idx = e.steel->findData(p.steel);
    e.steel->setCurrentIndex(idx >= 0 ? idx : 0);
    e.kx->setValue(p.kx);
    e.ky->setValue(p.ky);
    e.kz->setValue(p.kz);
    e.lx->setValue(p.lx);
    e.ly->setValue(p.ly);
    e.lz->setValue(p.lz);
    e.segments->setValue(p.segments);
    e.lb->setValue(p.lb);
    e.braced->setChecked(p.braced);
    e.cbOne->setChecked(p.cbOne);
    e.oneFlange->setCurrentIndex(std::clamp(p.oneFlangeBraced, 0, 2));
    e.forceAway->setChecked(p.forceAwayFromFreeFlange);
    e.b2->setValue(p.b2);
    e.netArea->setValue(p.netAreaRatio);
    e.defl->setValue(p.deflDenominator);
    e.stiffener->setValue(p.stiffenerSpacing);
    e.panel->setCurrentIndex(std::clamp(p.panelTensionFlange, 0, 2));
    e.coldWork->setChecked(p.coldWork);
    e.bearing->setValue(p.bearingLength);
    e.fatigueCategory->setCurrentIndex(std::clamp(p.fatigueCategory, 0, 9));
    e.fatigueCycles->setValue(p.fatigueCycles);
    e.openingD->setValue(p.openingDiameter);
    e.openingPos->setValue(p.openingPosition);
    e.openingOffset->setValue(p.openingOffset);
    e.openingNeighbourD->setValue(p.openingNeighbourD);
    e.openingSpacing->setValue(p.openingSpacing);
    e.ctMode->setCurrentIndex(std::clamp(p.ctMode, 0, 2));
    e.ctEc->setValue(p.ctEccentricity);
    e.ctLc->setValue(p.ctLength);
    e.ctAc->setValue(p.ctConnectedAreaRatio);
    e.holeDb->setValue(p.holeBoltDiameter);
    e.holeCount->setValue(p.holesInChain);
    e.holeT->setValue(p.holeThickness);
    e.staggerS->setValue(p.staggerPitch);
    e.staggerG->setValue(p.staggerGauge);
    e.staggerLinks->setValue(p.staggerLinks);
    e.compositeSpacing->setValue(p.compositeSpacing);
    e.compositeWelded->setChecked(p.compositeWelded);
    e.threadedEnds->setChecked(p.threadedEnds);
    e.webWeldLeg->setValue(p.webWeldLeg);
    e.webWeldFw->setValue(p.webWeldFw);
    e.flangeLineLoad->setValue(p.flangeLineLoad);
    e.coverB->setValue(p.coverB);
    e.coverT->setValue(p.coverT);
    e.coverStart->setValue(p.coverStart);
    e.coverEnd->setValue(p.coverEnd);
    e.coverFlanges->setCurrentIndex(p.coverFlanges == 1 ? 1 : 0);
    e.coverWeldLeg->setValue(p.coverWeldLeg);
    e.coverEndWeld->setChecked(p.coverEndWeld);
    e.plasticAnalysis->setChecked(p.plasticAnalysis);
    e.mixType->setCurrentIndex(std::max(0, e.mixType->findData(p.mixType)));
    e.mixModel->setCurrentIndex(std::max(0, e.mixModel->findData(p.mixModel)));
    e.mixHc->setValue(p.mixHc);
    e.mixBc->setValue(p.mixBc);
    e.mixFck->setValue(p.mixFck);
    e.mixFsk->setValue(p.mixFsk);
    e.mixBarArea->setValue(p.mixBarArea);
    e.mixBarsH->setValue(p.mixBarsH);
    e.mixBarsV->setValue(p.mixBarsV);
    e.mixCover->setValue(p.mixCover);
    e.mixAlphaE->setValue(p.mixAlphaE);
}

DesignMemberParams DesignSettingsDialog::read(const ParamEditors &e)
{
    DesignMemberParams p;
    p.steel = e.steel->currentData().toString();
    p.kx = e.kx->value();
    p.ky = e.ky->value();
    p.kz = e.kz->value();
    p.lx = e.lx->value();
    p.ly = e.ly->value();
    p.lz = e.lz->value();
    p.segments = e.segments->value();
    p.lb = e.lb->value();
    p.braced = e.braced->isChecked();
    p.cbOne = e.cbOne->isChecked();
    p.oneFlangeBraced = e.oneFlange->currentData().toInt();
    p.forceAwayFromFreeFlange = e.forceAway->isChecked();
    p.b2 = e.b2->value();
    p.netAreaRatio = e.netArea->value();
    p.deflDenominator = e.defl->value();
    p.stiffenerSpacing = e.stiffener->value();
    p.panelTensionFlange = e.panel->currentData().toInt();
    p.coldWork = e.coldWork->isChecked();
    p.bearingLength = e.bearing->value();
    p.fatigueCategory = e.fatigueCategory->currentData().toInt();
    p.fatigueCycles = e.fatigueCycles->value();
    p.openingDiameter = e.openingD->value();
    p.openingPosition = e.openingPos->value();
    p.openingOffset = e.openingOffset->value();
    p.openingNeighbourD = e.openingNeighbourD->value();
    p.openingSpacing = e.openingSpacing->value();
    p.ctMode = e.ctMode->currentData().toInt();
    p.ctEccentricity = e.ctEc->value();
    p.ctLength = e.ctLc->value();
    p.ctConnectedAreaRatio = e.ctAc->value();
    p.holeBoltDiameter = e.holeDb->value();
    p.holesInChain = e.holeCount->value();
    p.holeThickness = e.holeT->value();
    p.staggerPitch = e.staggerS->value();
    p.staggerGauge = e.staggerG->value();
    p.staggerLinks = e.staggerLinks->value();
    p.compositeSpacing = e.compositeSpacing->value();
    p.compositeWelded = e.compositeWelded->isChecked();
    p.threadedEnds = e.threadedEnds->isChecked();
    p.webWeldLeg = e.webWeldLeg->value();
    p.webWeldFw = e.webWeldFw->value();
    p.flangeLineLoad = e.flangeLineLoad->value();
    p.coverB = e.coverB->value();
    p.coverT = e.coverT->value();
    p.coverStart = e.coverStart->value();
    p.coverEnd = e.coverEnd->value();
    p.coverFlanges = e.coverFlanges->currentData().toInt();
    p.coverWeldLeg = e.coverWeldLeg->value();
    p.coverEndWeld = e.coverEndWeld->isChecked();
    p.plasticAnalysis = e.plasticAnalysis->isChecked();
    p.mixType = e.mixType->currentData().toInt();
    p.mixModel = e.mixModel->currentData().toInt();
    p.mixHc = e.mixHc->value();
    p.mixBc = e.mixBc->value();
    p.mixFck = e.mixFck->value();
    p.mixFsk = e.mixFsk->value();
    p.mixBarArea = e.mixBarArea->value();
    p.mixBarsH = e.mixBarsH->value();
    p.mixBarsV = e.mixBarsV->value();
    p.mixCover = e.mixCover->value();
    p.mixAlphaE = e.mixAlphaE->value();
    return p;
}

DesignSettingsDialog::DesignSettingsDialog(const DesignSettings &settings, const QStringList &combinationNames,
                                           const QVector<int> &selectedMemberIds, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Parâmetros de dimensionamento"));
    setMinimumWidth(600);
    resize(660, 720);
    auto *outer = new QVBoxLayout(this);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
    auto *root = new QVBoxLayout(content);

    auto *global = new QGroupBox(tr("Projeto"));
    auto *globalForm = new QFormLayout(global);
    m_defaultSteel = new QComboBox;
    for (const auto &steel : DesignService::steelGrades()) {
        m_defaultSteel->addItem(QStringLiteral("%1  (fy %2 / fu %3 MPa)").arg(steel.name).arg(steel.fy).arg(steel.fu), steel.name);
    }
    m_defaultSteel->setCurrentIndex(std::max(0, m_defaultSteel->findData(settings.defaultSteel)));
    m_tubeStandard = new QComboBox;
    m_tubeStandard->addItem(tr("Automático (laminado × formado a frio)"), QStringLiteral("AUTO"));
    m_tubeStandard->addItem(tr("Sempre NBR 14762 (formados a frio)"), QStringLiteral("NBR14762"));
    m_tubeStandard->addItem(tr("Sempre NBR 8800 (laminados / tubos pesados)"), QStringLiteral("NBR8800"));
    m_tubeStandard->setToolTip(tr("Automático: tubulares retangulares de parede fina (espessura < 6 mm, séries formadas a frio) seguem a NBR 14762; os de parede grossa "
                                  "(≥ 6 mm, séries acabadas a quente) seguem a NBR 8800. Os demais perfis já seguem a norma do seu tipo (laminados e soldados: NBR 8800; "
                                  "dobrados: NBR 14762)."));
    m_tubeStandard->setCurrentIndex(std::max(0, m_tubeStandard->findData(settings.rectTubeStandard)));
    m_amplify = new QCheckBox(tr("Amplificar momentos pelo efeito P-δ local (B1, Anexo C da NBR 8800)"));
    m_amplify->setChecked(settings.amplifyB1);
    m_slenderness = new QCheckBox(tr("Considerar o limite de esbeltez no aproveitamento"));
    m_slenderness->setChecked(settings.slendernessGoverns);
    globalForm->addRow(tr("Aço padrão"), m_defaultSteel);
    globalForm->addRow(tr("Norma dos tubulares retangulares"), m_tubeStandard);
    globalForm->addRow(QString(), m_amplify);
    globalForm->addRow(QString(), m_slenderness);
    root->addWidget(global);

    auto *analysis = new QGroupBox(tr("Efeitos de segunda ordem e serviço"));
    auto *analysisForm = new QFormLayout(analysis);
    m_secondOrder = new QCheckBox(tr("Análise de 2ª ordem (P-Δ e P-δ) nas combinações — dispensa B1/B2"));
    m_secondOrder->setChecked(settings.secondOrder);
    m_secondOrder->setToolTip(tr("Cada combinação é resolvida com rigidez geométrica (não há superposição). Requer refazer a análise."));
    m_stability = new QCheckBox(tr("Funções de estabilidade (Livesley) na 2ª ordem — P-δ exato em cada barra, sem subdividi-la"));
    m_stability->setChecked(settings.stabilityFunctions);
    m_stability->setToolTip(tr("Substitui a rigidez geométrica consistente (aproximação cúbica, que pede barras subdivididas quando a força axial é alta) pelas funções de "
                               "estabilidade da barra comprimida/tracionada (Euler-Bernoulli). Com a deformação por cortante ligada, vale a rigidez geométrica."));
    m_shear = new QCheckBox(tr("Deformação por cortante (Timoshenko) nas barras"));
    m_shear->setChecked(settings.shearDeformation);
    m_shear->setToolTip(tr("Usa as áreas de cisalhamento das seções (alma nos perfis I/U/T/tubos retangulares; A/2 em tubos circulares; 5/6 A em maciços retangulares). "
                           "Importa em vigas curtas e de alma esbelta. Seções sem área de cisalhamento (cantoneiras e não suportadas) ficam rígidas ao cortante. Requer refazer a análise."));
    m_warping = new QCheckBox(tr("Torção com empenamento (Vlasov) nas barras de seção I/H"));
    m_warping->setChecked(settings.warpingTorsion);
    m_warping->setToolTip(tr("Acrescenta o empenamento (θ′) como sétimo grau de liberdade das barras de seção I/H laminada ou soldada: o torque se divide entre torção "
                             "de Saint-Venant e flexo-torção, aparecem o bimomento e as tensões normais de empenamento, e a verificação da NBR 8800 (5.5.2.3) passa a ser feita. "
                             "Empenamento impedido nos nós totalmente engastados, contínuo entre barras colineares e livre nos demais (condições de apoio de garfo). "
                             "Não é combinado com flambagem/modal. Requer refazer a análise."));
    m_loadSteps = new QSpinBox;
    m_loadSteps->setRange(0, 200);
    m_loadSteps->setValue(settings.loadSteps);
    m_loadSteps->setSpecialValueText(tr("automático"));
    m_loadSteps->setToolTip(tr("Passos de carga da 2ª ordem: automático = iteração direta e, se divergir, incrementos (com estimativa do fator crítico); n > 1 = sempre em n passos."));
    m_rigidZone = spin(0.0, 1.0, settings.rigidZoneFactor, 2, 0.05);
    m_rigidZone->setSpecialValueText(tr("desligado"));
    m_rigidZone->setToolTip(tr("Zonas rígidas nas extremidades das barras (trecho do eixo dentro do perfil que as recebe): fator 0 = desligado, 1 = trecho todo rígido. "
                               "O comprimento é a fração do caminho do eixo da barra até a face do perfil do outro lado do nó. Os esforços são fornecidos nos nós. Requer refazer a análise."));
    m_reduction = spin(0.5, 1.0, settings.stiffnessReduction, 2, 0.05);
    m_reduction->setToolTip(tr("Redução de EA e EI nas combinações últimas (NBR 8800, 4.10.5: 0,8)"));
    m_autoB2 = new QCheckBox(tr("Calcular B2 por andar (Anexo C) na análise de 1ª ordem"));
    m_autoB2->setChecked(settings.autoB2);
    m_rs = spin(0.5, 1.0, settings.rs, 2, 0.05);
    m_rs->setToolTip(tr("Rs = 0,85 em pórticos; 1,0 nos demais"));
    m_singleStorey = new QCheckBox(tr("Galpão / edificação de um pavimento (limite de topo H/300; senão H/400)"));
    m_singleStorey->setChecked(settings.singleStorey);
    analysisForm->addRow(m_secondOrder);
    analysisForm->addRow(m_stability);
    analysisForm->addRow(tr("Passos de carga (2ª ordem)"), m_loadSteps);
    analysisForm->addRow(m_shear);
    analysisForm->addRow(m_warping);
    analysisForm->addRow(tr("Zonas rígidas nas extremidades (fator)"), m_rigidZone);
    analysisForm->addRow(tr("Fator de redução de rigidez (2ª ordem)"), m_reduction);
    analysisForm->addRow(m_autoB2);
    analysisForm->addRow(tr("Rs (Anexo C)"), m_rs);
    analysisForm->addRow(m_singleStorey);
    m_notional = new QCheckBox(tr("Forças nocionais (NBR 8800, 4.9.7.2): 0,3% das cargas gravitacionais, em X e Y"));
    m_notional->setChecked(settings.notionalLoads);
    m_notional->setToolTip(tr("Cada combinação última gera quatro variantes (±X, ±Y) com forças horizontais nodais proporcionais às cargas verticais "
                              "da combinação, para considerar imperfeições geométricas. Requer refazer a análise."));
    m_geometric = new QCheckBox(tr("Imperfeição geométrica inicial direta (desaprumo H/n) na 2ª ordem, no lugar das forças nocionais"));
    m_geometric->setChecked(settings.geometricImperfection);
    m_geometric->setToolTip(tr("NBR 8800, 4.9.7.1: as combinações últimas da análise de 2ª ordem geram quatro variantes (±X, ±Y) com os nós deslocados de "
                               "(altura acima da base)/n. Vale só com a 2ª ordem ligada e substitui as forças nocionais. Requer refazer a análise."));
    m_imperfectionDenominator = new QSpinBox;
    m_imperfectionDenominator->setRange(100, 2000);
    m_imperfectionDenominator->setValue(static_cast<int>(std::lround(1.0 / std::max(settings.imperfectionRatio, 1e-6))));
    m_imperfectionDenominator->setPrefix(QStringLiteral("H / "));
    m_notionalFactor = spin(0.0005, 0.02, settings.notionalFactor, 4, 0.0005);
    analysisForm->addRow(m_geometric);
    analysisForm->addRow(tr("Desaprumo da imperfeição geométrica"), m_imperfectionDenominator);
    analysisForm->addRow(m_notional);
    analysisForm->addRow(tr("Fator das forças nocionais"), m_notionalFactor);
    root->addWidget(analysis);

    auto *automatic = new QGroupBox(tr("Dimensionamento automático"));
    auto *autoForm = new QFormLayout(automatic);
    m_target = spin(0.3, 1.0, settings.targetEta, 2, 0.05);
    m_target->setToolTip(tr("Aproveitamento máximo aceito ao escolher o perfil (η alvo)"));
    m_groupByFamily = new QCheckBox(tr("Agrupar por família: barras da mesma família recebem o mesmo perfil (o mais solicitado define)"));
    m_groupByFamily->setChecked(settings.groupByFamily);
    autoForm->addRow(tr("η alvo"), m_target);
    autoForm->addRow(m_groupByFamily);
    root->addWidget(automatic);

    auto *comboGroup = new QGroupBox(tr("Combinações últimas verificadas"));
    auto *comboLayout = new QVBoxLayout(comboGroup);
    m_combinations = new QListWidget;
    m_combinations->setMaximumHeight(110);
    for (const QString &name : combinationNames) {
        auto *item = new QListWidgetItem(name, m_combinations);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        const bool checked = settings.combinations.isEmpty() ? !name.startsWith(QLatin1String("ELS"), Qt::CaseInsensitive)
                                                             : settings.combinations.contains(name);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
    comboLayout->addWidget(m_combinations);
    comboLayout->addWidget(new QLabel(tr("Por padrão são verificadas as combinações que não começam por \"ELS\".")));
    root->addWidget(comboGroup);

    auto *defaults = new QGroupBox(tr("Parâmetros padrão das barras"));
    auto *defaultsLayout = new QVBoxLayout(defaults);
    defaultsLayout->addWidget(buildParamForm(m_defaults, false));
    load(m_defaults, settings.defaults);
    m_defaults.steel->setEnabled(false);
    m_defaults.steel->setVisible(false);
    root->addWidget(defaults);

    m_selectionGroup = new QGroupBox(tr("Barras selecionadas (%1): parâmetros próprios").arg(selectedMemberIds.size()));
    m_selectionGroup->setCheckable(true);
    m_selectionGroup->setChecked(false);
    auto *selLayout = new QVBoxLayout(m_selectionGroup);
    selLayout->addWidget(buildParamForm(m_selected, true));
    DesignMemberParams start = selectedMemberIds.isEmpty() ? settings.defaults : settings.paramsFor(selectedMemberIds.first());
    load(m_selected, start);
    root->addWidget(m_selectionGroup);
    m_selectionGroup->setVisible(!selectedMemberIds.isEmpty());

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);
}

DesignSettings DesignSettingsDialog::settings() const
{
    DesignSettings s = m_settings;
    s.defaultSteel = m_defaultSteel->currentData().toString();
    s.rectTubeStandard = m_tubeStandard->currentData().toString();
    s.amplifyB1 = m_amplify->isChecked();
    s.slendernessGoverns = m_slenderness->isChecked();
    s.secondOrder = m_secondOrder->isChecked();
    s.stabilityFunctions = m_stability->isChecked();
    s.shearDeformation = m_shear->isChecked();
    s.warpingTorsion = m_warping->isChecked();
    s.rigidZoneFactor = m_rigidZone->value();
    s.loadSteps = m_loadSteps->value();
    s.stiffnessReduction = m_reduction->value();
    s.autoB2 = m_autoB2->isChecked();
    s.rs = m_rs->value();
    s.singleStorey = m_singleStorey->isChecked();
    s.notionalLoads = m_notional->isChecked();
    s.geometricImperfection = m_geometric->isChecked();
    s.imperfectionRatio = 1.0 / m_imperfectionDenominator->value();
    s.notionalFactor = m_notionalFactor->value();
    s.groupByFamily = m_groupByFamily->isChecked();
    s.targetEta = m_target->value();
    s.defaults = read(m_defaults);
    s.defaults.steel.clear();
    QStringList checked;
    QStringList all;
    for (int i = 0; i < m_combinations->count(); ++i) {
        const auto *item = m_combinations->item(i);
        all.append(item->text());
        if (item->checkState() == Qt::Checked) {
            checked.append(item->text());
        }
    }
    s.combinations = checked == DesignService::defaultUltimateCombinations(all) ? QStringList() : checked;
    return s;
}

bool DesignSettingsDialog::applyToSelection() const
{
    return m_selectionGroup && m_selectionGroup->isVisible() && m_selectionGroup->isChecked();
}

DesignMemberParams DesignSettingsDialog::selectionParams() const
{
    return read(m_selected);
}

} // namespace Structura::UI
