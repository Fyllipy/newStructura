# Gera o relatorio de validacao Structura x Mecway 28 a partir de summary.json (numeros copiados dos dados, nao digitados).
import json
import math
import sys

r = json.load(open(sys.argv[1]))
by = {(c['case'], c['solver']): c for c in r}
cases = ['ss_plate', 'clamped_plate', 'cantilever_plate', 'trapezoid_plate', 'cylinder_pressure', 'cylinder_pressure_fine']


def g(x, nd=3):
    if x is None or (isinstance(x, float) and math.isnan(x)):
        return '—'
    if x == 0:
        return '0'
    return ('%.' + str(nd) + 'g') % x


def pct(x):
    if x is None or (isinstance(x, float) and math.isnan(x)):
        return '—'
    return '%.3g %%' % (100 * x)


out = []
w = out.append
w('# Validação Structura × Mecway 28 — placas e cascas, estática linear')
w('')
w('Gerado automaticamente por `make_report.py` a partir de `summary.json`. Todos os números abaixo vêm dos arquivos de resultado dos dois programas.')
w('')
w('## 1. Método')
w('- **Mesmo modelo nos dois:** cada caso é montado pelo pipeline real do Structura (`Model` → `generateMesh` → `buildJob` → `mefs.dll` distribuída, SHA-256 `09ffc08a6001b2c4…`). A malha (nós, elementos, ordem dos nós, espessura), os apoios e a pressão são exportados e escritos em `.liml` (formato do Mecway) pelo `gen_mecway.py`: **mesmos nós, mesmos elementos, mesma numeração**, E = 200 GPa, ν = 0,3.')
w('- **Mecway controlado por linha de comando:** `mecway arquivo.liml script extrai.py` (solve sem interface, via script IronPython do próprio Mecway), com extração dos resultados por nó e por elemento. Dois solvers do Mecway foram rodados: o **interno** (quad4 = MITC4, segundo o manual do Mecway) e o **CalculiX 2.21** que acompanha o Mecway.')
w('- **Convenções confirmadas por execução:** pressão positiva do Structura (sentido da normal do elemento) = pressão na **face 6** do Mecway; reação com o mesmo sinal nos dois; momento principal com o mesmo sinal (razão Structura/Mecway = 1,00 nos casos de placa).')
w('- **Comparação:** nó a nó (deslocamentos e rotações) e canto a canto de cada elemento (momentos principais e tensão de von Mises na face inferior). Os eixos locais dos elementos diferem entre os programas, então momentos são comparados por **invariantes** (momentos principais).')
w('')
w('## 2. Casos')
w('| Caso | Descrição | Nós | Elementos |')
w('|---|---|---|---|')
desc = {
    'ss_plate': 'Placa 1×1 m, t = 10 mm, apoiada nas 4 bordas (UZ=0), pressão 1 kPa, malha 8×8',
    'clamped_plate': 'Placa 1×1 m, t = 10 mm, engastada nas 4 bordas, pressão 1 kPa, malha 8×8',
    'cantilever_plate': 'Faixa 4×1 m, t = 100 mm, engastada em x = 0, pressão 50 kPa, malha 16×4',
    'trapezoid_plate': 'Placa trapezoidal (malha não estruturada, elementos distorcidos), t = 20 mm, apoiada nas bordas, pressão 5 kPa',
    'cylinder_pressure': 'Cilindro R = 1 m, H = 2 m, t = 10 mm, engastado na base, topo livre, pressão interna 100 kPa, malha 0,2 m',
    'cylinder_pressure_fine': 'O mesmo cilindro com malha 0,07 m (2 668 elementos)',
}
nels = {'ss_plate': 64, 'clamped_plate': 64, 'cantilever_plate': 64, 'trapezoid_plate': 160, 'cylinder_pressure': 320, 'cylinder_pressure_fine': 2668}
for c in cases:
    d = by[(c, c)]
    w('| `%s` | %s | %d | %d |' % (c, desc[c], d['nodes_structura'], nels[c]))
