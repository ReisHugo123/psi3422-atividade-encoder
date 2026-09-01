# Roteiro de laboratório

Passo a passo de bancada da atividade 4. A ordem importa: cada etapa só começa
quando a anterior está provada, porque erro que passa de etapa vira duas horas
procurando defeito no lugar errado.

Tempo total estimado: **2 h**. Se apertar, corte na ordem da seção "prioridades".

## 0. Antes de sair de casa

| Item | Estado |
|---|---|
| Projeto clonado em `C:\dev\PlatformIO\Projects\psi3422-atividade-encoder` | ✔ |
| **Build já feito e cache quente** (o primeiro build leva ~15 min, trocar de MODO agora leva **8 a 18 s**) | ✔ |
| Os 7 modos compilam sem aviso | ✔ |
| `firmware.bin` do MODO 1 pronto em `.pio\build\frdm_kl25z\` | ✔ |
| Levar o material da lista de [montagem-encoder.md](montagem-encoder.md), seção 6 | ☐ |
| Conferir se o kit veio com LM393 ou HW-201 (muda a mecânica, não o código) | ☐ |

⚠ **Não deixe o primeiro build para o laboratório.** O professor avisou na aula 1
que em anos anteriores alunos queimaram a aula inteira instalando ambiente. Aqui
o build já está feito, só não rode `pio run --target clean`, que joga o cache
fora e obriga a recompilar o Zephyr inteiro.

Como trocar de modo, sempre:

1. abrir `src/main.c` e mudar a linha 33, `#define MODO   N`
2. `pio run --target upload`
3. `pio device monitor` (115200)

## Regra de ouro

**Uma variável por vez.** Não mexer na mecânica e no código na mesma tentativa,
não trocar de piso no meio de uma calibração, não ajustar trimpot e `TRIM_*`
juntos. Quando duas coisas mudam, o resultado não diz qual delas resolveu.

## Etapa 1: mecânica (≈ 20 min)

Objetivo do enunciado nº 1: **um encoder em cada motor**.

1. Encaixe o disco de 20 aberturas no eixo de cada roda, do lado de dentro do
   chassi. Gire com a mão: não pode encostar em nada.
2. Posicione o sensor. Com **LM393**, o disco tem de passar no **meio da fenda**,
   na região das aberturas, não no cubo central. Com **HW-201**, aponte para a
   face interna da roda ou para o disco, a 5 a 15 mm, com alvo de contraste.
3. Prenda: parafuso M3 → abraçadeira → fita dupla face (a fita solta com a
   vibração, e se for o único jeito reforce com abraçadeira).
4. Gire a roda com a mão uma volta inteira e confirme que nada raspa em nenhuma
   posição.

Detalhes e desenhos: [montagem-encoder.md](montagem-encoder.md).

## Etapa 2: fiação (≈ 10 min)

| Fio | Vai em | Header |
|---|---|---|
| OUT / D0 do encoder **esquerdo** | PTD6 | **J2-17** |
| OUT / D0 do encoder **direito** | PTD7 | **J2-19** |
| VCC dos dois | **3V3** | J9-04 ou J9-08 |
| GND dos dois | GND | J2-14, J9-12 ou J9-14 |

- **PTD6 e PTD7 são os dois últimos pinos da fileira ÍMPAR do J2** (a fileira
  interna, a que não tem nome de Arduino). J2-18 e J2-20, os vizinhos da fileira
  par, são D15 e D14, não são esses.
- **3,3 V, nunca 5 V.** Os pinos do KL25Z não toleram 5 V e a saída do módulo sai
  no nível da alimentação.
- Confirme com o multímetro que há 3,3 V no VCC de cada módulo e continuidade do
  GND até o GND da placa.
- Confira que os **jumpers ENA e ENB do L298N estão fora** (herança da atividade
  1: com eles no lugar não há controle de velocidade).
- Passe o fio do encoder longe do fio do motor.

## Etapa 3: provar que o fio está no pino certo (MODO 1, fase A · ≈ 5 min)

**Grave o MODO 1** e abra o monitor. A fase A imprime, por 12 s, o nível bruto
dos dois pinos.

