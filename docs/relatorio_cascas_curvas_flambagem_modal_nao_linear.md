# Cascas curvas: o que o Structura calcula (estática, flambagem, modal, não linear) e validação contra o Mecway 28

Data: 2026-11-24. DLLs, `.enc` e executáveis comerciais intactos (hashes `f40c09a9d032`, `09ffc08a6001`, `c3a9b8b2c3b6`, `ff6c9550e2d2`). Harness: `docs/validacao_mecway/curvas/` (`cb_dump.cpp`, `gen_cb.py`, `cb_shots.cpp`, `compose_side.ps1`; modelos do Mecway e resultados em `modelos_cilindro/`; prints em `prints/cilindro_*`).

## 1. O que o programa calcula em casca (verificado no código e por execução)

| Análise | Casca pura (`mefs.dll`) | Casca acoplada / com barras (`rds.dll`) |
|---|---|---|
| Estática linear | sim | sim |
| Flambagem linear (K0 + λKg) | **não** | **sim**; o Kg da casca só tem o termo de w, com a força de membrana do **centro** do elemento (`shellGeometricStiffness`) |
| Segunda ordem (P-Δ) | não | sim, com o mesmo Kg: sem rotações finitas |
| Modal | não | sim; massa de casca **concentrada** (ρ t A/4, só translações) |
| Não linear de grandes deslocamentos | **não** | **não** (o motor incremental trata rótulas plásticas e ligações; a casca entra como linear) |
| Não linearidade de material (plasticidade) | não | não |

Prova do último ponto (`s_nonlinear.txt`): tira em balanço de casca com carga na ponta, PL²/EI = 1 (elástica de referência: flecha 0,3017 L, encurtamento 0,0560 L): linear e "segunda ordem" dão **o mesmo** −0,3333 L (+10,5 % sobre a elástica) e encurtamento nulo. Então nenhum dos dois captura grandes rotações em casca.

Limitação de produto encontrada: a ponte `SolverBridge` descartava as formas de flambagem e modais dos nós de casca (só as das barras eram devolvidas); a UI só desenha formas de barras. **Corrigido na ponte**: novos campos `bucklingShellShapes` e `modalShellShapes` (6 valores por nó de casca), com verificação em `TestShellCoupling` (14/14 passam). **A janela ainda não desenha modos de casca**: isso é um recurso novo (ver adiante).

## 2. Validação contra o Mecway 28 (cilindro de parede fina, mesma malha)

Cilindro R = 1, L = 2, t = 0,01, aço (E = 2,1e11, ν = 0,3, ρ = 7850), malha de quadriláteros por revolução. Base: ux = uy = uz = 0; topo: ux = uy = 0, com anel de forças axiais (linha de 1 N/m no total). Mecway: B30 (flambagem) e V30 (frequências), solver interno. No Mecway uma força em seleção de nós é o total repartido (por isso o total é a soma dos nós).

**Flambagem (modo 1, carga crítica em N/m; clássica E t²/(R√(3(1−ν²))) = 12,710e6):**

| elementos | Mecway | Structura | Structura/Mecway |
|---|---|---|---|
| 352 nós | 18,68e6 | 19,25e6 | +3,1 % |
| 768 nós | 15,36e6 | 15,75e6 | +2,5 % |
| 1344 nós | 14,14e6 | 14,49e6 | +2,5 % |
| 2760 nós (2668 elem.) | 13,30e6 | 13,59e6 | **+2,1 %** |

Os dois convergem para baixo; o Structura fica 2 a 3 % acima do Mecway (contra a segurança) em todas as malhas e o desvio cai devagar. Contra a clássica, na malha fina: Mecway +4,7 %, Structura +6,9 % (a restrição circunferencial das bordas eleva a carga crítica nos dois). **Causa provável** (não isolada por teste): o Kg do Structura só tem w e usa força de membrana do centro do elemento; o do Mecway é completo. Os modos aparecem em pares iguais nos dois programas (1–2, 3–4, 5–6).

**Frequências (modo 1, Hz):**

| nós | Mecway | Structura | Structura/Mecway |
|---|---|---|---|
| 352 | 129,98 | 119,77 | −7,9 % |
| 768 | 122,00 | 117,66 | −3,6 % |
| 1344 | 119,35 | 116,96 | −2,0 % |
| 2760 | 117,60 | 116,51 | **−0,9 %** |

Na malha fina, os 8 primeiros modos: Mecway 117,60 / 119,18 / 140,03 / 141,69 Hz (pares); Structura 116,51 / 117,58 / 137,46 / 140,87 Hz (−0,9 %, −1,3 %, −1,8 %, −0,6 %). A diferença decresce com o refino (massa concentrada do Structura contra massa consistente do Mecway).

Prints (mesma malha, mesmo renderizador e escala em faixas do Structura; formas normalizadas): `cilindro_flambagem_{structura,mecway,comparacao}.png`, `cilindro_modal_{structura,mecway,comparacao}.png`. **O print do Mecway é o resultado dele renderizado no Structura, não uma captura da janela do Mecway** (ver abaixo). O sinal e a orientação dos modos duplos são arbitrários em cada programa.

## 3. Não feito / limites

- **Não linear:** não há comparação com o Mecway porque o Structura não tem a análise; o Mecway faz não linear de casca (grandes deslocamentos) via CalculiX, não testado aqui.
- Um único cilindro e uma única condição de contorno; pinched cylinder, hemisfério e Scordelis-Lo não foram rodados em flambagem/modal. Referência clássica da flambagem é de memória.
- A convergência do Kg (2 %) não foi investigada além da hipótese; melhorar o Kg exigiria alterar `shell_q4.cpp` e reconstruir `rds.dll` (fora do escopo sem sua autorização).
- Capturas de tela da janela do Mecway foram **suspensas**: a tentativa de captura automática pegou o navegador do usuário (o Mecway não ficou em primeiro plano). A imagem foi apagada e `mw_shot.ps1` agora aborta sem clicar nem capturar se a janela do Mecway não estiver em primeiro plano.