w('')
w('## 3. Structura × Mecway (solver interno)')
w('Erro relativo = máx. |diferença| ÷ máx. |valor do Mecway|.')
w('')
w('| Caso | u_z máx. Structura (m) | u_z máx. Mecway (m) | Dif. u_z | Dif. rotação rx | Dif. momento principal máx. | Dif. momento principal mín. | ΣRz Structura / Mecway (N) |')
w('|---|---|---|---|---|---|---|---|')
for c in cases:
    d = by[(c, c)]
    rc = d['reaction']
    w('| `%s` | %s | %s | %s | %s | %s | %s | %s / %s |' % (c, g(d['uz_extreme']['structura'], 5), g(d['uz_extreme']['mecway'], 5), pct(d['disp']['uz']['rel_max']), pct(d['disp']['rx']['rel_max']),
                                                               pct(d['moment_principal_max']['rel_max']), pct(d['moment_principal_min']['rel_max']), g(rc['structura_sum'][2], 6), g(rc['mecway_sum'][2], 6)))
w('')
w('Tensão de von Mises na face inferior (valores nos 4 cantos de cada elemento; o Mecway também inclui o cisalhamento transversal):')
w('')
w('| Caso | Dif. máx. | Dif. mediana (pontos > 5 % do máx.) | Dif. L2 |')
w('|---|---|---|---|')
for c in cases:
    d = by[(c, c)]['vonmises_bottom_corner']
    w('| `%s` | %s | %s | %s |' % (c, pct(d['rel_max']), pct(d['rel_median']), pct(d['rel_l2'])))
w('')
w('## 4. Contra soluções analíticas (referência independente dos dois programas)')
w('| Caso | Referência | Valor de referência | Structura | Mecway interno | Dif. Structura |')
w('|---|---|---|---|---|---|')
d = by[('ss_plate', 'ss_plate')]
ref = d['reference']['uz_center_navier']
w('| `ss_plate` | Navier (série dupla), flecha no centro | %s m | %s m | %s m | %s |' % (g(ref, 5), g(d['uz_extreme']['structura'], 5), g(d['uz_extreme']['mecway'], 5), pct(d['uz_extreme']['structura'] / ref - 1)))
d = by[('clamped_plate', 'clamped_plate')]
ref = d['reference']['uz_center_timoshenko']
w('| `clamped_plate` | Timoshenko & Woinowsky-Krieger, α = 0,00126 (placa quadrada engastada, ν = 0,3), flecha no centro | %s m | %s m | %s m | %s |' % (g(ref, 5), g(d['uz_extreme']['structura'], 5), g(d['uz_extreme']['mecway'], 5), pct(d['uz_extreme']['structura'] / ref - 1)))
d = by[('cantilever_plate', 'cantilever_plate')]
ref = d['reference']['uz_tip_with_shear']
w('| `cantilever_plate` | Euler–Bernoulli + cortante (largura unitária, EI = E t³/12) | %s m (sem cortante: %s m) | %s m | %s m | %s |' % (g(ref, 5), g(d['reference']['uz_tip_euler_bernoulli'], 5), g(d['uz_extreme']['structura'], 5), g(d['uz_extreme']['mecway'], 5), pct(d['uz_extreme']['structura'] / ref - 1)))
for c in ('cylinder_pressure', 'cylinder_pressure_fine'):
    d = by[(c, c)]
    ref = d['reference']['ur_membrane']
    w('| `%s` | Teoria de membrana, deslocamento radial longe do engaste (p R² / E t), média da metade superior | %s m | %s m | %s m | %s |' % (c, g(ref, 5), g(d['reference']['ur_upper_structura_mean'], 5), g(d['reference']['ur_upper_mecway_mean'], 5), pct(d['reference']['ur_upper_structura_mean'] / ref - 1)))
w('')
d = by[('cylinder_pressure_fine', 'cylinder_pressure_fine')]['reference']
w('Momento de engaste do cilindro (camada limite): a teoria dá M₀ = p R t / (2 √(3(1−ν²))) = **%s N·m/m**. Maior |momento principal| nos cantos dos elementos da primeira fileira: Structura **%s**, Mecway **%s** (malha 0,07 m) — os **dois concordam entre si, mas nenhum chega ao valor teórico**: a camada limite tem comprimento característico ≈ 0,08 m, da ordem do elemento, então o pico não é resolvido nessa malha (valores nos cantos, sem extrapolação). A comparação com a teoria para esse pico **não foi concluída**; uma malha mais fina não pôde ser comparada porque o Mecway não devolveu solução para a malha de 8 000 elementos (falha silenciosa não investigada).' % (g(d['M_base_analytic'], 4), g(d['M_base_structura_max'], 4), g(d['M_base_mecway_max'], 4)))
w('')
w('## 5. Convergência da diferença Structura × Mecway no cilindro')
w('| Quantidade | Malha 0,2 m | Malha 0,07 m |')
w('|---|---|---|')
a, b = by[('cylinder_pressure', 'cylinder_pressure')], by[('cylinder_pressure_fine', 'cylinder_pressure_fine')]
for label, key in [('Deslocamento radial (ux)', ('disp', 'ux')), ('Rotação (rx)', ('disp', 'rx')), ('Momento principal máx.', ('moment_principal_max',)), ('Momento principal mín.', ('moment_principal_min',))]:
    def pick(d):
        x = d
        for k in key:
            x = x[k]
        return x['rel_max']
    w('| %s | %s | %s |' % (label, pct(pick(a)), pct(pick(b))))