Tape e destape cada sensor com o dedo ou um papel. **O número da roda
correspondente tem de mudar de 0 para 1.**

Se não muda: fio na casa errada do header, módulo sem 3,3 V, ou GND solto. Isso
elimina, em 30 segundos, o erro mais comum e mais caro da montagem, e é por isso
que essa fase existe antes de qualquer motor girar.

## Etapa 4: amarrar cada encoder ao seu motor (MODO 1, fase B · ≈ 5 min)

**Rodas no ar** (apoie o chassi em dois livros ou segure). A fase B gira um motor
de cada vez por 2 s e imprime as duas contagens.

Critério: **girando a roda esquerda, a contagem da direita tem de ficar em zero**,
e vice-versa. Se as duas sobem juntas, os dois fios estão no mesmo pino ou houve
troca. Se subir a contagem do lado errado, troque os dois fios de lugar (ou
troque `ENC_ESQ`/`ENC_DIR` em `encoder.c`, mas trocar o fio é mais honesto).

Olhe também o número de **pulsos/s**: com o motor a 100% e a roda no ar ele fica
tipicamente entre 100 e 200. Zero de um lado com a roda visivelmente girando é
sensor desalinhado, volte à etapa 1.

## Etapa 5: pulsos por volta (MODO 1, fase C · ≈ 5 min)

Motores parados. Gire **uma** roda com a mão **exatamente 10 voltas**, devagar,
com fita marcando o ponto de partida.

Esperado: **400** (= 20 aberturas × 2 bordas × 10 voltas).

| Deu | Significa | Ação |
|---|---|---|
| ~400 | certo | seguir |
| ~200 | conta uma borda só | conferir `IRQC_DUAS_BORDAS` em `encoder.c` |
| ≫ 400 e `glitch` subindo | chatter | ajustar o trimpot ([montagem, §5](montagem-encoder.md)) |

**Ajuste o trimpot agora**, antes de calibrar. Procedimento na seção 5 de
[montagem-encoder.md](montagem-encoder.md): achar a faixa em que alterna limpo e
parar no meio dela.

Daqui em diante o objetivo 1 está cumprido e provado.

## Etapa 6: calibrar a distância (MODO 2 · ≈ 20 min)

Objetivo do enunciado nº 2.

1. Escolha **2,5 m de piso livre**. Anote qual piso é, trocar de piso invalida a
   calibração.
2. Grave o MODO 2. Ele anda 400 pulsos e freia, o alvo está compilado, então
   **não precisa do terminal** para essa parte.
3. Ponha o carrinho no chão com o **eixo das rodas** alinhado com uma fita.
   ⚠ Posicione **antes** de ligar o power bank: ligar reinicia o firmware, e
   depois da contagem regressiva ele larga. Para repetir sem desligar, aperte o
   **reset** da placa.
4. Marque onde o eixo parou. Meça. Repita 3 vezes.
5. `ODO_PULSOS_POR_M = 400 × 1000 / mm_médios`, em `lib/odometria/odometria.h`.
6. Anote também o `desvio entre rodas` e a `escorregada` que o firmware imprime
   (com o cabo USB ligado, ou repetindo uma corrida curta na mesa) e preencha
   `TRIM_*` em `motores.c` e `ODO_ESCORREGO_RETO`.

Tabelas para preencher e as contas: [calibracao.md](calibracao.md), passos 2 e 3.

## Etapa 7: calibrar o ângulo (MODO 3 · ≈ 20 min)

Objetivo do enunciado nº 3, primeira metade.

O MODO 3 dá **4 giros seguidos** do mesmo tamanho. Quatro giros de 90° fecham
uma volta, então o desalinhamento final é o erro de um giro **multiplicado por 4**.
É o que torna 2° mensuráveis com régua.

1. Fita reta e comprida no chão, lateral do chassi alinhada com ela.
2. Solte. 4 giros com pausa de 0,8 s.
3. Meça o desalinhamento final `Δ` com régua em dois pontos do chassi
   (`Δ = atan((d_frente − d_trás)/C)`, com `C ≈ 180 mm`: 3 mm ≈ 1°).
