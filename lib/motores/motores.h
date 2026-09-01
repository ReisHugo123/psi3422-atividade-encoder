/*
 * motores.h - tracao do carrinho 2WD com ponte H L298N.
 *
 * Cada motor tem duas entradas de sentido em GPIO e uma de habilitacao ligada
 * num canal de PWM do TPM0. As entradas de sentido dizem para que lado a
 * corrente atravessa o motor; o PWM no EN define a tensao media, ou seja, a
 * velocidade.
 *
 * A velocidade entra sempre como -100 a +100: o modulo e o quanto de PWM, o
 * sinal e o sentido. Curva = velocidades diferentes nas duas rodas.
 */
#ifndef MOTORES_H_
#define MOTORES_H_

#include <stdint.h>

#define VEL_MAX   100

typedef enum {
	MOTOR_ESQ = 0,
	MOTOR_DIR = 1,
} motor_t;

/* Configura o TPM0 e as quatro saidas de sentido. Deixa os motores parados. */
void motores_init(void);

void motor_set(motor_t motor, int vel);

/* Duty (%) que aquela velocidade produz de fato no pino EN - nao e igual a
 * velocidade pedida, porque ha a compensacao da zona morta no meio. */
int motor_duty_pct(int vel);

void carrinho_frente(int vel);
void carrinho_re(int vel);

/* Curva: as duas rodas no mesmo sentido, a de fora mais rapida - descreve um arco. */
void carrinho_curva_dir(int vel_externa, int vel_interna);
void carrinho_curva_esq(int vel_externa, int vel_interna);

/* Giro no proprio eixo: rodas em sentidos opostos, raio quase zero. */
void carrinho_gira_dir(int vel);
void carrinho_gira_esq(int vel);

/* Solta os motores - o carrinho ainda desliza pela inercia. */
void carrinho_para(void);

/* Curto-circuita os terminais dos motores (freio dinamico): para bem mais rapido. */
void carrinho_freia(void);

#endif /* MOTORES_H_ */
