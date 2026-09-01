# Montagem do encoder

Guia de bancada: qual sensor você tem, onde vai cada fio, e como prender o
disco. A fundamentação das escolhas está em [decisoes.md](decisoes.md), e a
calibração, em [calibracao.md](calibracao.md).

## 1. Qual dos três sensores veio no kit

O enunciado cita dois e o material de apoio traz um terceiro. Dá para saber qual
é olhando o número de pinos:

| Sensor | Pinos | Como é | Sinal |
|---|---|---|---|
| **LM393**, sensor de velocidade | **4** (VCC, GND, D0, A0) | placa com um **garfo** óptico, o disco passa **por dentro** da fenda | D0 vai a **1** quando o feixe é **bloqueado** |
| **HW-201**, sensor de obstáculo IR | **3** (VCC, GND, OUT) | placa com dois olhos (LED IR + fototransistor) apontando **para a frente**, tem potenciômetro | OUT vai a **0** quando **detecta** reflexo |
| **MOCH22A** | 4 fios soltos, **sem placa** | garfo óptico cru: LED IR de um lado, fototransistor do outro | precisa de resistor e pull-up externos |

Para o firmware os três são a mesma coisa: um pino digital que troca de nível a
cada abertura que passa. **A polaridade não importa**, `encoder.c` conta as
duas bordas, então tanto faz se a abertura leva o pino a 0 ou a 1.

O LM393 é o mais fácil dos três: o garfo obriga a geometria a ficar certa e o
comparador já entrega uma borda limpa. Se o kit tiver os dois, use ele.

## 2. Fiação

### LM393 (4 pinos)

| Pino do módulo | Vai para | Observação |
|---|---|---|
| VCC | **3V3** (J9-04 ou J9-08) | o módulo aceita 3 a 5 V |
| GND | GND (J2-14, J9-12 ou J9-14) | |
| **D0** | PTD6 (J2-17) esquerdo · PTD7 (J2-19) direito | é este que se usa |
| A0 | **nada** | saída analógica, não serve aqui |

### HW-201 (3 pinos)

| Pino do módulo | Vai para |
|---|---|
| VCC | **3V3** |
| GND | GND |
| **OUT** | PTD6 (esquerdo) · PTD7 (direito) |

### MOCH22A (cru, sem placa)

Só se não houver outro. São dois circuitos:

```
LED IR:            3V3 --[ 150 R ]-- anodo | catodo -- GND
fototransistor:    3V3 --[  10 k ]--+-- coletor | emissor -- GND
                                    |
                                    +--> PTD6 (ou PTD7)
```

O resistor do LED sai de `(3,3 − 1,2 V) / 20 mA ≈ 105 Ω`, 150 Ω dá margem. Em
5 V seria 220 Ω. O pull-up de 10 k no coletor é o que define o nível quando o
transistor corta. **O pull-up interno do KL25Z já está ligado** pelo firmware,
mas ele é fraco (~20 a 50 kΩ) e a borda sai lenta, o que gera contagem dupla,
e o de 10 k externo resolve. Se ainda assim o contador de `glitch` subir, aumente
`ENC_LOCKOUT_US` em `lib/encoder/encoder.c`.

### ⚠ Por que 3,3 V e não 5 V

Os pinos do KL25Z **não são tolerantes a 5 V**. A saída digital desses módulos
sai no nível da própria alimentação: alimentado em 5 V, o encoder entrega 5 V no
PTD6 e pode matar o pino. É a mesma decisão que se tomou para o HC-SR04 na
atividade 1, e pelo mesmo motivo.

Os três sensores funcionam em 3,3 V (o LM393 é especificado de 3 a 5 V, o HW-201
de 3,3 a 5 V). O que muda é que o LED infravermelho fica menos brilhante, então
**o potenciômetro pode precisar de reajuste**, ver a seção 5.

Se algum módulo teimar em não responder em 3,3 V: alimente em 5 V e ponha um
divisor na saída (10 k em série com 20 k para GND, ou 1 k / 2 k), **nunca** a
saída em 5 V direto no pino.

## 3. O disco e onde ele vai

O disco de 20 aberturas do kit encaixa no **eixo da roda**, do lado de dentro do
chassi. Como o eixo é o de saída da caixa de redução, **20 aberturas equivalem a
20 aberturas por volta da roda**, não por volta do motor. É isso que faz a conta
de 204,2 mm por volta valer.

Ordem de montagem que evita retrabalho:

