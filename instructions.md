# instructions.md

# Sistema de Visualização Estrutural Profissional

## Objetivo

Implementar um sistema moderno de visualização estrutural para um software de análise de pórticos espaciais desenvolvido em C++ utilizando VTK.

O sistema deve possuir qualidade visual comparável aos principais softwares comerciais do mercado:

* SAP2000
* ETABS
* RFEM
* Robot Structural Analysis
* SCIA Engineer
* Midas Gen

O objetivo não é apenas exibir resultados numéricos.

O objetivo é transformar os resultados estruturais em uma representação visual clara, elegante e intuitiva.

---

# Escopo

A implementação deve contemplar:

## Diagramas de esforços internos

* Normal (N)
* Cortante local Y (Vy)
* Cortante local Z (Vz)
* Momento fletor My
* Momento fletor Mz
* Torção Mt

## Glifos estruturais

* Apoios
* Cargas concentradas
* Cargas distribuídas
* Momentos aplicados

## Sistema de anotações

* Valores máximos
* Valores mínimos
* Valores locais selecionados

## Integração completa com VTK

Toda a implementação deverá utilizar o pipeline gráfico já existente.

---

# Filosofia Visual

A visualização deve seguir os princípios dos softwares estruturais modernos.

Os diagramas devem ser visualmente ricos.

Os glifos devem ser visualmente leves.

O usuário deve conseguir compreender rapidamente:

* geometria
* vínculos
* carregamentos
* esforços internos

sem poluição visual.

---

# Princípios de Projeto

Priorizar:

* clareza
* simplicidade
* contraste
* desempenho
* legibilidade

Evitar:

* excesso de sólidos 3D
* excesso de polígonos
* elementos decorativos
* aparência de software CAD genérico

---

# Filosofia dos Diagramas

Os diagramas de esforços são o principal elemento visual do pós-processador.

Eles devem possuir:

* preenchimento translúcido
* contornos destacados
* aparência contínua
* superfícies suaves
* diferenciação clara entre sinais positivos e negativos

Os diagramas serão os únicos elementos predominantemente sólidos da cena.

---

# Filosofia dos Glifos

Os glifos NÃO devem ser implementados como objetos sólidos tridimensionais.

Não utilizar:

* cones
* cilindros
* prismas
* extrusões
* sólidos volumétricos

A aparência desejada é semelhante a desenhos técnicos profissionais.

Todos os glifos devem ser compostos por:

* linhas
* polylines
* polígonos planos
* superfícies muito finas

A representação deve ser vetorial.

A cena deve lembrar um desenho de engenharia e não um modelo CAD.

---

# Sistema de Escala dos Diagramas

Criar uma classe responsável por controlar a escala visual.

A escala dos diagramas deve ser independente da magnitude física dos esforços.

O sistema deve:

* localizar o maior valor absoluto
* normalizar os esforços
* aplicar fator visual configurável

O usuário poderá ajustar a escala visual dos diagramas sem alterar os resultados.

---

# Diagramas de Esforços

## Normal (N)

Representação:

superfície contínua deslocada segundo o eixo local Y.

Positivo:

azul.

Negativo:

vermelho.

---

## Cortante Vy

Representação:

superfície contínua deslocada segundo o eixo local Y.

---

## Cortante Vz

Representação:

superfície contínua deslocada segundo o eixo local Z.

---

## Momento My

Representação:

superfície contínua deslocada segundo o eixo local Z.

---

## Momento Mz

Representação:

superfície contínua deslocada segundo o eixo local Y.

---

## Torção Mt

Representação:

superfície tubular ou faixa especial envolvendo o eixo da barra.

A implementação deve permitir futura evolução para representações mais sofisticadas.

---

# Separação de Regiões Positivas e Negativas

Todo diagrama deverá distinguir visualmente:

* região positiva
* região negativa

Utilizar:

Positivo:
RGB(45,110,255)

Negativo:
RGB(220,50,50)

Transparência:
0.65

---

# Contornos dos Diagramas

Todo diagrama deverá possuir contorno externo.

O contorno deve:

* possuir maior contraste
* permanecer visível em qualquer zoom
* acompanhar exatamente a geometria do diagrama

---

# Suavização

Os diagramas devem apresentar aparência contínua.

Evitar:

* aspecto facetado
* triangulação visível
* bordas serrilhadas

Aplicar suavização geométrica e cálculo adequado de normais.

---

# Apoios

Os apoios devem seguir a simbologia clássica da engenharia estrutural.

## Articulado

Representação:

triângulo vetorial.

Apenas contorno.

Sem preenchimento sólido.

---

## Móvel

Representação:

triângulo vetorial com roletes.

Sem sólidos.

---

## Engaste

Representação:

linha de engaste com hachuras.

Sem prismas.

Sem blocos tridimensionais.

---

## Apoio Fixo

Representação conforme padrão estrutural adotado pelo software.

Sempre utilizando elementos vetoriais.

---

# Cargas Concentradas

As cargas concentradas devem utilizar representação vetorial.

Composição:

* haste
* ponta de seta plana

Não utilizar:

* cilindros
* cones

A ponta deve ser um polígono plano.

O aspecto visual deve lembrar desenhos técnicos profissionais.

---

# Cargas Distribuídas

Representação:

linha de ação acompanhada por múltiplas setas vetoriais.

Características:

* espaçamento automático
* quantidade proporcional ao comprimento
* atualização automática com zoom

---

# Momentos Aplicados

Representação:

arco vetorial.

Acrescido de:

* ponta de seta tangencial

Não utilizar geometria sólida.

Não utilizar toroides.

Não utilizar objetos volumétricos.

---

# Labels

Implementar identificação automática de:

* máximo global
* mínimo global

As labels devem possuir:

* fundo escuro
* texto branco
* aparência semelhante aos softwares comerciais

---

# Performance

O sistema deverá suportar modelos com milhares de barras.

Requisitos:

* cache geométrico
* atualização incremental
* reutilização de mappers
* reutilização de actors

Evitar reconstrução completa da cena.

---

# Arquitetura

Implementar componentes independentes:

* DiagramGenerator
* DiagramMeshBuilder
* DiagramScaleManager
* DiagramExtremaFinder
* DiagramLabelManager
* SupportGlyphFactory
* LoadGlyphFactory
* MomentGlyphFactory
* VisualizationTheme

Cada classe deve possuir responsabilidade única.

---

# Critério de Aceitação

A implementação será considerada concluída quando:

1. Os diagramas apresentarem qualidade visual comparável aos softwares comerciais.

2. Os sinais positivos e negativos forem imediatamente identificáveis.

3. Os apoios apresentarem aparência de desenho técnico profissional.

4. As cargas apresentarem aparência vetorial elegante.

5. Os momentos forem claramente identificáveis.

6. O sistema permanecer responsivo em modelos grandes.

7. Toda a visualização permanecer desacoplada do solver.

8. A arquitetura permitir expansão futura sem refatorações significativas.
