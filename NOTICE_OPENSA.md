# Origem do comportamento portado e licenca

Parte do comportamento de modelagem e visualizacao deste software foi reimplementada em C++/Qt/VTK
a partir do projeto **Open Structural Analysis** (pasta `opensa/`, licenca **GNU GPL v3**,
ver `opensa/LICENSE`). Foram portados, de forma funcional:

- lancamento grafico de nos e barras sobre o plano de construcao, com snaps (malha, extremidade,
  centro, perpendicular e ortogonal), guias e caixa de coordenadas editavel;
- plano de construcao (grid com fade) e eixos de referencia nomeados;
- exibicao extrudada das secoes (familias parametricas de perfis);
- representacao de apoios (cubo modular) e de cargas (faixas, setas e arcos de momento);
- opcoes de exibicao e os conceitos de casos de carga e combinacoes.

O codigo foi reescrito (nao copiado) e a interface (icones, janelas, balao de coordenadas, painel de
eixos) segue o estilo proprio do Structura. Mesmo assim, por ser uma obra derivada do comportamento
e da geometria de um projeto GPL v3, a distribuicao deste software em binario ou fonte deve
observar os termos da GPL v3. Confirme a situacao de licenca com o autor do Open Structural Analysis
antes de distribuir.

Os arquivos `icons/svg/*.svg` sao desenhos originais deste projeto.
