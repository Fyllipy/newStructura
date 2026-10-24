# cype2structura — conversor CYPE 3D (.ed3) → Structura (.dat)

Lê os projetos do CYPE 3D (`.ed3`; testado em arquivos salvos pelas versões 2020-f, 2021-g, 2022-h e 2026-a — os exemplos de 2008/2011
da instalação, de outro módulo, não são lidos) e grava o
modelo no formato `.dat` do Structura, sem alterar o Structura nem os arquivos do CYPE.

## Uso

```
python -m cype2structura "D:\CYPE Ingenieros\Projetos\CYPE 3D"              # converte a pasta toda -> .\saida\*.dat
python -m cype2structura "D:\...\Restaurante.ed3" -o C:\temp\Restaurante.dat
```

Opções: `--combinacoes` (uma combinação unitária por caso, veja abaixo), `--sem-estabilizar`,
`--madeira-e-gpa`, `--madeira-g-gpa`, `--madeira-peso`. Requer Python 3.10+; `numpy` é usado só na verificação de estabilidade.

## O que é convertido

| CYPE 3D | Structura (`.dat`) |
|---|---|
| Nós (planos X/Y/Z da grade) | `[NODES]` (m) |
| Barras e peças | `[MEMBERS]` (id do CYPE preservado) |
| Perfis (W/IPE/HE, UPE/U, L, U e C formados, tubo circular, barras redondas, madeira maciça/caibros) | `[SECTIONS]` + `[SECTION_GEOMETRY]`, **somente famílias que já existem** no Structura. W da Gerdau usam as dimensões do catálogo do Structura; A, I e J vêm da biblioteca embutida no `.ed3` quando existe, senão da geometria nominal |
| Vinculação exterior (livre, articulada, engastada, em reta, em plano) | restrições dos nós |
| Vinculação interior dos nós (rígida / articulada / genérica por grupos de peças) | liberações de extremidade (`[MEMBER_PLACEMENT]`); barras biarticuladas viram "Treliça" |
| Hipóteses (nomes) | `[LOAD_CASES]` (+ `PP` com peso próprio) |
| Cargas superficiais dos painéis (tf/m² → kN/m²) | `[MEMBER_LOADS]` (distribuição nas barras de apoio, como o Structura faz) e `[AREA_LOADS]` |

## Premissas que valem conferir

* **Unidades**: o CYPE 3D (Brasil) grava em tf e m; o conversor multiplica por 9,80665 → kN.
* **Aço**: E = 2,1×10⁷ tf/m², ν = 0,3, γ = 7,85 tf/m³ (valores gravados no arquivo). O grau de aço (fy/fu) não foi localizado no
  `.ed3`; `fy`/`fu` ficam em 0.
* **Madeira**: classe não localizada no arquivo; usa C24 (E = 11 GPa, G = 0,69 GPa, γ = 4,2 kN/m³) — ajuste com `--madeira-*`.
* **Cargas de painel**: CP e SC atuam na vertical (gravidade) sobre a superfície; as hipóteses cujo nome começa com "V"/"vento" atuam
  perpendiculares ao painel (pressão positiva contra a superfície). Cada painel reparte a carga nas barras coplanares de apoio
  pela largura de influência (conserva a carga total). Cargas negativas geram aviso.
* **Combinações**: o CYPE gera as combinações pela norma e elas não são convertidas. `--combinacoes` grava uma combinação unitária
  por caso só para o Structura conseguir analisar.
* **Mecanismos**: o CYPE tolera mecanismos que as cargas não excitam; o Structura não. O conversor detecta e fixa as rotações
  que formam esses mecanismos (cada uma é listada nos avisos). `--sem-estabilizar` desliga isso.

## Limitações conhecidas

* Barras de concreto, pilares/vigas "por vigas" e ligações (blocos depois das *uniões* no arquivo) não são lidos: nesses projetos
  (ex.: `Anexo Veloo 2`) as barras sem descrição de perfil são omitidas e o aviso lista cada uma.
* Vinculações externas "definidas pelo usuário", giro da seção, excentricidades, nomes de grupos, parâmetros de
  dimensionamento e resultados do CYPE não são convertidos.

## Como foi feito

O `.ed3` é um fluxo binário proprietário. O layout e o código de leitura (inclusive os ramos por versão) foram extraídos do próprio
CYPE 3D com o Ghidra e empacotados em `cype2structura/lee_funcs.json.gz` (árvores sintáticas das funções `*_lee`), interpretadas
por `cype2structura/lee_interp.py`. A pasta `tools/` guarda o material de engenharia reversa (scripts do Ghidra, extratores,
validação com o solver `rds.dll` do Structura e detector de mecanismos).

## Validação feita

Os 14 projetos de `D:\CYPE Ingenieros\Projetos\CYPE 3D` convertem; os modelos foram abertos no Structura e analisados; com o
solver do Structura (`rds.dll`) o equilíbrio fecha (reações = cargas aplicadas) em todos os casos testados. Isso valida a
geometria, os apoios, as liberações e a distribuição das cargas **dentro do Structura**, não a equivalência com os resultados do
CYPE (os resultados do CYPE estão no `.ed3`, mas não foram lidos).
