# Dinâmica e recursos de uso

## Dinâmica (Carregamentos > Análise > Dinâmica)

Todas as análises partem dos modos de vibração calculados no próprio diálogo (fonte de massa = combinação escolhida,
normalmente a quase permanente; massa em t = kN·s²/m, concentrada nos nós; inércia rotacional desprezada).

- **Espectro de resposta**: espectro paramétrico (patamar, 1/T e 1/T²) ou tabela T × Sa/g, fatores R e I, amortecimento ζ,
  combinação modal **CQC** (Der Kiureghian) ou SRSS, direções X/Y/Z combinadas por SRSS ou 100% + 30%.
  Cada modo vira um caso estático de forças equivalentes `F = m·Γ·φ·Sa` resolvido pelo solver; os resultados por modo
  (deslocamentos, reações e esforços locais, com sinal) são combinados. O resultado combinado é uma envoltória em valor absoluto.
  Mostra massa efetiva acumulada (aviso abaixo de 90%), cortante basal por modo e combinado, deslocamentos, esforços e CSV.
  *A NBR 15421 não está em `normas/`: o usuário informa o espectro de projeto.*
- **Vibração de piso (NBR 8800:2024, Anexo I)**: usa a frequência e a massa modal do modelo de elementos finitos (avaliação precisa, I.3)
  com as equações de I.2.4: `a_p = 0,55·P0·e^(−0,35 f0)/(2·M·β)` e `a_w,rms = ζ·P0·W/(2√2·M·β)`, `R = a_w,rms/0,005`.
  β soma sistema estrutural (0,010), ocupação, forros e divisórias (Tabela I.2); limites da Tabela I.3; f0 ≥ 3 Hz (I.1.6).
  A forma modal é normalizada com valor 1 no nó de maior deslocamento vertical. Para o caminhar de pessoas a norma pede a combinação
  frequente com ψ = 0,1 (I.1.5): escolha a fonte de massa correspondente.
- **Resposta no tempo**: superposição modal com integração de Newmark (aceleração média), carga harmônica, pulso de meio seno ou tabela;
  deslocamento, velocidade e aceleração no nó escolhido, fator dinâmico e aceleração em % g.

Limites: sem vento dinâmico (NBR 6123, cap. 9 / NBR 8800 Anexo J), sem sismo da NBR 15421 pronto, resultado do espectro não entra
automaticamente nas combinações de dimensionamento (a combinação modal perde o sinal).

## Edição

- **Excluir** (Delete) nós e barras (barras ligadas a um nó excluído saem junto); **Inserir nó** com snap ao longo da barra divide a barra;
  **Dividir barras** em N partes iguais, a cada X m ou em distâncias informadas; **Medir** distância, deltas e inclinação.
  Tudo com desfazer; perfil, cargas uniformes, grupos, posicionamento e ligações acompanham os trechos.
- **Cubo de vistas** (canto superior direito) e **vistas salvas** (também aparecem nas pranchas como vistas 3D).

## Sessão

- Título com ● quando há alterações não salvas, confirmação ao fechar/abrir/criar novo, **recuperação automática** a cada 90 s
  (`%LOCALAPPDATA%/.../recovery`), oferecida na próxima abertura após um fechamento inesperado.
- Aviso "Resultados desatualizados" e diagramas esmaecidos quando o modelo muda depois da análise.
- Erros do solver traduzidos, com o que fazer e atalhos (Checar modelo, Ir ao nó).

## Resultados e documentos

- **Diagramas**: hachura, contorno escuro, cartão de legenda (máx/mín, barra e escala em kN/m desenhado), rótulos "Extremos" ou "Por barra",
  valores com o sinal físico de My e Mz, e leitura sob o mouse com os seis esforços na seção.
- **Passar o mouse** em nós/barras mostra dados do elemento, deslocamentos/reações, esforços máximos e η.
- **Comparar combinações**: grandeza escolhida × combinações, envoltória e combinação que governa, CSV.
- **Folhas compostas** das pranchas salvas no projeto (`[SHEET_LAYOUTS]` do .dat) com posição manual (% da área útil) por vista.

## Desempenho e estabilidade

