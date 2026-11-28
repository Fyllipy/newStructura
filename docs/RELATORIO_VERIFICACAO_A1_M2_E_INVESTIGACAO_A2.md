# Verificação funcional de A1/M2 e investigação técnica do A2

Este relatório **não** declara validação profissional do módulo. Os números do item A2 são comparações indicativas (um exemplo, uma geometria) e a formulação proposta ainda não foi implementada nem validada contra referência independente.

## Parte 1 — Verificação funcional de A1/M2

### Como foi feito (e o que NÃO foi feito)
- Não foi possível operar a janela principal do aplicativo (não há ferramenta de controle de interface nesta sessão). Em vez disso, foi usado o **mesmo `NodeConnectionsDialog` do aplicativo**, instanciado fora da tela, com os cenários pedidos, lendo o que ele mostra (veredito, lista, tabela de verificações), acionando o botão real **"Copiar memorial"** e gravando imagens do diálogo. As imagens saem **sem fontes** (o ambiente fora da tela não as tem: texto vira quadradinhos); o texto foi lido dos widgets, não das imagens. A imagem confirma o leiaute (veredito em âmbar com várias linhas, linha vermelha na tabela).
- A persistência foi exercitada com o `DatModelIoService` real (salvar, carregar, reavaliar e reabrir o diálogo).
- **Não exercitados**: o relatório PDF/DOCX (`ReportServiceDesign`), a legenda dos desenhos (`ConnectionDrawing::etaLabel`) e a janela principal. Para esses, só há compilação e leitura de código. Eles continuam pendentes de verificação visual por quem abrir o aplicativo.
- A sonda usada está fora do repositório (scratchpad); não faz parte da entrega.

### Resultados por cenário
| # | Cenário | Veredito no diálogo | Memorial | Observação |
|---|---|---|---|---|
| 1 | Sem barra e sem geometria | NÃO VERIFICÁVEL — DADOS INSUFICIENTES | `RESULTADO DA LIGAÇÃO: NÃO VERIFICÁVEL`, causa listada (d, bf, tf, tw, fy, fu = 0) | A lista da esquerda mostra "NÃO VERIFICÁVEL — dados insuficientes" no lugar de η |
| 2 | Manual **não confirmada** | NÃO VERIFICÁVEL | Origem: "geometria informada manualmente, NÃO CONFIRMADA" | OK |
| 3 | Manual **confirmada** (HEB200) | Atende (0,882) | Item "Seção do pilar utilizada": origem, d/bf/tf/tw/r e aço | OK |
| 4 | Dimensão alterada **após** confirmar | ver D1 | | **Defeito D1** (corrigido) |
| 5 | Confirmada, mas `tw = 0` | NÃO VERIFICÁVEL (tw deve ser > 0) | | A confirmação não "libera" dado inválido |
| 6 | Reprovada (M = 150 kN·m) | NÃO atende (1,710 — alavanca da chapa) | Linhas OK / NAO ATENDE | OK |
| 7 | Aprovada (barra de apoio vinculada, esforços baixos) | Atende (0,882) | `RESULTADO DA LIGAÇÃO: APROVADA`, origem "barra 7 (seção efetiva do modelo)" | OK |
| 8 | Emenda viga-viga | Atende (0,882); sem verificações de pilar e sem "Seção do pilar" | | Usa a geometria fixa de "outra chapa" (limitação conhecida, declarada no código) |
| 9 | Salvar → reabrir projeto → reabrir diálogo | Mesmo veredito dos cenários originais | | Valores (incl. `supConfirmed`) preservados; η = 1e9 gravado e relido; ligação sem dados volta NÃO VERIFICÁVEL |

