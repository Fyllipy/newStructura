# skill_visualization_postprocessor.md

# Skill: Professional Structural Post-Processor

## Objetivo

Implementar um sistema profissional de pós-processamento estrutural para um software de análise de pórticos espaciais desenvolvido em C++ utilizando VTK.

Esta skill é responsável por:

* reconstrução dos diagramas internos
* geração das malhas dos diagramas
* renderização dos diagramas
* glifos estruturais
* labels
* cache visual
* otimização gráfica

O objetivo é atingir qualidade visual comparável aos softwares:

* SAP2000
* ETABS
* RFEM
* SCIA Engineer
* Robot Structural Analysis

---

# Filosofia Fundamental

O pós-processador NÃO deve construir diagramas utilizando apenas interpolação entre esforços nodais.

Essa abordagem produz resultados visualmente incorretos quando existem carregamentos distribuídos.

Em vez disso, os diagramas devem ser reconstruídos utilizando:

* esforços nodais
* carregamentos do elemento
* equações de equilíbrio

A reconstrução deve reproduzir exatamente a distribuição interna de esforços.

---

# Dados Necessários do Solver

Cada elemento deverá disponibilizar:

```cpp
struct BeamElementResults
{
    Vector3d node1;
    Vector3d node2;

    Matrix3d localAxes;

    EndForces endForces;

    std::vector<LoadCase> loads;
};
```

---

# Esforços Nodais

```cpp
struct EndForces
{
    double N1;
    double Vy1;
    double Vz1;
    double Mt1;
    double My1;
    double Mz1;

    double N2;
    double Vy2;
    double Vz2;
    double Mt2;
    double My2;
    double Mz2;
};
```

---

# Filosofia de Reconstrução

O diagrama é uma função contínua:

```text
N(x)
Vy(x)
Vz(x)
Mt(x)
My(x)
Mz(x)
```

onde:

```text
0 ≤ x ≤ L
```

---

# Regra Geral

Os esforços devem ser obtidos por:

```text
Equilíbrio interno
+
Carregamentos existentes
```

e não por simples interpolação gráfica.

---

# Caso 1 — Elemento Sem Carregamentos

Quando:

```cpp
loads.empty()
```

utilizar interpolação linear.

Exemplo:

```text
Mz(x)=Mz1+(Mz2-Mz1)x/L
```

---

# Caso 2 — Carga Distribuída Uniforme

Considere:

```text
q = constante
```

Aplicada em Y local.

---

# Cortante

O cortante é linear:

```text
V(x)=V1-qx
```

---

# Momento

O momento é quadrático:

```text
M(x)=M1+V1x-qx²/2
```

O diagrama deve ser reconstruído utilizando essa equação.

NÃO utilizar interpolação linear entre M1 e M2.

---

# Caso 3 — Carga Distribuída Linear

Considere:

```text
q(x)=q1+(q2-q1)x/L
```

---

# Cortante

```text
V(x)=
V1
-
q1x
-
(q2-q1)x²/(2L)
```

---

# Momento

```text
M(x)=
M1
+
V1x
-
q1x²/2
-
(q2-q1)x³/(6L)
```

Resultado:

momento cúbico.

---

# Caso 4 — Força Concentrada Interna

Para:

```text
P
```

na posição:

```text
a
```

Utilizar Heaviside.

---

# Cortante

Salto instantâneo.

---

# Momento

Mudança de inclinação.

---

# Caso 5 — Momento Concentrado

Para:

```text
M0
```

na posição:

```text
a
```

Aplicar salto instantâneo no diagrama de momento.

---

# Arquitetura de Reconstrução

Implementar:

```cpp
class InternalForceField
```

Responsável por calcular:

```cpp
double N(double x);
double Vy(double x);
double Vz(double x);
double Mt(double x);
double My(double x);
double Mz(double x);
```

---

# Amostragem

Nunca desenhar diretamente a função.

Primeiro amostrar.

```cpp
constexpr int MinSamples = 80;
```

---

# Regra Adaptativa

```cpp
samples =
max(
80,
int(length/0.10)
);
```