4. `ODO_PULSOS_90 = pulsos_usados × 4 × 90 / (360 + Δ)`.
5. **Regrave o MODO 3 e repita.** `CAL_PULSOS_GIRO` é o próprio `ODO_PULSOS_90`,
   então a segunda rodada mede a calibração nova. Δ tem de encolher. Duas
   iterações costumam bastar.
6. Anote `ODO_ESCORREGO_GIRO` da mesma forma que na etapa 6.

## Etapa 8: entregar os objetivos (MODO 4 e MODO 5 · ≈ 15 min)

- **MODO 4**, objetivo 2: anda 1000 mm e volta. Meça o erro da ida e confirme
  que a volta cai na marca de partida. Critério razoável: erro < 20 mm em 1000 mm
  (2%).
- **MODO 5**, objetivo 3: 90° à direita, pausa, 90° à esquerda. Depois dos dois
  o rumo tem de ser o original. **É este o entregável do objetivo 3.**

Grave vídeo dos dois. Anote os números que o monitor imprime.

## Etapa 9: a demonstração que fecha (MODO 7 · ≈ 10 min)

Quadrado de 50 cm: 4 retas e 4 giros de 90°. O carrinho deveria terminar na marca
de partida, com o rumo de partida.

É o melhor teste que existe para as duas calibrações juntas: erro de distância
deforma os lados, erro de ângulo abre o quadrado, e as duas coisas aparecem numa
medida só, a distância entre o ponto final e a marca. **É este o vídeo do
relatório.**

## Etapa 10: opcional, só com bateria (MODO 6)

Curva de 90° **em arco**, a outra leitura possível de "curva de 90°" no enunciado:
as duas rodas para frente com velocidades diferentes, raio definido.

⚠ **Precisa de bateria de 7,4 a 9 V nos motores.** A roda de dentro vai a ~56% de
velocidade, e com power bank de 5 V (motor em ~3,2 V no duty máximo) isso não sai
do lugar: a manobra aborta com `ODO_TRAVOU`, que é o comportamento correto e não
um defeito. Com bateria, aproveite para pôr `ODO_KP` em 2 a 4 e refazer o MODO 7.
O quadrado fecha bem melhor com correção de rumo ligada.

## Prioridades, se o tempo apertar

1. Etapas 1 a 5 (objetivo 1, e sem isso nada mais existe)
2. Etapa 6 (objetivo 2)
3. Etapa 7 + MODO 5 (objetivo 3)
4. MODO 4 completo, MODO 7
5. MODO 6

Os três objetivos do enunciado estão cobertos ao fim do item 3. O resto é
qualidade de entrega.

## Diagnóstico

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| Nível do pino não muda na fase A | fio na casa errada, sem 3,3 V, GND solto | multímetro no VCC do módulo, recontar os pinos do J2 |
| Nível preso em 0 ou 1 mesmo com o disco girando | trimpot fora da faixa, sensor longe/desalinhado | ajustar trimpot, reposicionar |
| Contagem sobe nas duas rodas ao girar uma | dois fios no mesmo pino | conferir J2-17 e J2-19 |
| 10 voltas dão ~200 | contando uma borda só | `IRQC_DUAS_BORDAS` em `encoder.c` |
| 10 voltas dão ≫ 400, `glitch` alto | chatter / ruído do motor | trimpot, afastar fios, 100 nF no módulo, subir `ENC_LOCKOUT_US` |
| Contagem some quando o motor liga (só com motor) | ruído de PWM na alimentação | 100 nF no módulo, separar fios, GND curto |
| `ODO_TRAVOU` logo na largada | encoder não conta, roda travada | voltar à fase A |
| `ODO_TRAVOU` no meio da manobra | roda parou por falta de tensão | `ODO_KP = 0`, conferir bateria, power bank não fornece folga |
| `ODO_TIMEOUT` | alvo grande demais para o tempo | subir `ODO_TIMEOUT_MS` |
| Motor não gira, mas o encoder conta ao girar na mão | jumper ENA/ENB no lugar, 5VEN, GND do L298N | ver README, "Ligações" |
| Carrinho puxa para um lado na reta | assimetria dos motores | `TRIM_*` em `motores.c` (calibracao.md, passo 2) |
| Giro passa ou falta sempre o mesmo tanto | `ODO_PULSOS_90` fora | iterar a etapa 7 |
| Giro erra de forma aleatória | escorregamento de pneu, freio inconsistente | preencher `ODO_ESCORREGO_GIRO`, piso mais aderente |
| Não aparece nada no `pio device monitor` | porta errada, 115200 | conferir a COM, a serial é a do OpenSDA |
| Upload falha | `upload_protocol` | não usar `cmsis-dap`, arrastar o `firmware.bin` para o drive da placa |

