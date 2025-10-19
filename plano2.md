# Plano de Implementação: Visualização dos Eixos Locais (LCS) das Barras

Este documento descreve o plano de implementação para adicionar ao software **Structura** uma funcionalidade que exibe os eixos locais (LCS) de cada barra, conforme as instruções da imagem fornecida.

---

## 1. Objetivo de Produto
- Adicionar um botão na **sidebar** (“Mostrar LCS das barras”) que alterna (on/off) a visualização dos **eixos locais (LCS)** de cada barra.
- Cada barra renderiza um **gizmo** (setas x′, y′, z′) centrado na **metade do comprimento**.
- O **vetor de referência K** de cada barra é salvo como atributo persistente, para uso futuro.
- O cálculo do LCS segue rigorosamente as instruções da imagem: escolha de eixo x′, regra do ponto K/auxiliar, produto vetorial para z′, sistema destrógiro para y′.

---

## 2. Mudanças no Modelo de Dados

### 2.1. Entidades

```cpp
class Bar {
    uuid id;
    Point3 A;    // nó inicial
    Point3 B;    // nó final

    // Novo
    Optional<Point3> kPoint;         // ponto K associado
    Optional<Vec3>  kVectorCached;   // vetor auxiliar (K - A)
    Optional<LCS>   lcsCached;       // cache do LCS
};

struct LCS {
    Vec3 xPrime;
    Vec3 yPrime;
    Vec3 zPrime;
    Point3 origin;
};
```

### 2.2. Persistência
- Serializar `kPoint` (e opcionalmente `kVectorCached`).
- Para barras antigas, se `kPoint` for nulo, o sistema gera automaticamente um vetor auxiliar válido.

---

## 3. Interface e UX

### 3.1. Sidebar
- **ToggleButton**: “Mostrar eixos locais das barras”.
- Estado global: `RenderSettings.showBarLCS : bool`.

### 3.2. Interações
- Ligar → exibir overlay “BarLCS”.
- Desligar → ocultar overlay.

---

## 4. Cálculo Geométrico

### 4.1. Eixo x′
\[
\vec{x'} = \frac{\overrightarrow{AB}}{||\overrightarrow{AB}||}
\]

### 4.2. Vetor Auxiliar
- Ponto **K** não pode ser paralelo a `x′`.
- Se não existir K, escolher entre (1,0,0), (0,1,0), (0,0,1).

### 4.3. Eixo z′
\[
\vec{z'} = \frac{\vec{x'} \times \hat{v}_{aux}}{\| \vec{x'} \times \hat{v}_{aux} \|}
\]

### 4.4. Eixo y′
\[
\vec{y'} = \vec{z'} \times \vec{x'}
\]

### 4.5. Origem
\[
O = \frac{A + B}{2}
\]

---

## 5. API Interna

```cpp
interface ILocalAxisProvider {
    LCS computeLCS(const Bar& bar) const;
}

class DefaultLocalAxisProvider : public ILocalAxisProvider {
    double parallelEps = 1e-5;
    std::array<Vec3,3> fallbackK = {Vec3{1,0,0}, Vec3{0,1,0}, Vec3{0,0,1}};

    LCS computeLCS(const Bar& bar) const override;
};
```

---

## 6. Renderização

### 6.1. Classe
```cpp
class BarLCSOverlay : public IOverlayLayer {
    ILocalAxisProvider& axisProvider;
    GizmoStyle style;
    void draw(const Scene& scene, const RenderSettings& s) override;
};
```

### 6.2. Fluxo
1. Se `!RenderSettings.showBarLCS` → retorna.
2. Itera barras e desenha eixos ortogonais em `LCS.origin`.

---

## 7. Atualização do Ponto K
- `Bar::setKPoint()` atualiza `kPoint`, recalcula vetor auxiliar e marca como “dirty”.
- Migração automática para barras antigas.

---

## 8. Validações
- Barra muito curta → sem renderização.
- Vetor auxiliar paralelo → warning.
- Falha de fallback → erro visual no inspetor.

---

## 9. Testes

- Testes unitários para vetores, ortogonalidade e paralelismo.
- Testes de renderização e desempenho com múltiplas barras.

---

## 10. Critérios de Aceitação

- Botão liga/desliga LCS.
- Eixos x′, y′, z′ centrados e corretamente orientados.
- K persistente e reaproveitado.
- Sem queda perceptível de desempenho.

---

## 11. Pseudocódigo

```cpp
function computeLCS(bar):
    x = normalize(bar.B - bar.A)
    if bar.kPoint exists:
        v = bar.kPoint - bar.A
        if nearlyParallel(v, x): v = pickFallback(x)
    else:
        v = pickFallback(x)

    z = normalize(cross(x, normalize(v)))
    y = cross(z, x)
    O = (bar.A + bar.B) * 0.5
    return LCS{x, y, z, O}
```

---

## 12. Boas Práticas

- SRP e injeção de dependências.
- Logs e telemetria de erros.
- Documentação das fórmulas e referências teóricas.

---

## 13. Tarefas

1. Modelo: adicionar atributos.
2. Math Core: `DefaultLocalAxisProvider`.
3. Render: `BarLCSOverlay`.
4. UI: botão na sidebar.
5. Persistência: leitura e gravação de K.
6. Testes unitários e de integração.
7. Documentação para desenvolvedores: atualize o developer_guide.md com informações pertinentes a essa implementação.
