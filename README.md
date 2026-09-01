# PSI3422, Atividade 4: encoders nas rodas

Carrinho 2WD com **FRDM-KL25Z**, ponte H **L298N** e **um encoder óptico em cada
roda**. Zephyr RTOS + PlatformIO. Continuação direta da atividade 1
(`psi3422-atividade-carrinho`), que andava por tempo. Aqui ele anda por
**distância medida**.

Roteiro da atividade, como está no enunciado:

1. Acoplar um encoder em cada motor.
2. Calibrar o encoder para estimar a distância percorrida.
3. Calibrar o encoder para controlar o movimento e fazer uma curva de 90° para a
   direita e outra para a esquerda.

O enunciado diz que o kit pode vir com o **sensor de velocidade LM393** (garfo
óptico, 4 pinos) ou com o **sensor de obstáculo IR HW-201** (refletivo, 3 pinos),
e o material de apoio ainda lista o **MOCH22A** (garfo cru, sem placa). O
firmware serve os três sem mudar uma linha: para ele o encoder é um pino digital
que troca de nível quando uma abertura do disco passa. O que muda é a fiação e a
mecânica, e isso está em [docs/montagem-encoder.md](docs/montagem-encoder.md).

## O problema que esta atividade resolve

Na atividade 1 o ângulo era tempo: `T_MEIA_VOLTA_MS = 2500` dava 180°. Isso foi
calibrado no chão do laboratório, com o power bank em cima, e vale **só naquele
piso, com aquele peso e aquela carga de bateria**, porque duty de PWM é tensão
média, não velocidade. Trocar qualquer um dos três muda o ângulo e nada avisa.

Com encoder o comando deixa de ser "gire por 2,5 s" e passa a ser "gire até as
rodas terem andado 22 pulsos". A malha fecha na grandeza que interessa.

## Ligações

| Encoder | Pino | Header | Por quê |
|---|---|---|---|
| ENC_ESQ (OUT / D0) | **PTD6** | J2-17 | PORTD, tem interrupção de pino |
| ENC_DIR (OUT / D0) | **PTD7** | J2-19 | PORTD, vizinho, mesma ISR |
| VCC dos dois | **3V3** | J9-04 ou J9-08 | os pinos do KL25Z **não** toleram 5 V |
| GND dos dois | GND | J2-14, J9-12 ou J9-14 | referência comum é obrigatória |

Os dois pinos foram **reservados na atividade 3**, quando a placa de circuito
impresso foi desenhada, em `psi3422-atividade-pcb/docs/pinos.md`, seção 4. A razão
é de silício: no KL25Z **somente PORTA e PORTD têm interrupção de pino** (macro
`PORT_IRQS` do `MKL25Z4.h`, e no devicetree do Zephyr só `gpioa` e `gpiod`
declaram `interrupts`). Contagem de pulso por polling perde borda quando o motor
acelera, então os encoders tinham de nascer num desses dois portos.

Motores, sem nenhuma mudança em relação à atividade 1:

| L298N | Pino | Header |
|---|---|---|
| ENA | PTD2 | D11 |
| IN1 | PTD0 | D10 |
| IN2 | PTD5 | D9 |
| ENB | PTD3 | D12 |
| IN3 | PTB2 | A2 |
| IN4 | PTB3 | A3 |

Três coisas que travam o projeto se passarem batido, as mesmas da atividade 1:

- **Os jumpers ENA e ENB do módulo L298N têm que sair.** Com eles no lugar essas
  entradas ficam em 5 V fixo e não existe controle de velocidade.
- **GND do L298N ligado ao GND da placa.** Sem referência comum a ponte H ignora
  os comandos.
- **Encoder alimentado com 3,3 V, não 5 V.** A saída sai no nível da
  alimentação, e o pino do KL25Z não tolera 5 V.

### Se a PCB da aula 3 já estiver fabricada

Nela os encoders já vêm roteados para PTD6 e PTD7 (conectores `J6` e `J7`), mas
**IN3 e IN4 mudaram** para PTE0 (J2-20) e PTE1 (J2-18) no reposicionamento que
fechou o roteamento. São quatro edições em `lib/motores/motores.c`:

1. `SIM->SCGC5 |= ... | SIM_SCGC5_PORTE_MASK;`
2. `#define IN3_PIN 0u` e `#define IN4_PIN 1u`
3. `pino_saida(PORTE, GPIOE, IN3_PIN);` e `pino_saida(PORTE, GPIOE, IN4_PIN);`
4. em `motor_pinos`, `GPIOB` → `GPIOE`

O banner que o firmware imprime no boot mostra quais pinos estão compilados.
Confira antes de sair procurando defeito na fiação.

## Compilar e gravar

```
pio run
pio run --target upload
pio device monitor
```