### Defeitos encontrados (documentados antes de corrigir)
- **D1 (funcional, A1) — a confirmação sobrevivia à alteração das dimensões.** No cenário 4, `tf` foi alterado de 15 para 6 mm pela interface e `supConfirmed` continuou 1: a geometria "confirmada" já não era a que o projetista confirmou. **Corrigido**: `supConfirmedSig` guarda uma assinatura das dimensões e do aço na hora da confirmação; o diálogo volta a campo para "Não confirmada" quando qualquer dimensão muda; `toEndPlate` recusa a confirmação cuja assinatura difere ("geometria do pilar ALTERADA após a confirmação: confirme novamente"). Confirmações antigas sem assinatura (arquivos salvos antes) adotam as dimensões atuais ao abrir o diálogo. Teste novo: `confirmationIsInvalidatedWhenDimensionsChange`. Reverificado na sonda: após a edição, `supConfirmed = 0` e o veredito é NÃO VERIFICÁVEL, também após salvar e reabrir.
- **D2 (texto) — a mensagem citava um rótulo que não existe** ("Geometria do pilar confirmada"). **Corrigido** para o rótulo real do campo e a opção "Confirmada pelo projetista".
- **D3 (visual) — o veredito de NÃO VERIFICÁVEL ocupava ~7 linhas** (copiava a primeira causa inteira). **Corrigido**: texto curto com a contagem de ocorrências; as causas ficam na dica (tooltip) e na tabela.
- **Observação (não corrigida):** os campos de geometria do pilar aceitam 0 e a ligação sem barra de apoio abre com a sugestão de aço do pilar igual ao da chapa (`supFy/supFu`); isso só vale se o projetista confirmar a geometria, mas vale revisar a UX (por exemplo, sugerir o aço ou deixar em branco).
- **Observação:** ao vincular uma barra de apoio depois da confirmação manual, a confirmação deixa de ser usada (a seção da barra prevalece); não há aviso visual dessa troca.

Testes automatizados após as correções: 30/30 (`TestInputValidation` agora com 11 casos).

## Parte 2 — Investigação técnica do A2 (nenhuma implementação)

### 2.1 Mecânica
**Transferência.** Em ligação rígida por chapa de extremidade, o momento da viga vira um binário nas mesas (T = M/z). A parte tracionada puxa a chapa, que traciona os parafusos, que tracionam a **mesa do pilar** do lado oposto. A mesa do pilar e a chapa formam, em cada fileira, um "tê" (T-stub): a alma do pilar (e a solda mesa-alma/raio) é a "aba" que recebe a carga; os parafusos puxam a mesa em dois pontos afastados da alma.

**Deformação e alavanca.** A mesa flete como placa apoiada na alma. Se for flexível, as bordas se aproximam da chapa e o contato na borda cria uma reação adicional Q (alavanca), somada à tração do parafuso (B = T/2 + Q). Quanto mais fina a mesa, maior Q e menor a resistência efetiva. O mesmo ocorre na chapa de extremidade (que o programa já verifica); aqui o que falta é o lado do pilar.

**Fatores geométricos.** `m` = distância do eixo do parafuso à face da alma descontado o raio (aqui m = (g − tw)/2 − 0,8 r = 26,1 mm); `e` = distância do eixo do parafuso à borda livre ((bf − g)/2 = 55 mm; a alavanca usa n = min(e; 1,25 m)). A espessura da mesa entra ao quadrado na plastificação; um gabarito g maior aumenta m e reduz a resistência; folga de borda grande (e) aumenta a capacidade do modo 2 até o limite de 1,25 m.

**Interação entre fileiras.** Fileiras próximas (pitch pequeno) formam um mecanismo de grupo (linha de plastificação contínua), com comprimentos efetivos diferentes da soma das fileiras isoladas. Fileiras com pitch grande atuam isoladas. A soma de fileiras isoladas pode superestimar a capacidade.

**Modos de ruptura (T-stub):** (1) plastificação completa da mesa (rótulas nos dois lados do parafuso) com parafusos intactos; (2) plastificação na alma + ruptura dos parafusos (com alavanca); (3) ruptura dos parafusos sem plastificação da mesa. Há ainda estados que a alavanca não cobre: punção/arrancamento localizado, cisalhamento do painel, flambagem da alma comprimida (esses já têm verificações em 5.7).

