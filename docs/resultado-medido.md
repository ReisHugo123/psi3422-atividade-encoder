# Resultado medido no laboratório

O que a bancada devolveu, contra o que o projeto previa. É daqui que sai o
relatório. O procedimento está em [calibracao.md](calibracao.md) e o passo a
passo em [roteiro-laboratorio.md](roteiro-laboratorio.md).

## 1. O que foi construído, e onde divergiu do planejado

O kit não trouxe o disco vazado de 20 aberturas, e o sensor que veio foi o
**HW-201**, refletivo, de 3 pinos. O plano original supunha o LM393 de garfo com
disco.

A saída foi usar a **própria roda como alvo**: o cubo de alumínio anodizado
reflete o infravermelho, e fita isolante preta não reflete. Ficaram **3 fitas na
roda esquerda e 4 na direita**, coladas no perímetro, com os sensores presos por
cola quente sobre uma camada de fita isolante que faz o isolamento elétrico
contra a chapa de alumínio.

| Item | Planejado | Construído |
|---|---|---|
| Sensor | LM393 de garfo | **HW-201 refletivo** |
| Alvo | disco de 20 aberturas | **3 e 4 fitas pretas na roda** |
| Pulsos por volta | 40 | **6 na esquerda, 8 na direita** |
| Entre-rodas | 65 e 140 mm estimados | **170 mm** |

### Por que rodas com números diferentes de marcas funcionam

Foi a dúvida mais importante do dia e a resposta veio do código. O critério de
parada em `executa()` é a **média das duas contagens**, e nas duas manobras que a
atividade pede (reta e giro no próprio eixo) as duas rodas percorrem a mesma
distância. Então a escala efetiva é a média das duas escalas, e é exatamente essa
média que o MODO 2 mede.

A condição para isso valer é `ODO_KP = 0`, que é o único ponto do código que
compararia uma roda com a outra. Ele já estava em 0, por causa da falta de folga
de tensão do power bank.

Efeito colateral bom: como o critério soma as duas contagens, cada borda de
**qualquer** uma das rodas conta. São 14 bordas por volta, e não 6 ou 8.

## 2. Calibração medida

Piso do laboratório, alimentação por power bank de 5 V.

| Constante | Valor | De onde veio |
|---|---|---|
| `ODO_PULSOS_POR_M` | **31** | 38 pulsos comandados deram 1212 mm |
| `ODO_PULSOS_90` | **4** | com 3 o giro deu 65°, com 4 ficou perto de 90° |
| `ODO_ESCORREGO_RETO` | 0 | o firmware reportou `escorregou 0 pulsos` |
| `ODO_ESCORREGO_GIRO` | 0 | idem |
| `ODO_KP` | 0 | obrigatório, ver seção 1 |

Isso dá **32,3 mm por pulso**, e passo de decisão de 16 mm, porque a soma das
duas contagens avança a cada borda de qualquer roda.

### As corridas

| Corrida | Comandado | Medido | mm por pulso |
|---|---|---|---|
| MODO 2, primeira | 68 pulsos | 1780 mm | 26,2 |
| MODO 4, com `POR_M` 38 | 1000 mm | 1212 mm | 31,9 |
| MODO 4, com `POR_M` 31 | 1000 mm | repetiu e voltou à marca | 32,3 |

As duas primeiras discordam 22%. A terceira repetiu e o carrinho voltou ao ponto
de partida na volta de ré, que é o critério que fechou a calibração.

## 3. Objetivos do enunciado

| Objetivo | Como foi provado |
|---|---|
| 1. Um encoder em cada motor | HW-201 em cada roda, contando por interrupção de PORTD |
| 2. Estimar a distância percorrida | MODO 4, 1000 mm de ida e 1000 de volta, parando na marca de partida |
| 3. Curva de 90° para cada lado | MODO 5, giro à direita e à esquerda voltando ao rumo original |

## 4. A limitação que este carrinho tem, e ela é do encoder

Com 14 bordas por volta da roda e entre-rodas de 170 mm, **o menor incremento de
ângulo que dá para comandar é de cerca de 22 graus**. Os valores alcançáveis em
volta de 90 são:

| `ODO_PULSOS_90` | Roda percorre | Ângulo |
|---|---|---|
| 3 | 96,8 mm | 65° |
| **4** | **129,0 mm** | **perto de 90°** |
| 5 | 161,3 mm | 109° |

Não existe valor intermediário. Isso não é limitação do controle, que parou com
`erro final 0 pulsos` nas duas medidas, é **resolução do encoder**: o alvo tem de
ser mais fino que o movimento que se quer controlar. Mais marcas na roda
reduziriam esse degrau proporcionalmente, ao custo de refazer a calibração.

### Ruído no sensor esquerdo

O firmware conta bordas rejeitadas por chegarem antes do `ENC_LOCKOUT_US`. Nos
giros medidos:

```
glitches esq 1 dir 0
glitches esq 3 dir 0
```

O sensor esquerdo oscila no limiar do comparador e o direito não. Num giro em que
a esquerda contou 3 pulsos válidos, 3 bordas foram descartadas. Parte do ruído
mais lento que o lockout pode estar passando como pulso bom, e isso faz a soma
chegar ao alvo cedo, girando menos que o pedido.

Foi a maior fonte de incerteza da calibração. O conserto é ajustar o trimpot
daquele módulo procurando o meio da faixa em que ele alterna limpo, e um
capacitor de 100 nF entre VCC e GND no próprio módulo.

## 5. O que invalida esta calibração

- trocar de piso
- trocar power bank por bateria, ou o power bank descarregar
- colar, tirar ou reposicionar fita em qualquer das duas rodas
- mover o sensor, inclusive por a cola quente ceder com o calor do motor