- **Janela de progresso** ("Calculando…", nome do cálculo, etapa e barra) em análise estrutural, modal, flambagem, espectro, resposta no tempo,
  verificação/otimização de perfis e geração de pranchas. Os cálculos rodam em outra thread (`BusyProgress.h`, `runBusy`); a janela só
  aparece se a tarefa passar de ~250 ms e a interface fica sem aceitar cliques durante o cálculo (autosave e dicas do mouse são suspensos).
- **Dimensionamento**: os esforços, B2 e flechas ficam em cache enquanto modelo, resultados e parâmetros não mudam (trocar o perfil por duplo
  clique deixou de refazer tudo); a verificação de todos os perfis candidatos e de todas as barras é paralela; `findSteel` não monta mais a lista de aços.
- **Diagramas**: uma malha por categoria (4 atores no total, em vez de milhares), sem filtro de normais, 6 amostras/m (24 a 120) e escala com
  agrupamento de movimentos do controle deslizante; os carregamentos ficam ocultos enquanto há diagrama na tela.
- **Renderização** da cena agrupada (vários pedidos viram um único `Render()`); leitura sob o mouse limitada a ~30 Hz.
- **Rótulos de carga** na cor do desenho, em etiquetas, por prioridade de valor, sem sobreposição e sem repetir o mesmo valor por perto.
- **Solido extrudado**: a seleção destaca o sólido inteiro (só as cores são atualizadas).
- **Marca**: executável sem console, ícone embutido, tela de abertura trocável (`images/splash.png`, `icons/app/*`; regenerar com `tools/gen_branding.ps1`
  e `tools/make_ico.py`).

## Vento dinâmico (Dinâmica > Vento dinâmico)

Modelo discreto da NBR 6123:2023, 9.4 (a subseção 9 é a que o Anexo J da NBR 8800 referencia): níveis = planos de nós de mesma cota (cota 0 = menor Z), massa e forma modal de cada nível vêm do modo escolhido. Componente média X̄ᵢ = q₀ b̄ₘ² Cₐ Aᵢ (zᵢ/zᵣ)^2p e amplitude flutuante X̂ᵢ = F_H ψᵢ xᵢ, com F_H = q₀ b̄ₘ² A₀ (Σβᵢxᵢ / Σψᵢxᵢ²) ξ e V̄p = 0,69 V₀ S₁ S₃. O **fator dinâmico ξ vem dos ábacos (Figuras 20 a 24) e é informado pelo usuário** (no 2º modo, já multiplicado por C₂). A resposta transversal é Y = X/3 (9.5). *Criar caso(s) de carga* grava as forças nodais (repartidas pela massa dos nós de cada nível) em casos `VENTO DIN X M1` (e `… TRANSV`). Com dois modos, crie o caso do 1º completo e o do 2º só com a flutuante e combine pela raiz da soma dos quadrados (9.4.4). Não calcula conforto humano (9.6), torção nem edificações fora do campo de aplicação (f < 0,2 Hz, h > 200 m).

## Folhas compostas: posicionamento com o mouse

Na prévia de uma folha composta, clique numa vista para selecioná-la, arraste para movê-la e use as alças (cantos e bordas) para redimensionar. A posição fica manual (frações da área útil) e é gravada no projeto; os campos numéricos continuam disponíveis. A folha é regenerada ao soltar o mouse.

## Apoios x restrições nodais, novo projeto e propriedades

- **Apoio** é todo nó que restringe o deslocamento vertical (Z): engaste (6 graus), articulado (translações) e móvel (rolos ou esferas nos graus livres). **Restrição nodal** é qualquer outra combinação (por exemplo travar só Y ou só uma rotação): barras com batentes por translação e anéis por rotação, em laranja. Cada grupo tem o seu botão na barra de exibição.
- **Novo projeto** abre um assistente (identificação para o carimbo, aço padrão, casos de carga e ponto de partida: em branco ou modelo pronto).
- **Propriedades da barra** lista os carregamentos atuantes (peso próprio, cargas distribuídas, momentos e cargas de área/vento por caso). O botão *Ocultar rótulos dos carregamentos* só esconde os valores (desligado por padrão). O esmaecimento de outros níveis foi removido.
