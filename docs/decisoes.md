# Decisões de projeto, e a evidência de cada uma

Este arquivo existe para duas coisas: alimentar o relatório e evitar que a
próxima sessão (humana ou de IA) refaça análise já feita. Cada decisão vem com a
fonte que a sustenta, e onde há armadilha ela está descrita, não escondida.

## 1. Os encoders em PTD6 e PTD7, e não em qualquer pino livre

**Restrição de silício, não preferência:** no KL25Z **somente PORTA e PORTD têm
interrupção de pino**. Três fontes independentes dizem o mesmo:

- `MKL25Z4.h` do SDK da NXP, linha 3220:
  `#define PORT_IRQS { PORTA_IRQn, NotAvail_IRQn, NotAvail_IRQn, PORTD_IRQn, NotAvail_IRQn }`
 A posição de PORTB, PORTC e PORTE é `NotAvail_IRQn`.
- Devicetree do Zephyr (`nxp_kl25z.dtsi`): só `gpioa` declara `interrupts = <30 2>`
  e só `gpiod` declara `interrupts = <31 2>`. `gpiob`, `gpioc` e `gpioe` não
  declaram nada.
- O próprio `FRDM-KL25Z Arduino R3 pin layout compatibility comparison chart`
  oficial marca **X vermelho na coluna "Interrupt"** para A0 a A5, D6, D7, D14 e
  D15, que são exatamente os pinos de PORTB, PORTC e PORTE.

Contagem de pulso quer interrupção: por polling, a borda se perde quando o motor
acelera, e pulso perdido não se recupera, o erro de odometria é cumulativo.

Por que **PTD6 e PTD7** especificamente, e não dois pinos de PORTA:

- **Mesmo porto = uma ISR só.** O PORTD tem um vetor único (`PORTD_IRQn = 31`) e
  o registrador `ISFR` diz qual pino interrompeu, com um bit por pino. As duas
  rodas são atendidas na mesma rotina, o que garante que as duas contagens usam a
  mesma base de tempo.
- **Vizinhos no header** (J2-17 e J2-19, fileira ímpar), o que na placa de
  circuito impresso da atividade 3 deu uma trilha curta e paralela para o par.
- **Nenhuma função de bordo.** Na tabela oficial `FRDM-KL25Z Pinouts (Rev 1.0)`,
  PTD6 (pino 79) e PTD7 (pino 80) têm a coluna de uso de bordo vazia. PTD6 tem
  `UART0_RX` e PTD7 tem `UART0_TX` como alternativa, mas o console do OpenSDA usa
  PTA1 e PTA2, então não há conflito, desde que os dois fiquem em MUX 001
  (GPIO), que é o que `encoder_init()` faz.
- **PORTD já estava aberto**: os pinos da ponte H (PTD0, PTD2, PTD3, PTD5) moram
  lá, então o clock do PORTD já está habilitado e não há porto novo no projeto.

Isso foi decidido na **atividade 3**, quando a placa foi desenhada, justamente
porque o enunciado da aula 3 manda consultar os objetivos da aula 4:
`psi3422-atividade-pcb/docs/pinos.md`, seção 4. Pino em cobre não se remaneja.

## 2. Contar as duas bordas, não uma só

O exemplo do professor (arduinoecia, LM393) conta `FALLING` e usa
`pulsos_por_volta = 20`. Aqui o `IRQC` do PCR é `0xB` (as duas bordas), o que dá
**40 pulsos por volta** pelo mesmo disco.

Por que vale: a **resolução angular é o que limita a curva de 90°**. Com 20
pulsos/volta, um giro de 90° tem só ~11 pulsos por roda, e meio pulso de
quantização já são 4,2°. Dobrando para 40, cai para 2,1°. O custo é zero, a
mesma ISR, o mesmo fio.

O que não muda: a largura da abertura não precisa ser igual à do cheio. As duas
bordas por abertura são exatamente duas, independentemente da simetria, então
40 pulsos por volta é exato. O que a assimetria afeta é a *posição* de cada
contagem dentro da volta, e isso se dilui em qualquer distância maior que uma
abertura.

