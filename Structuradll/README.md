# Structura

Um solver para porticos espaciais desenvolvido para a disciplina de Introdução aos métodos computacionais do PPGEC UFAL 2025.1
Aluno: João Fyllipy de Lima Nunes

## Requerimentos

### Dependencias
- Eigen3 library (linear algebra)
- CMake

## Documentação

- [Formulação matemática](docs/FORMULACAO_MATEMATICA.md): equações implementadas (rigidez, eixos locais, giro e
  excentricidade da seção, liberações, cargas, solução, verificações e limitações).
- [Como consumir a DLL](consumeDll.md).

## Uso
### Compilando
Apenas compile pelo visual studio ou usando o CMake (cmake .)

### Rodando
```bash
# Use um arquivo de entrada
./Structura my_structure.txt
```

### Template do arquivo de input

Comentários começam com um `#`.

```txt
[MATERIALS]
# ID    E (Pa)          G (Pa)
1     2.1e11          8.07e10

[SECTIONS]
# ID    A (m^2)     Iz (m^4)      Iy (m^4)      J (m^4)
1     0.005       4.0e-6        8.0e-6        1.2e-5

[NODES]
# ID    X (m)    Y (m)    Z (m)    UX  UY  UZ  RX  RY  RZ
1       0.0      0.0      0.0      1   1   1   1   1   1
2       5.0      0.0      0.0      0   0   0   0   0   0

[MEMBERS]
# ID    Node_i   Node_j   Material_ID   Section_ID
1       1        2        1             1

[NODAL_LOADS]
# Node_ID   Fx (N)    Fy (N)   Fz (N)   Mx (Nm)   My (Nm)   Mz (Nm)
2           0.0       -10000.0  0.0      0.0       0.0       5000.0

[MEMBER_LOADS]
# Member_ID   Sistema(Local/Global)   qx (N/m)   qy (N/m)   qz (N/m)
1             LOCAL                   0.0        0.0        -5000.0
```

## Arquitetura do programa

O programa segue o paradigma da programação orientada a objetos com as seguintes classes principais

- **Material**: Armazena propiedades dos materiais
- **Section**: Armazena propriedades das seções transversais
- **Node**: Nós da estrutura
- **Member**: Barras da estrutura com o calculo das matrizes de rigidez
- **Load**: Classe base para os carregamentos
  - **NodalLoad**: Cargas concentradas nos nós
  - **DistributedLoad**: Carga distribuida nas barras
- **Structure**: Classe principal que faz a solução.

## Template de saída

```
================================================================================
                    STRUCTURAL ANALYSIS RESULTS
================================================================================

NODAL DISPLACEMENTS:
--------------------------------------------------------------------------------
  Node      UX (m)      UY (m)      UZ (m)    RX (rad)    RY (rad)    RZ (rad)
--------------------------------------------------------------------------------
     1  0.0000e+00  0.0000e+00  0.0000e+00  0.0000e+00  0.0000e+00  0.0000e+00
     2  1.2345e-03 -4.5678e-03  2.3456e-04  1.2345e-04  3.4567e-05  6.7890e-04

REACTION FORCES:
--------------------------------------------------------------------------------
  Node      FX (N)      FY (N)      FZ (N)    MX (Nm)    MY (Nm)    MZ (Nm)
--------------------------------------------------------------------------------
     1  1.2345e+04  2.3456e+04  3.4567e+03  4.5678e+03  5.6789e+03  6.7890e+03

MEMBER INTERNAL FORCES:
------------------------------------------------------------------------------------------------------------------------
Member     End      Fx (N)      Fy (N)      Fz (N)    Mx (Nm)    My (Nm)    Mz (Nm)
------------------------------------------------------------------------------------------------------------------------
     1       I  1.2345e+04  2.3456e+03  4.5678e+02  5.6789e+02  6.7890e+03  7.8901e+02
             J -1.2345e+04 -2.3456e+03 -4.5678e+02 -5.6789e+02 -6.7890e+03 -7.8901e+02

AUXILIARY VECTOR K PER MEMBER:
------------------------------------------------------------
Member            Kx (-)         Ky (-)         Kz (-)
------------------------------------------------------------
     1        0.0000e+00    0.0000e+00    1.0000e+00
```