1. Encaixe o disco no eixo antes de prender o sensor. Ele tem de girar **sem
   encostar** no chassi nem no motor.
2. Posicione o sensor e só então marque onde ele vai. Com o LM393 o disco tem de
   passar **no meio da fenda**, sem raspar, a fenda tem ~1 cm de profundidade e
   o disco tem de entrar pelo menos metade dela, na região das aberturas, não
   no cubo central, onde não há furo nenhum.
3. Prenda. Na ordem de preferência: parafuso M3 nos furos do próprio módulo →
   abraçadeira de nylon → fita dupla face. **Fita dupla face solta com a
   vibração do motor**, se for o único jeito, reforce com abraçadeira.
4. Confira girando a roda com a mão: o disco não pode encostar em nada em
   nenhuma posição.

Com o **HW-201** a geometria é outra: ele é refletivo, então não há fenda. Aponte
os dois olhos para a **face interna da roda** ou para o disco, a **5 a 15 mm** de
distância, e o alvo tem de ter contraste, trechos claros e escuros alternados.
Se o disco for preto vazado, ponha um pedaço de papel branco atrás dele, do outro
lado, para que "furo" seja claro e "cheio" seja escuro. É mais trabalhoso que o
garfo, e é o motivo de preferir o LM393 quando existem os dois.

## 4. Passagem dos fios

Motor com escova em PWM injeta ruído, e o encoder é uma entrada digital sensível
correndo no mesmo chassi.

- Passe os fios do encoder **longe dos fios do motor**, se tiverem de cruzar, que
  cruzem em ângulo reto, não paralelos.
- Deixe o fio de GND do encoder junto do de sinal ao longo de todo o caminho
  (trançados, se der). O retorno de corrente perto do sinal é o que fecha a área
  do laço, e área de laço é o que capta ruído.
- Se sobrar um capacitor de 100 nF, ponha um entre VCC e GND **no próprio
  módulo** do encoder. É a defesa mais barata que existe contra a queda de tensão
  no arranque do motor.
- Deixe folga de fio: a roda gira, o chassi vibra, e fio esticado arranca.

O firmware conta quantas bordas ele rejeitou por chegarem cedo demais
(`glitch`, impresso no modo 1 e no fim de cada manobra). **Se esse número cresce
junto com a contagem, é ruído ou o comparador oscilando no limiar**, é problema
de bancada, não de código.

## 5. Ajuste do potenciômetro

Só o LM393 e o HW-201 têm. O trimpot mexe no limiar do comparador.

1. Rode a roda **bem devagar** com a mão, com o firmware no **MODO 1, fase A**
   (ele imprime o nível bruto dos dois pinos).
2. O nível tem de **alternar 0 e 1 de forma limpa** a cada abertura. Se ficar
   preso num valor, gire o trimpot devagar até começar a alternar.
3. Depois de alternar, procure o **meio da faixa** em que alterna: gire até parar
   de alternar de um lado, marque, gire para o outro lado até parar, marque, e
   deixe no meio das duas marcas. É esse meio que dá imunidade a ruído.
4. Confirme na fase C, com 10 voltas na mão: a contagem tem de dar 400 (± 1 ou
   2). Se der ~200, está contando só uma borda, se der bem mais que 400, há
   chatter.

O HW-201 tem dois LEDs indicadores na placa: um de alimentação, sempre aceso, e
outro que acende quando detecta. O segundo pisca junto com as aberturas e é uma
conferência visual grátis.

## 6. O que levar para o laboratório

| Item | Para quê |
|---|---|
| trena ou fita métrica (≥ 2 m) | medir a reta da calibração |
| régua de 30 cm ou paquímetro | diâmetro da roda e entre-rodas |
| fita crepe | marcar chão, roda e ponto de partida |
| caneta / marcador | marcar o ponto de referência do disco |
| folha A4 ou esquadro | referência de 90° |
| abraçadeiras de nylon + fita dupla face | prender o sensor |
| parafusos M3 curtos | idem, se o módulo tiver furo |
| chave de fenda pequena | trimpot e bornes do L298N |
| multímetro | conferir 3,3 V no módulo e continuidade do GND |
| jumpers fêmea-fêmea extras | os do encoder |
| power bank + cabo USB | alimentação, como nas atividades anteriores |
| bateria 7,4 a 9 V, se houver | libera o modo 6 e a correção de rumo |
| 2 capacitores de 100 nF | desacoplamento dos módulos, se der |