Consequência prática: a polaridade do sensor deixou de importar. LM393 sobe
quando bloqueia, HW-201 desce quando detecta, contando as duas bordas, tanto
faz.

## 3. `irq_connect_dynamic` e não `IRQ_CONNECT`

Esta é a armadilha real desta atividade, e ela é de build, não de bancada.

O nó `gpiod` está `status = "okay"` no `frdm_kl25z.dts` (o LED azul de bordo é
PTD1), e o driver `gpio_mcux.c` faz, para cada instância habilitada:

```c
IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), gpio_mcux_port_isr, ...);
irq_enable(DT_INST_IRQN(n));
```

Ou seja **o vetor 31 já está estaticamente registrado antes de o nosso código
existir**. Verificado no `isr_tables.c` gerado pelo build: as entradas 30 e 31
apontam para o ISR do driver. Um segundo `IRQ_CONNECT(31, ...)` faz o
`gen_isr_tables.py` abortar o build com registro duplicado.

A conexão dinâmica substitui a entrada em tempo de execução. Ela funciona porque
com `CONFIG_DYNAMIC_INTERRUPTS=y` a tabela vai para RAM, conferido no binário:

```
1ffff028 00000100 D _sw_isr_table
```

Seção `D` (dados, RAM), portanto gravável. O mesmo mecanismo já havia sido usado
na atividade do ultrassom, em PSI3441, para o TPM1.

**Consequência assumida e registrada:** substituir o tratador do PORTD desliga o
`gpio_pin_interrupt_configure()` para qualquer pino desse porto. Neste projeto
nada depende disso (o LED azul é saída), mas quem acrescentar um botão em PORTD
depois precisa saber.

Detalhe da ISR: ela lê o `ISFR` inteiro e escreve o valor lido de volta. O `ISFR`
é write-1-to-clear, então isso limpa exatamente as flags que estavam ativas e
não perde borda que chegou entre a leitura e a escrita. Limpar o registrador
inteiro, e não só os nossos dois bits, é de propósito: flag presa de outro pino
manteria o pedido de interrupção ativo para sempre, e o resultado seria a CPU
travada dentro da ISR.

O plano B, com a API de GPIO do Zephyr, está pronto e escrito em
[roteiro-laboratorio.md](roteiro-laboratorio.md), na seção "Plano B".

## 4. Janela de bloqueio em software, porque o KL25Z não tem filtro digital

O sensor é um comparador. Perto do limiar ele pode oscilar e gerar várias bordas
onde passou uma abertura só, e cada borda espúria é um erro cumulativo de
odometria.

O caminho de hardware não existe nesta família: **não há `PORTx_DFER`, `DFCR` nem
`DFWR` no `MKL25Z4.h` do KL25Z**, o filtro digital de pino, que existe em outros
Kinetis, não está aqui. O que existe é o `PFE` (passive filter enable), que é um
filtro de nanossegundos contra glitch de EMI, não de milissegundos contra
chatter de comparador.

Então o filtro é em software: borda que chega antes de `ENC_LOCKOUT_US = 300 µs`
da anterior é descartada e contada como `glitch`.

Dimensionamento: 40 bordas por volta. Um motor TT do kit chega a ~200 rpm sem
carga, ou seja ~3,3 voltas/s, ~133 bordas/s, ~7,5 ms entre bordas. Com abertura
bem mais estreita que o cheio, o menor intervalo legítimo cai para ~2,2 ms.
300 µs fica **7 vezes abaixo** disso, rejeita chatter e não tem como perder
pulso real.

O contador de `glitch` é exposto de propósito: se ele cresce junto com a
contagem, o problema é de bancada (trimpot, ruído, fio) e não de código.

## 5. Alimentar o encoder em 3,3 V

Os pinos do KL25Z **não são tolerantes a 5 V** e a saída digital desses módulos
sai no nível da própria alimentação. Alimentado em 5 V, o encoder entrega 5 V no
PTD6.

É a mesma decisão da atividade 1 para o HC-SR04, e os três sensores possíveis
aceitam: LM393 é especificado de 3 a 5 V, HW-201 de 3,3 a 5 V. O custo é o LED
infravermelho menos brilhante, o que pode pedir reajuste do trimpot, barato de
resolver, ao contrário de um pino queimado.

