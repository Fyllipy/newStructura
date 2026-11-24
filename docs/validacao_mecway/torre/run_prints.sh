#!/bin/bash
# uso: run_prints.sh <tower_shots.exe> <geom.csv (caminho ASCII)> <pasta_structura> <pasta_mecway> <pasta_saida>
T="$1"; G="$2"; ST="$3"; MW="$4"; O="$5"
mkdir -p "$O"
GEO="Torre autoportante 30 m, 84 nos e 244 barras (perfis L e C), apoios articulados. "
"$T" "$G" "$ST/static_nodes.csv" "$O/torre_estatica_structura.png" "Structura — torre de telecomunicacoes: analise estatica" "${GEO}Peso proprio + antenas + vento; deslocamento no topo 49,08 mm (figura ampliada)" "Deslocamento total" "mm" 0 3.0 1000
"$T" "$G" "$MW/static_mw_formatted.csv" "$O/torre_estatica_mecway.png" "Mecway 28 — torre de telecomunicacoes: analise estatica" "${GEO}Mesmo modelo (vigas de 6 GDL); deslocamento no topo 49,08 mm (resultado renderizado com a escala do Structura)" "Deslocamento total" "mm" 0 3.0 1000
for k in 1 3; do
 "$T" "$G" "$ST/modal_mode$k.csv" "$O/torre_modal${k}_structura.png" "Structura — torre: vibracao, modo $k" "${GEO}Frequencia do modo $k: $(grep "^f$k=" $ST/modal.csv | cut -d= -f2 | cut -c1-7) Hz" "Forma normalizada (x1000)" "" 1 4.0 1000
 "$T" "$G" "$MW/modal_mw_formatted$k.csv" "$O/torre_modal${k}_mecway.png" "Mecway 28 — torre: vibracao, modo $k" "${GEO}Frequencia do modo $k: $(grep "^f$k=" $MW/torre_modal_mw_result.txt | cut -d= -f2 | cut -c1-7) Hz (resultado renderizado com a escala do Structura)" "Forma normalizada (x1000)" "" 1 4.0 1000
done
k=1
"$T" "$G" "$ST/buckling_mode$k.csv" "$O/torre_flambagem${k}_structura.png" "Structura — torre: flambagem linear, modo $k" "${GEO}Fator critico do modo $k: $(grep "^fator$k=" $ST/buckling.csv | cut -d= -f2 | cut -c1-7)" "Forma normalizada (x1000)" "" 1 4.0 1000
"$T" "$G" "$MW/flambagem_mw_formatted$k.csv" "$O/torre_flambagem${k}_mecway.png" "Mecway 28 — torre: flambagem linear, modo $k" "${GEO}Fator critico do modo $k: $(grep "^fator$k=" $MW/torre_flambagem_mw_result.txt | cut -d= -f2 | cut -c1-7) (resultado renderizado com a escala do Structura)" "Forma normalizada (x1000)" "" 1 4.0 1000
