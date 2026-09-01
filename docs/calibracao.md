# Calibração

Este documento é o **objetivo 2 e o objetivo 3 do enunciado**: "calibre o encoder
para estimar a distância percorrida" e "calibre o encoder para controlar o
movimento". Ele é feito de medidas, não de contas: as contas só dão o ponto de
partida.

O produto final são **quatro números** em `lib/odometria/odometria.h`:

```c
#define ODO_PULSOS_POR_M      196   /* pulsos em 1000 mm de reta */
#define ODO_PULSOS_90          22   /* pulsos por roda para 90 graus no eixo */
#define ODO_ESCORREGO_RETO      0   /* pulsos que o carrinho anda depois do freio */
#define ODO_ESCORREGO_GIRO      0   /* idem, no giro */
```

## Por que medir em vez de calcular

A conta geométrica dá 196 pulsos/m e 22 pulsos por 90°. Ela vai errar, e por
motivos que não são erro de conta:

- **O pneu é de borracha e deforma.** O raio efetivo sob o peso do carrinho é
  menor que o raio livre, então a roda avança menos por volta do que π·D diz.
- **O diâmetro nominal não é o diâmetro.** Roda de "65 mm" sai da injetora com
  alguns décimos de diferença, e o pneu tem espessura.
- **No giro no eixo o pneu arrasta de lado.** Ele não rola, ele escorrega, e o
  quanto escorrega depende do piso. É por isso que `ODO_PULSOS_90` é medido e não
  derivado do entre-rodas.
- **O entre-rodas efetivo não é a distância entre os centros dos pneus**, é a
  distância entre os pontos onde eles agarram, e num giro isso muda.

Nada disso é defeito do modelo: é a razão de existir a etapa de calibração.

## Passo 0: geometria, com régua

Antes de qualquer firmware, três medidas. Elas não entram na malha, mas dizem se
os números medidos depois fazem sentido.

| Medida | Como | Onde anotar |
|---|---|---|
| diâmetro da roda | régua ou paquímetro, na roda **montada**, atravessando o centro | `ODO_DIAM_RODA_MM` |
| entre-rodas | centro a centro dos pneus, na largura | `ODO_ENTRE_RODAS_MM` |
| aberturas do disco | contar. São 20 no disco do kit | `ODO_ABERTURAS_DISCO` |

Preencher esses três em `odometria.h` **não é a calibração**, eles só alimentam
o banner e a estimativa inicial. Mas se o disco tiver 12 aberturas em vez de 20,
é aqui que isso aparece.

## Passo 1: pulsos por volta da roda (MODO 1, fase C)

**Grave o MODO 1.** Ele tem três fases, e a terceira é esta.

Com os motores parados, gire **uma** roda com a mão **exatamente 10 voltas**,
devagar. Marque o ponto de partida com um pedacinho de fita na roda e no chassi,
para contar volta sem erro.

```
pulsos por volta = contagem daquela roda / 10
```

| O que deu | O que significa | O que fazer |
|---|---|---|
| ~400 (40/volta) | certo: 20 aberturas, 2 bordas cada | seguir |
| ~200 (20/volta) | está contando **uma borda só** | conferir `IRQC_DUAS_BORDAS` em `encoder.c` |
| bem mais que 400, `glitch` subindo | chatter no comparador ou ruído | ajustar o trimpot, ver [montagem-encoder.md](montagem-encoder.md), seção 5 |
| 0 | o encoder não chega no pino | voltar à fase A |

Dividir por 10 é de propósito: dilui por 10 o erro de quem conta a volta.

## Passo 2: pulsos por metro (MODO 2)

**Grave o MODO 2.** Ele anda `CAL_PULSOS = 400` pulsos (≈ 2 m com a estimativa
inicial) e freia. **Não precisa do terminal**: o alvo está compilado, e quem mede
é a trena.

1. Escolha 2,5 m de piso livre e liso. **O mesmo piso das corridas seguintes**,
   piso diferente dá calibração diferente.
2. Cole uma fita no chão. Alinhe o **eixo das rodas** com ela (não o para-choque:
   o eixo é o ponto que a odometria mede).
3. Solte. Depois de parar, marque onde o eixo das rodas ficou.
4. Meça a distância entre as duas marcas.
5. Repita **3 vezes** e use a média.

```
ODO_PULSOS_POR_M = CAL_PULSOS × 1000 / distância_média_mm
```

| Corrida | mm medidos | Desvio entre rodas (pulsos) |
|---|---|---|
| 1 | | |
| 2 | | |
| 3 | | |
| **média** | | |

Se não houver 2 m livres, baixe `CAL_PULSOS` para 200. Funciona, mas a precisão
relativa cai pela metade: medir 2 m com erro de 5 mm dá 0,25%, e medir 1 m com o
mesmo erro dá 0,5%.

### O que fazer com o desvio entre rodas

O firmware imprime `desvio entre rodas N pulsos` no fim de cada corrida, é a
diferença entre a contagem da esquerda e a da direita, e ela é a razão de o
carrinho não andar reto. Correção sem custo de folga de tensão, em
`lib/motores/motores.c`:

```
lado mais rápido:  TRIM = 100 − 100 × |N| / CAL_PULSOS
```

Exemplo: 400 pulsos de corrida com desvio de +12 (esquerda adiantada) →
`TRIM_ESQ = 100 − 100×12/400 = 97`. Rode o MODO 2 de novo e confirme que o
desvio caiu.