### 2.2 Fundamentação normativa (NBR 8800:2024, texto lido em `normas/`)
- **6.3.5.1** exige considerar "o efeito de alavanca produzido pelos deslocamentos e deformações das **partes ligadas**" na tração dos parafusos e permite "quaisquer métodos" que atendam a 6.1.1.3; **6.3.5.3 (placa rígida) e 6.3.5.4 (placa flexível)** atendem a esses requisitos. As expressões geram a espessura mínima de "uma placa" com geometria da Figura 15 (b, a, p). A norma **não limita** esses métodos à chapa de extremidade: a mesa do pilar é parte ligada deformável.
- **5.7.2** (flexão local da mesa, `R_d = 6,25 tf² fy/γa1`) trata de **força transversal localizada aplicada na face externa da mesa**, perpendicularmente, com comprimento de atuação; é a verificação de uma chapa soldada, não de duas linhas de parafusos afastadas da alma. Usá-la como substituto da alavanca é interpretação do programa, não exigência textual.
- A NBR 8800:2024 **não define T-stub**, comprimentos efetivos, modos 1-2-3 nem comportamento de grupo de fileiras (busca no texto por "T-stub"/"tê equivalente": nada). O método do T-stub é da **EN 1993-1-8 (6.2.4, 6.2.6.4)**, aqui usado **somente como método complementar** (de referência), sempre rotulado assim. É aplicável a mesas de perfis I/H com parafusos fora da alma; não substitui a verificação normativa quando esta existir.
- O que é **exigência expressa da NBR**: considerar alavanca nas partes ligadas (6.3.5.1) e usar um método compatível (6.3.5.3/6.3.5.4 são os dois dados). O que exige **metodologia complementar**: comprimentos efetivos da mesa do pilar, interação entre fileiras, e o equivalente em modos.

### 2.3 Comparação numérica (indicativa)
Exemplo da auditoria: HEB200 (bf = 200, tf = 15, tw = 9, r = 18, fy = 250 / fu = 400 MPa), parafusos M20 A325 (Ft,Rd = 144,86 kN pela NBR 6.3.3.1), g = 90 mm, pitch = 70 mm. Programa: sonda sobre `checkEndPlate` real; T-stub: cálculo em Python (EN 6.2.4/6.2.6.4, γM0 = 1,10). Valores em kN, força total de tração na linha (T).

| Critério | 1 fileira (2 parafusos) | 2 fileiras (4 parafusos) |
|---|---|---|
| Parafusos (Σ Ft,Rd) | 289,7 | 579,4 |
| **Structura hoje**: maior T aceito | **289,7** (limitado pelos parafusos; 5.7.2 dá 319,6) | **319,6** (limitado por 5.7.2 contra o T total) |
| 6.3.5.4 aplicado à mesa (b = m, a = e, p = 90) | 273,0 | 546,0 (sem interação de fileiras) |
| T-stub EN, modo 1 / 2 / 3 | 321 / **236** / 290 | grupo: 476 / **428** / 579; isoladas somadas: 643 / 473 / 579 |
| Resistência T-stub (mínimo) | **236,3** | **427,8** (grupo) |

Leitura:
- **1 fileira:** o programa aceita até 289,7 kN, **22,6 % acima** do T-stub (236,3) e **6 % acima** do 6.3.5.4 aplicado à mesa (273). O efeito de alavanca (modo 2) reduz a capacidade abaixo da dos parafusos: confirma o A2 do relatório da auditoria.
- **2 fileiras:** o programa limita-se a 319,6 (5.7.2 contra o T total) — **25 % abaixo** do T-stub (427,8): conservador, não inseguro. A soma de fileiras isoladas (473) seria 10 % acima do grupo: o grupo precisa ser considerado.
- A aplicação direta do 6.3.5.4 a cada fileira sem tratar a interação (546 kN) **superestima** frente ao grupo EN (428): não basta replicar a verificação da chapa.
- Sensibilidade: com γM0 = 1,00 o T-stub 1 fileira vai a 243,9; a conclusão qualitativa não muda. Os números dependem de fy da mesa, do raio e do gabarito; **um exemplo não valida a formulação**.