## 6. Comandar por pulso, não por tempo

Na atividade 1 o ângulo era `T_MEIA_VOLTA_MS = 2500`. Isso foi calibrado no chão
do laboratório com o power bank em cima e vale só ali, porque **duty de PWM é
tensão média, não velocidade**: mudou o piso, o peso ou a carga da bateria, mudou
o ângulo, e nada avisa. É malha aberta.

Com encoder o comando é "gire até as rodas terem andado 22 pulsos". A malha fecha
na grandeza que interessa, e as três variáveis acima deixam de entrar na conta,
elas afetam quanto **tempo** a manobra leva, não onde ela termina.

## 7. Parar pela média das duas rodas

Se uma roda escorrega, a contagem dela sobe menos que a real e a da outra sobe
mais. A média erra menos que qualquer uma das duas, e é ela que estima o
deslocamento do carrinho.

Efeito colateral bom: a média avança **meio pulso** quando qualquer uma das duas
rodas conta, então a resolução efetiva é o dobro da de uma roda, 2,55 mm em vez
de 5,105 mm.

## 8. Uma malha de controle só, três geometrias

Reta, giro no eixo e curva em arco diferem em duas coisas: o sentido de cada roda
e quantos pulsos cada roda tem de dar. Correr, corrigir a simetria, decidir a
hora de frear e vigiar travamento é idêntico nos três, então é um laço só
(`executa()`), parametrizado por uma `struct plano`.

O critério de parada é a média dos progressos fracionários chegando a 1:

```
esq/alvo_esq + dir/alvo_dir >= 2   <=>   esq*alvo_dir + dir*alvo_esq >= 2*alvo_esq*alvo_dir
```

A forma da direita é a implementada: só multiplicação inteira, sem divisão e sem
ponto flutuante no laço. Quando os dois alvos são iguais (reta e giro), ela se
reduz a "a média chegou no alvo". Quando são diferentes (arco), ela mede o
progresso das duas rodas na proporção certa. Uma expressão, três geometrias.

Todo o resto da aritmética também é inteira, incluindo o arco:
`s = graus × π/180 × raio` virou `s = graus × 31416 × raio / 1 800 000`, com
31416 = π×10⁴. Ponto flutuante no Cortex-M0+ é emulado em software.

## 9. `ODO_KP` nasce em zero

Corrigir rumo é sempre **tirar** velocidade de uma roda. Com power bank de 5 V o
motor já está em ~3,2 V no duty máximo e não há de onde tirar: baixar mais faz a
roda parar, e a manobra aborta com `ODO_TRAVOU`.

Então o padrão é 0, que é o valor que não pode causar falha de laboratório. Com
bateria de 7,4 a 9 V sobra folga e 2 a 4 funciona bem, e é com a correção ligada
que o quadrado do MODO 7 fecha direito.

A correção de rumo que **não** custa folga é o `TRIM_ESQ`/`TRIM_DIR` de
`motores.c`: um ganho constante e pequeno no lado mais rápido. O MODO 2 imprime
exatamente o número necessário (`desvio entre rodas`).

## 10. Falhar alto, nunca calado

Manobra que não termina tem de parar o carrinho e aparecer:

- **`ODO_TRAVOU`**, uma roda passou `ODO_PARADO_MS = 800 ms` sem gerar borda.
  É a proteção que importa de verdade: encoder solto, fio caído, roda atolada,
  ou carrinho contra a parede. Aborta em menos de 1 s.
- **`ODO_TIMEOUT`**, rede de segurança, `15 s`.
- **`ODO_PARAMETRO`**, raio de curva menor que meia bitola pediria a roda de
  dentro girando ao contrário, e isso é giro, não curva. Recusa em vez de fazer
  silenciosamente outra coisa.

Os três acendem o LED vermelho piscando e imprimem o motivo por extenso. O que
**não** existe é o caso "não deu certo, segue em frente": carrinho com motor
ligado e malha aberta é o modo de falha caro desta atividade.

## 11. O alvo em pulsos está compilado, de propósito

