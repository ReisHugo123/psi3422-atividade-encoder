/*
 * encoder.h - dois encoders opticos de canal unico nas rodas.
 *
 * ENC_ESQ -> PTD6 (J2-17), ENC_DIR -> PTD7 (J2-19), VCC em 3V3.
 * PORTD porque so PORTA e PORTD tem interrupcao de pino no KL25Z.
 * Contagem SEM SINAL: canal unico nao sabe o sentido, quem sabe e a camada
 * que comanda os motores, e por isso ela zera antes de cada manobra.
 */
#ifndef ENCODER_H_
#define ENCODER_H_

#include <stdint.h>

typedef enum {
	ENC_ESQ = 0,
	ENC_DIR = 1,
} enc_t;

void encoder_init(void);
void encoder_zera(void);

uint32_t encoder_conta(enc_t e);

/* Media das duas rodas: se uma escorrega, a media erra menos que qualquer uma. */
uint32_t encoder_media(void);

/* esquerda - direita. Positivo = o carrinho esta puxando para a direita. */
int32_t encoder_dif(void);

/* Bordas recusadas pela janela de bloqueio. Crescendo junto com a contagem, e
 * ruido ou comparador oscilando no limiar: trimpot, nao codigo. */
uint32_t encoder_glitches(enc_t e);

/* Microssegundos desde a ultima borda. Detecta roda travada ou encoder solto. */
uint32_t encoder_us_parado(enc_t e);

/* Nivel bruto do pino, para o teste de fiacao do MODO 1. */
int encoder_nivel(enc_t e);

#endif /* ENCODER_H_ */