---

# Posição da Amostra

```cpp
x = i * L/(samples-1)
```

---

# Avaliação

Para cada amostra:

```cpp
value = Mz(x);
```

---

# Sistema Local

Para cada barra:

```cpp
Xlocal
Ylocal
Zlocal
```

normalizados.

---

# Construção do Diagrama

Cada amostra gera:

```cpp
centerPoint
```

e

```cpp
diagramPoint
```

---

# Momento Mz

Deslocamento:

```cpp
offset =
Ylocal *
scaledValue
```

---

# Momento My

Deslocamento:

```cpp
offset =
Zlocal *
scaledValue
```

---

# Cortante Vy

Deslocamento:

```cpp
offset =
Ylocal *
scaledValue
```

---

# Cortante Vz

Deslocamento:

```cpp
offset =
Zlocal *
scaledValue
```

---

# Normal

Deslocamento:

```cpp
offset =
Ylocal *
scaledValue
```

---

# Escala Global

Implementar:

```cpp
class DiagramScaleManager
```

---

# Processo

Percorrer todos os elementos.

Calcular:

```cpp
maxAbs =
max(
abs(all samples)
)
```

---

# Escala Visual

```cpp
scaledValue =
value/maxAbs
*
userScale
```

---

# Cruzamento de Zero

Detectar:

```cpp
v1*v2 < 0
```

---

# Localização Exata

```cpp
t =
abs(v1)/
(abs(v1)+abs(v2))
```

---

# Inserir Vértice

Criar vértice exatamente no ponto:

```cpp
value = 0
```

---

# Separação de Malhas

Gerar:

* malha positiva
* malha negativa

independentes

---

# Construção da Superfície

Gerar duas curvas:

Base:

```cpp
P0,P1,P2...
```

Diagrama:

```cpp
D0,D1,D2...
```

---

# Triangulação

Para cada segmento:

```text
P(i) ----- P(i+1)

 |           |

D(i) ----- D(i+1)
```

Criar:

```cpp
Triangle A
```

e

```cpp
Triangle B
```

---

# Renderização dos Diagramas

Os diagramas devem ser sólidos.

Translúcidos.

Suaves.

Com aparência semelhante aos softwares comerciais.

---

# Cores

Positivo

```cpp
RGB(45,110,255)
```

Negativo

```cpp
RGB(220,50,50)
```

---

# Transparência

```cpp
0.65
```

---

# Contorno

Gerar ator independente.

Espessura:

```cpp
2 px
```

---

# Normais

Aplicar:

```cpp
vtkPolyDataNormals
```

---

# Suavização

Aplicar:

```cpp
vtkSmoothPolyDataFilter
```

somente nos diagramas.

---

# Shader Recomendado

Utilizar iluminação suave.

Evitar brilho especular excessivo.

Objetivo:

visual técnico.

Não visual CAD.

---

# Labels

Implementar:

```cpp
DiagramExtremaFinder
```

---

# Resultado

Identificar:

```text
#1 MAX
#2 MAX
#3 MAX

#1 MIN
#2 MIN
#3 MIN
```

---

# Glifos

Todos os glifos devem ser vetoriais.

Não utilizar:

* cones
* cilindros
* prismas
* extrusões

---

# Cargas Concentradas

Construir:

* polyline principal
* ponta de seta plana

---

# Cargas Distribuídas

Construir:

* linha de referência
* múltiplas setas vetoriais

---

# Momentos

Construir:

* arco poligonal
* ponta tangencial

---

# Apoios

Utilizar simbologia clássica da engenharia estrutural.

Representação vetorial.

Sem sólidos.

---

# Cache

Implementar:

```cpp
DiagramGeometryCache
```

---

# Chave

```cpp
elementId
diagramType
loadCase
scaleFactor
```

---

# Atualização

Modificar apenas geometrias alteradas.

Nunca reconstruir toda a cena.

---

# Meta Final

O resultado deve parecer um pós-processador estrutural profissional moderno e não uma visualização CAD baseada em sólidos tridimensionais.