### 2.4 Proposta (a aprovar antes de implementar)
**Formulação.** Duas camadas, ambas rotuladas:
1. *Normativa (obrigatória)*: aplicar 6.3.5.4 também à mesa do pilar, por fileira, com b = m, a = e (≤ 1,25 b), p = largura tributária; esse resultado alimenta a razão da verificação "alavanca – mesa do pilar" (NBR 8800:2024, 6.3.5).
2. *Complementar (EN 1993-1-8, 6.2.4 e 6.2.6.4)*: T-stub com modos 1/2/3, comprimentos efetivos por fileira e por grupo de fileiras, **só como verificação adicional** e rotulada "método complementar (EN)". Aprovação global considera a mais desfavorável das duas camadas **somente depois** de validada; até lá, a camada EN pode ser informativa.

**Hipóteses.** Mesa de perfil I/H sem enrijecedor na altura da fileira (com enrijecedor: comprimentos efetivos diferentes, tratar separadamente); parafusos simétricos em relação à alma; sem tração axial no pilar que reduza a mesa; raio de concordância informado; fy/fu do pilar da seção usada (A1).

**Componentes envolvidos.** Entradas já existentes (colFlangeWidth, colFlangeThickness, colWebThickness, colRadius, gauge, pitch, tensionRows, aço do pilar); novos cálculos de `m`, `e`, `leff`; reaproveitar `checkPrying` (6.3.5.4) para a camada 1; novo módulo de T-stub (isolado, reutilizável pela ferramenta momento-rotação, que hoje usa seu próprio modelo de componentes).

**Integração.** Em `checkEndPlate`, após os parafusos: adicionar "Alavanca – mesa do pilar (6.3.5)" e, opcionalmente, "T-stub da mesa do pilar (EN, complementar)"; entradas validadas pela camada de `InputValidation` (não verificável se faltar dado); estados e memória como em M2; fórmulas e origem das dimensões na memória.

**Testes necessários.** (i) Casos de um e dois parafusos por fileira contra o exemplo acima e contra exemplos publicados independentes (SCI P398 / exemplos do Eurocode 3 com T-stub; exemplo de 6.3.5 do próprio texto da norma, se existir); (ii) caso de transição entre modos 1, 2 e 3 variando tf; (iii) interação de fileiras (pitch 40, 70, 150 mm); (iv) invariância do resultado quando a mesa é "rígida" (sem alavanca); (v) `InputValidation` (tf, raio, gabarito inválidos → NÃO VERIFICÁVEL); (vi) golden: valores das demais verificações inalterados; (vii) comparação com análise de elementos finitos (modelo de casca do próprio Structura ou Mecway) de uma fileira — somente para cotejo.

**Referências de validação independentes.** EN 1993-1-8 §6.2.4/6.2.6.4 e SCI P398 (exemplos resolvidos); resultados de um solver MEF independente; literatura de T-stub (Zoetemeijer; Kulak/Fisher/Struik para o equivalente em placa). As citações de literatura aqui são **de memória e precisam ser conferidas** antes de entrarem como critério.

**Limitações.** T-stub aplica-se a mesas planas de perfis I/H com parafusos afastados da alma e, para fileiras extremas, a regras diferentes; não cobre mesa enrijecida (diferente), pilar tubular, tração axial combinada, nem protensão relevante; fatores parciais EN (γM0, γM2) diferem dos da NBR; misturar fórmulas EN com γ da NBR é uma escolha que **precisa ser aprovada**; o 6.3.5.4 foi deduzido para placa simples e sua extensão à mesa de perfil laminado (com raio e alma como apoio) é uma hipótese conservadora que também deve ser validada.

## Pendências
1. Verificar visualmente, na janela real do aplicativo, o relatório (memória), a legenda dos desenhos e a barra de status das ligações.
2. Decidir a UX do aço do pilar sugerido e do aviso ao trocar entre geometria manual e barra de apoio.
3. Aprovar a formulação do A2 (camadas, γ, rótulo) e a estratégia de validação; só então implementar. A3 e demais itens da auditoria seguem abertos.
4. Validação profissional do módulo: **não realizada**; este relatório não a substitui.