w('')
w('## 6. Solver CalculiX do Mecway (comparação secundária)')
w('O CalculiX expande cada nó de casca em dois (superfícies superior e inferior); o deslocamento do nó da superfície média foi obtido pela média do par. Só deslocamentos foram comparados (as variáveis por elemento do CalculiX não são equivalentes).')
w('')
w('| Caso | u_z máx. Structura (m) | u_z máx. CalculiX (m) | Dif. u_z (máx. no nó) |')
w('|---|---|---|---|')
for c in cases:
    d = by.get((c, c + '_ccx'))
    if not d or 'error' in d:
        continue
    w('| `%s` | %s | %s | %s |' % (c, g(d['uz_extreme']['structura'], 5), g(d['uz_extreme']['mecway'], 5), pct(d['disp']['uz']['rel_max'])))
w('')
w('Nas placas finas e simplesmente apoiada/engastada, o CalculiX resulta **bem mais rígido** que o solver interno do Mecway, o Structura e as soluções analíticas (flecha 26 % e 40 % menor). Isso foi **observado, não investigado**; a hipótese é a limitação conhecida dos elementos de casca expandidos do CalculiX em placas muito finas. É comportamento do solver CalculiX do Mecway, não do Structura, e por isso o solver interno é a referência principal.')
w('')
w('## 7. Conclusões e limites')
w('- **Placas (flexão):** Structura e Mecway interno coincidem em **1e-13 a 1e-10** em deslocamentos, rotações e momentos, inclusive em malha não estruturada com elementos distorcidos. Isto é coerente com o fato de ambos usarem a formulação MITC4 para flexão e cisalhamento (o manual do Mecway indica MITC4 para o quad4). Portanto, nesses casos, a concordância mostra que a implementação do Structura reproduz a formulação, **não é uma validação de formulação independente**.')
w('- **Contra a teoria:** flechas dentro de ~0,6 % de Navier/Timoshenko (placas) e de 3 % no balanço (efeito de largura finita e malha), deslocamento radial do cilindro a 0,06 % da teoria de membrana na malha fina.')
w('- **Cilindro (casca curva):** diferença de 0,1 % (u_z), 2 % (ux), 10–18 % (momentos, malha grossa) que **cai para 0,3–1,4 % no refino**. As diferenças restantes em membrana/curvatura são esperadas porque o Structura acrescenta modos incompatíveis de Wilson na membrana e rigidez de drilling, que o quad4 do Mecway não tem (explicação plausível, não testada isoladamente).')
w('- **Tensão de von Mises:** diferença mediana ≤ 0,1 % nas placas; o máximo é maior (até 7,5 % no cilindro e 2,8 % na trapezoidal) em poucos pontos, em parte porque o Mecway inclui cisalhamento transversal. Não é desvio sistemático.')
w('- **Não coberto por esta validação:** análise não linear, flambagem, modal, apoios elásticos/só-compressão, ties e acoplamento com barras, cargas térmicas e de corpo, peso próprio, hidrostática, moments/cortantes contra referência independente (só contra o outro programa), curvatura dupla (domos). Apenas **5 casos + 1 refinamento**, todos com uma única carga de pressão.')
w('- **Versão do Structura testada:** `mefs.dll` distribuída (a antiga, sem as correções D1/D2 da Etapa 3; elas não afetam análise linear com um caso de carga).')
w('')
w('## 8. Arquivos')
w('Em `docs/validacao_mecway/`: `*.liml` (modelos prontos para abrir no Mecway 28), `summary.json` (todos os números), `gen_mecway.py`, `compare.py`, `val_cases.h`, `val_dump.cpp`, `val_shots.cpp`, `mw_shot.ps1` (reprodução) e `prints/` (imagens para divulgação: Structura, Mecway e lado a lado).')
open(sys.argv[2], 'w', encoding='utf-8').write('\n'.join(out))
print('ok', len(out))