Upload pelo protocolo padrão `mbed`, que copia o `.bin` para o drive USB da
placa. Não usar `upload_protocol = cmsis-dap`: exige o pyOCD, que não instala
aqui. Fallback manual: arrastar `.pio/build/frdm_kl25z/firmware.bin` para o
drive da placa.

**O primeiro build depois de clonar leva ~15 minutos** e é normal (baixa e
compila o Zephyr inteiro). Deixe rodando **na véspera**, não no laboratório.

## Organização

```
lib/motores     sentido em GPIO + velocidade em PWM (TPM0), igual a atividade 1
lib/encoder     contagem de bordas de PTD6/PTD7 pela ISR do PORTD
lib/odometria   manobras medidas: anda_mm, gira_graus, curva_graus
src/main.c      a escada de modos
docs/           montagem, calibração, roteiro de bancada e as decisões
```

## A escada de modos

O `#define MODO` no topo de `src/main.c` escolhe o degrau. Os três primeiros
**medem**, os três seguintes **entregam** os objetivos, e o sétimo é a
demonstração que fecha tudo.

| MODO | O que faz | Serve para |
|---|---|---|
| 1 | rodas no ar: nível dos pinos, um motor por vez, 10 voltas na mão | provar a fiação e medir pulsos por volta |
| 2 | anda `CAL_PULSOS` pulsos e freia | medir `ODO_PULSOS_POR_M` |
| 3 | gira `CAL_PULSOS_GIRO` pulsos e freia | medir `ODO_PULSOS_90` |
| 4 | anda 1000 mm, ida e volta | **objetivo 2** |
| 5 | gira 90° no eixo, direita e esquerda | **objetivo 3** |
| 6 | curva de 90° em arco de raio definido | objetivo 3, a outra leitura de "curva" |
| 7 | quadrado de 50 cm | demonstração / vídeo |

Os modos 2 e 3 **não precisam do terminal**: o alvo em pulsos está compilado, e
quem mede é a trena e o esquadro. É de propósito: com o carrinho no chão,
alimentado pelo power bank, não há console.

⚠ **O modo 6 quer bateria de 7,4 a 9 V nos motores.** A curva em arco põe a roda
de dentro em ~56% de velocidade, e com power bank de 5 V isso não move o
carrinho: a manobra aborta com `ODO_TRAVOU`, que é o comportamento correto.
Com power bank, o objetivo 3 se entrega pelo modo 5 (giro no eixo, as duas rodas
a 100%).

## A matemática, e por que ela é medida e não calculada

Duas grandezas transformam pulso em mundo real:

```
ODO_PULSOS_POR_M   pulsos que cabem em 1000 mm de reta
ODO_PULSOS_90      pulsos por roda para girar 90° no próprio eixo
```

A estimativa geométrica de partida, com roda de 65 mm, disco de 20 aberturas
contadas nas duas bordas (40 pulsos/volta) e entre-rodas de 140 mm:

```
circunferência       π × 65      = 204,2 mm por volta
resolução            204,2 / 40  = 5,105 mm por pulso
pulsos por metro     1000 / 5,105 = 196
arco de 90° no eixo  (π/2) × 70  = 110,0 mm por roda
pulsos por 90°       110,0 / 5,105 = 21,5 → 22
```

Esses números fazem o carrinho andar na primeira tentativa. Eles **não** são a
calibração, porque o pneu é de borracha e deforma, a roda escorrega no giro, e
o entre-rodas efetivo não é a distância entre os centros dos pneus mas entre os
pontos onde eles agarram. O procedimento de medida está em
[docs/calibracao.md](docs/calibracao.md).

## Limitações assumidas

- **Encoder de canal único não sabe o sentido.** Quem sabe é a camada que
  comanda os motores, e é ela que aplica o sinal, e por isso a contagem é zerada
  antes de cada manobra. Um encoder em quadratura (dois canais defasados)
  resolveria, e daria contagem com sinal de graça.
- **A resolução angular do giro no eixo é grossa.** Meio pulso da média são
  2,55 mm de arco, ou seja **2,1° no giro no eixo**, e é o limite do disco de
  20 aberturas, não do código. A curva em arco do modo 6 tem resolução ~4×
  melhor porque o raio é maior. Aumentar de verdade exigiria disco com mais
  aberturas.
- **Escorregada depois do freio.** O firmware mede a própria escorregada e
  imprime, e compensá-la é preencher `ODO_ESCORREGO_*`. Ela muda com o piso.
- **Só posição, sem controle de velocidade.** A malha fecha em pulsos
  acumulados, e a velocidade continua sendo duty em malha aberta.
- **Sem ultrassom.** O HC-SR04 da atividade 1 ficou fora de propósito: as
  manobras aqui são curtas e comandadas, e menos hardware no ar é menos coisa
  para depurar no laboratório.