Este é o caminho certo **com power bank de 5 V**. Com bateria de 7,4 a 9 V há
folga para a correção dinâmica: aí ponha `ODO_KP` em 2 a 4 e deixe a malha
corrigir a cada 2 ms.

## Passo 3: escorregada depois do freio

Sai de graça, do mesmo MODO 2. O firmware imprime:

```
freio  esq 396  dir 392   media 394
fim    esq 401  dir 398   media 400
escorregou 6 pulsos depois do freio
```

Ou seja: o freio entrou com 394 pulsos e o carrinho ainda andou 6 pulsos
(≈ 30 mm) antes de parar de verdade. Anote a média de 3 corridas e ponha em
`ODO_ESCORREGO_RETO`. A partir daí o firmware freia **antes** do alvo, e a
manobra termina no ponto certo.

Faça o mesmo no MODO 3 para `ODO_ESCORREGO_GIRO`.

⚠ Meça **antes** de preencher. Preencher com chute esconde o efeito e depois não
tem como saber se a compensação ajudou.

## Passo 4: pulsos por 90° (MODO 3)

**Grave o MODO 3.** Ele dá `CAL_GIRO_REPETE = 4` giros seguidos de
`CAL_PULSOS_GIRO` pulsos cada. Quatro giros de 90° fecham uma volta inteira e o
carrinho volta ao rumo de partida, então **o que sobrar de desalinhamento é o
erro de um giro multiplicado por 4**. Amplificar para medir é mais barato que
medir com precisão: 2° de erro por giro viram 8°, e 8° dá para ler com régua.

1. Cole uma fita **reta e comprida** no chão e alinhe a lateral do chassi com
   ela. Anote em que ponto o chassi começa.
2. Solte. O carrinho dá 4 giros com pausa de 0,8 s entre eles.
3. No fim, meça o desalinhamento. Duas formas:
   - **Melhor:** encoste uma régua na lateral do chassi e meça a distância da
     régua até a fita em **dois pontos**, um na frente e um atrás, separados por
     um comprimento `C` conhecido (~180 mm no chassi do kit). O ângulo é
     `Δ = atan((d_frente − d_trás) / C)`. Para `C = 180 mm`, cada 3 mm de
     diferença é ≈ 1°.
   - **Rápido:** apoiar uma folha A4 e comparar visualmente com a borda. Serve
     para ver se errou muito, não para calibrar.
4. O total girado foi `360° + Δ` (Δ positivo se passou do rumo, negativo se
   faltou).

```
ODO_PULSOS_90 = CAL_PULSOS_GIRO × 4 × 90 / (360 + Δ)
```

| Rodada | Δ medido (°) | Total girado (°) | `ODO_PULSOS_90` calculado |
|---|---|---|---|
| 1 | | | |
| 2 | | | |
| 3 | | | |

Exemplo: girando 22 pulsos por giro, o carrinho passou 10° do rumo depois de 4
giros → total 370° → `ODO_PULSOS_90 = 22 × 4 × 90 / 370 = 21,4 → 21`.

Depois de mudar `ODO_PULSOS_90`, **rode o MODO 3 de novo**: `CAL_PULSOS_GIRO` é
definido como `ODO_PULSOS_90`, então a segunda rodada já mede a calibração nova.
Δ tem de encolher. Duas iterações costumam bastar.

## Passo 5: validar

| MODO | O que provar | Critério |
|---|---|---|
| 4 | objetivo 2 | ida de 1000 mm com erro < 20 mm, e a volta cai na marca de partida |
| 5 | objetivo 3 | 90° à direita e 90° à esquerda, e depois dos dois o rumo é o original |
| 6 | objetivo 3, em arco | precisa de bateria de 7,4 a 9 V, ver o README |
| 7 | tudo junto | o quadrado de 50 cm fecha: o erro final é o resumo honesto da calibração |

O **quadrado é o melhor teste que existe** para as duas calibrações juntas, e é o
que vale gravar em vídeo: erro de distância deforma os lados, erro de ângulo abre
o quadrado, e as duas coisas aparecem em uma medida só, a distância entre o
ponto final e a marca de partida.

## Orçamento de erro: o que dá para melhorar e o que não dá

| Fonte | Quanto | Dá para reduzir? |
|---|---|---|
| **Quantização** | meio pulso da média = **2,55 mm**, e no giro no eixo isso é **2,1°** | não, sem trocar o disco por um de mais aberturas |
| Escorregada do freio | medida no passo 3, tipicamente 3 a 10 pulsos | sim, compensando e frenando antes |
| Assimetria dos motores | medida no passo 2 | sim, `TRIM_*` ou `ODO_KP` |
| Escorregamento de pneu no giro | absorvido pela calibração | só se o piso não mudar |
| Deriva de calibração com o piso | refazer o passo 4 | é limitação de encoder de canal único |

**De onde vêm os 2,1°:** cada pulso são 5,105 mm de arco. A média das duas rodas
avança meio pulso quando qualquer uma das duas conta, ou seja 2,55 mm, e 2,55 mm
de arco num raio de 70 mm (metade do entre-rodas) são 2,1°. É o limite físico do
disco de 20 aberturas neste chassi, e é honesto declarar isso no relatório em vez
de perseguir décimos de grau.

Na **curva em arco** do MODO 6 o mesmo meio pulso acontece num raio muito maior:
com raio de 250 mm, a roda de fora gira num raio de 320 mm, e 2,55 mm ali são
0,46°. Quatro vezes melhor, pelo mesmo disco, porque a resolução angular é
resolução linear dividida pelo raio.