Com o carrinho no chão alimentado pelo power bank **não existe console**. Se a
calibração dependesse de ler um número no terminal, ela só poderia ser feita com
a placa presa ao notebook por um cabo de 1 m, o que limita a corrida a menos de
um metro e piora a precisão.

Por isso os MODOs 2 e 3 andam/giram um número **conhecido e compilado** de
pulsos, e quem mede é a trena e a régua. O terminal continua imprimindo tudo,
para quando ele estiver ligado, mas não é necessário.

## 12. Amplificar para medir: 4 giros em vez de 1

Medir 2° de erro angular com goniômetro de laboratório é ruim. Quatro giros de
90° fecham uma volta inteira, então o desalinhamento final é o erro de um giro
**multiplicado por 4**, 2° viram 8°, e 8° dá para ler com régua em dois pontos
do chassi.

É mais barato amplificar o efeito que aumentar a precisão do instrumento. A mesma
ideia está no passo 1 da calibração: 10 voltas na mão em vez de 1, para diluir
por 10 o erro de quem conta a volta.

## 13. O que não foi feito, e por quê

- **Encoder em quadratura.** Dois canais defasados dariam contagem **com sinal**
  (sabe para que lado a roda gira) e resolução 4×. O kit não tem, e improvisar um
  segundo sensor por roda com o disco de 20 aberturas não daria a defasagem de
  90° elétricos de forma confiável. Fica declarado como limitação: aqui o sentido
  vem de quem comanda os motores, e a contagem é zerada antes de cada manobra.
- **Controle de velocidade (PID).** A malha fecha em posição acumulada, e a
  velocidade continua sendo duty em malha aberta. Fazia sentido se a atividade
  pedisse velocidade constante, e ela não pede.
- **Aproximação lenta antes de parar.** Reduzir a velocidade perto do alvo
  cortaria a escorregada, e é o caminho natural de melhoria, mas exige folga de
  tensão que o power bank de 5 V não dá. Com bateria, vale fazer.
- **O ultrassom da atividade 1.** Ficou fora: as manobras aqui são curtas e
  comandadas, e menos hardware no ar é menos coisa para depurar no laboratório.
  Voltar é copiar `lib/hcsr04` do repo do carrinho.

## 14. Fontes

- `FRDM-KL25Z Pinouts (Rev 1.0)`, tabela de conexões dos conectores I/O e o
  gráfico de compatibilidade com Arduino R3, posição de PTD6/PTD7 e a coluna
  "Interrupt". Em `usp-2026-2/psi3422/material/frdm-kl25z/`.
- `MKL25Z4.h` do SDK da NXP (instalado pelo PlatformIO em
  `~/.platformio/packages/framework-zephyr/_pio/modules/hal/nxp/mcux/mcux-sdk/devices/MKL25Z4/`):
  `PORT_IRQS`, `PORTD_IRQn = 31`, `PORT_PCR_IRQC`, layout do `PORT_Type`
  (`PCR[32]` + `ISFR`), e a ausência de `DFER`/`DFCR`/`DFWR`.
- `nxp_kl25z.dtsi` e `frdm_kl25z.dts` do Zephyr instalado, quais `gpio` declaram
  `interrupts`, e quais estão `okay`.
- `drivers/gpio/gpio_mcux.c` do Zephyr, o `IRQ_CONNECT` por instância.
- `psi3422-atividade-pcb/docs/pinos.md` e `docs/montagem.md`, o plano de pinos
  que reservou PTD6 e PTD7 para esta atividade, e o mapa de fios da placa.
- `psi3422-atividade-carrinho`, `lib/motores` veio de lá sem mudança.
- Sensor de velocidade LM393: `arduinoecia.com.br/sensor-de-velocidade-lm393-arduino/`
  (4 pinos, VCC 3 a 5 V, D0 vai a 1 quando bloqueia, disco de **20 aberturas**,
  exemplo contando `FALLING`).
- Sensor de obstáculo IR HW-201:
  `blogdarobotica.com/2023/04/18/como-utilizar-o-sensor-de-obstaculo-reflexivo-infravermelho-ir-com-arduino/`
  (3 pinos, VCC 3,3 a 5 V, OUT vai a 0 quando detecta, trimpot de sensibilidade).