## Plano B: se a interrupção do PORTD não disparar

Sintoma: a fase A mostra o nível mudando (o pino está certo), mas a contagem
**não sobe**. Isso significa que a ISR não está sendo chamada.

A causa possível é o `irq_connect_dynamic` não ter substituído o tratador que o
driver de GPIO do Zephyr já registrou no vetor 31. Foi verificado que a tabela
`_sw_isr_table` está em RAM (endereço `0x1ffff028`, seção `D`), portanto
gravável, e que o `CONFIG_DYNAMIC_INTERRUPTS=y` está no `prj.conf`, então isso
não deveria acontecer. Mas se acontecer, o caminho alternativo é usar a API de
GPIO do Zephyr, que passa a contar pelo próprio ISR do driver.

Em `lib/encoder/encoder.c`, trocar `encoder_init()` e a ISR por:

```c
#include <zephyr/drivers/gpio.h>

static const struct device *gpiod;
static struct gpio_callback cb;

static void cb_borda(const struct device *d, struct gpio_callback *c,
		     gpio_port_pins_t pins)
{
	uint32_t agora = k_cycle_get_32();

	if (pins & BIT(ENC_ESQ_PIN)) { conta_borda(ENC_ESQ, agora); }
	if (pins & BIT(ENC_DIR_PIN)) { conta_borda(ENC_DIR, agora); }
}

void encoder_init(void)
{
	gpiod = DEVICE_DT_GET(DT_NODELABEL(gpiod));
	encoder_zera();

	for (int i = 0; i < 2; i++) {
		gpio_pin_configure(gpiod, pino[i], GPIO_INPUT | GPIO_PULL_UP);
		gpio_pin_interrupt_configure(gpiod, pino[i], GPIO_INT_EDGE_BOTH);
	}
	gpio_init_callback(&cb, cb_borda,
			   BIT(ENC_ESQ_PIN) | BIT(ENC_DIR_PIN));
	gpio_add_callback(gpiod, &cb);
}
```

E `encoder_nivel()` passa a ser `gpio_pin_get(gpiod, pino[e])`. Nada mais muda:
`conta_borda`, as contagens e toda a `lib/odometria` continuam iguais.

Por que essa não é a versão principal: o resto do projeto (`motores.c`,
`hcsr04.c` da atividade 1) é escrito em registrador, e a decisão de onde os
encoders podiam nascer veio de ler `PORT_IRQS` e o devicetree. A versão em
registrador é a que se explica no relatório. O detalhe está em
[decisoes.md](decisoes.md).

## O que registrar para o relatório

Anote no laboratório, porque depois não se recupera:

- **qual sensor** veio no kit (LM393 / HW-201 / MOCH22A) e como foi preso
- **as três medidas de geometria**: diâmetro da roda, entre-rodas, aberturas
- **pulsos por volta medidos** (fase C), e se bateu com 40
- **as 3 corridas do MODO 2**: mm medidos e desvio entre rodas
- **a escorregada** depois do freio, em reta e em giro
- **as iterações do MODO 3**: Δ medido e `ODO_PULSOS_90` resultante em cada
- **erro do MODO 4** (mm em 1000 mm) e do MODO 5 (rumo depois de dir + esq)
- **erro de fechamento do quadrado** (MODO 7), em mm
- **qual piso** e **qual alimentação** (power bank ou bateria) em cada medida
- **vídeo** do MODO 5 e do MODO 7
- **os quatro números finais** de `odometria.h`

E a declaração de uso de IA, que a disciplina exige.
