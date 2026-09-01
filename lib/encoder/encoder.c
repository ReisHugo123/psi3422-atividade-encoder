/*
 * encoder.c - contagem de bordas de PTD6 e PTD7 pela interrupcao do PORTD.
 *
 * Uma ISR atende as duas rodas: o PORTD tem um vetor so e o ISFR diz qual pino
 * interrompeu. As duas bordas de cada abertura sao contadas, o que da 40 pulsos
 * por volta com o disco de 20 do kit.
 *
 * O porque de cada escolha (IRQ dinamico, janela de bloqueio, pull-up) esta em
 * docs/decisoes.md, secoes 1 a 5.
 */
#include "encoder.h"

#include <MKL25Z4.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>

#define ENC_ESQ_PIN     6u
#define ENC_DIR_PIN     7u
#define PORTD_IRQ      31u

#define IRQC_DUAS_BORDAS  0xBu   /* 0x9 subida, 0xA descida, 0xB as duas */

/* 300 us fica 7x abaixo do menor intervalo legitimo entre bordas na rotacao
 * maxima. O KL25Z nao tem filtro digital de pino, entao o filtro e aqui. */
#define ENC_LOCKOUT_US   300u

static const uint8_t pino[2] = { ENC_ESQ_PIN, ENC_DIR_PIN };

static volatile uint32_t contagem[2];
static volatile uint32_t glitch[2];
static volatile uint32_t t_ultima[2];

static void conta_borda(int i, uint32_t agora)
{
	if ((agora - t_ultima[i]) < k_us_to_cyc_ceil32(ENC_LOCKOUT_US)) {
		glitch[i]++;
		return;
	}
	t_ultima[i] = agora;
	contagem[i]++;
}

static void portd_isr(const void *arg)
{
	ARG_UNUSED(arg);

	/* Le tudo e devolve tudo: o ISFR e write-1-to-clear, entao isso limpa
	 * exatamente as flags ativas. Limpar as de outros pinos tambem evita que
	 * flag presa mantenha o pedido de interrupcao ativo para sempre. */
	uint32_t flags = PORTD->ISFR;

	if (flags == 0u) {
		return;
	}
	PORTD->ISFR = flags;

	uint32_t agora = k_cycle_get_32();

	if (flags & (1u << ENC_ESQ_PIN)) {
		conta_borda(ENC_ESQ, agora);
	}
	if (flags & (1u << ENC_DIR_PIN)) {
		conta_borda(ENC_DIR, agora);
	}
}

void encoder_init(void)
{
	SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;

	for (int i = 0; i < 2; i++) {
		/* pull-up interno da nivel definido se o fio do encoder cair; sem ele
		 * o pino solto gera contagem fantasma */
		PORTD->PCR[pino[i]] = PORT_PCR_MUX(1)
				    | PORT_PCR_PE_MASK
				    | PORT_PCR_PS_MASK
				    | PORT_PCR_IRQC(IRQC_DUAS_BORDAS);
		GPIOD->PDDR &= ~(1u << pino[i]);
	}

	PORTD->ISFR = (1u << ENC_ESQ_PIN) | (1u << ENC_DIR_PIN);

	encoder_zera();

	/* dinamico, e nao IRQ_CONNECT: o driver de GPIO do Zephyr ja registrou o
	 * vetor 31 estaticamente, e dois registros no mesmo IRQ quebram o build */
	irq_connect_dynamic(PORTD_IRQ, 2, portd_isr, NULL, 0);
	irq_enable(PORTD_IRQ);
}

uint32_t encoder_conta(enc_t e)
{
	return contagem[e];
}

void encoder_zera(void)
{
	unsigned int chave = irq_lock();

	uint32_t agora = k_cycle_get_32();

	for (int i = 0; i < 2; i++) {
		contagem[i] = 0u;
		glitch[i]   = 0u;
		t_ultima[i] = agora;
	}
	irq_unlock(chave);
}

/* As duas contagens saem do mesmo instante, senao media e diferenca misturam
 * leituras de momentos diferentes. */
static void le_par(uint32_t *esq, uint32_t *dir)
{
	unsigned int chave = irq_lock();

	*esq = contagem[ENC_ESQ];
	*dir = contagem[ENC_DIR];
	irq_unlock(chave);
}

uint32_t encoder_media(void)
{
	uint32_t esq, dir;

	le_par(&esq, &dir);

	return (esq + dir + 1u) / 2u;
}

int32_t encoder_dif(void)
{
	uint32_t esq, dir;

	le_par(&esq, &dir);

	return (int32_t)esq - (int32_t)dir;
}

uint32_t encoder_glitches(enc_t e)
{
	return glitch[e];
}

uint32_t encoder_us_parado(enc_t e)
{
	unsigned int chave = irq_lock();
	uint32_t t = t_ultima[e];
	irq_unlock(chave);

	return k_cyc_to_us_floor32(k_cycle_get_32() - t);
}

int encoder_nivel(enc_t e)
{
	return (int)((GPIOD->PDIR >> pino[e]) & 1u);
}
