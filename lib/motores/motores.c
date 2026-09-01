/*
 * motores.c - ponte H L298N na FRDM-KL25Z, escrevendo direto nos registradores.
 *
 *   Motor A / roda esquerda          Motor B / roda direita
 *     ENA = PTD2 (D11) TPM0_CH2        ENB = PTD3 (D12) TPM0_CH3
 *     IN1 = PTD0 (D10) GPIO            IN3 = PTB2 (A2)  GPIO
 *     IN2 = PTD5 (D9)  GPIO            IN4 = PTB3 (A3)  GPIO
 *
 * PTD0 a PTD5 sao os canais 0 a 5 do TPM0 na funcao ALT4. Como os dois EN ficam
 * no mesmo TPM, eles compartilham a frequencia (o TPM tem um contador e um MOD
 * por instancia), mas cada canal tem o seu CnV - o seu duty. E isso que permite
 * a curva: mesma frequencia, velocidades diferentes.
 *
 * f_pwm = 48 MHz / (prescaler 16 * 1000 passos) = 3 kHz.
 */
#include "motores.h"
#include <MKL25Z4.h>

#define EN_ESQ_CH     2u      /* TPM0_CH2 */
#define EN_DIR_CH     3u      /* TPM0_CH3 */
#define EN_ESQ_PIN    2u      /* PTD2 */
#define EN_DIR_PIN    3u      /* PTD3 */

#define IN1_PIN       0u      /* PTD0 */
#define IN2_PIN       5u      /* PTD5 */
#define IN3_PIN       2u      /* PTB2 */
#define IN4_PIN       3u      /* PTB3 */

#define TPM_SRC_FLL   1u      /* TPMSRC = 1 -> MCGFLLCLK (48 MHz) */
#define PS_16         4u
#define PWM_PASSOS    1000u   /* MOD + 1 */

/* Abaixo de ~30% de duty o motor com o carrinho em cima nao sai do lugar, so
 * zumbe. A velocidade pedida (1 a 100) e reescalada para 30% a 100%. */
#define DUTY_MIN      300u

/* Ajustes de bancada, para nao ter que mexer na fiacao: INVERTE_x = 1 se aquela
 * roda gira ao contrario, TRIM_x < 100 para segurar o lado mais rapido quando o
 * carrinho puxa para um lado andando reto. */
#define INVERTE_ESQ   0
#define INVERTE_DIR   0
#define TRIM_ESQ      100
#define TRIM_DIR      100

static void pino_saida(PORT_Type *port, GPIO_Type *gpio, uint8_t pin)
{
	port->PCR[pin] = PORT_PCR_MUX(1);   /* MUX 001 = GPIO */
	gpio->PDDR |= (1u << pin);
	gpio->PCOR  = (1u << pin);
}

/* (1,0) e (0,1) sao os dois sentidos, (0,0) solta o motor, (1,1) freia. */
static void motor_pinos(motor_t motor, uint8_t a, uint8_t b)
{
	GPIO_Type *gpio = (motor == MOTOR_ESQ) ? GPIOD : GPIOB;
	uint32_t   m_a  = (motor == MOTOR_ESQ) ? (1u << IN1_PIN) : (1u << IN3_PIN);
	uint32_t   m_b  = (motor == MOTOR_ESQ) ? (1u << IN2_PIN) : (1u << IN4_PIN);

	if (a) {
		gpio->PSOR = m_a;
	} else {
		gpio->PCOR = m_a;
	}

	if (b) {
		gpio->PSOR = m_b;
	} else {
		gpio->PCOR = m_b;
	}
}

static uint32_t vel_para_duty(uint32_t modulo)
{
	if (modulo == 0u) {
		return 0u;
	}
	return DUTY_MIN + (modulo * (PWM_PASSOS - DUTY_MIN)) / 100u;
}

static void motor_duty(motor_t motor, uint32_t duty)
{
	uint8_t ch = (motor == MOTOR_ESQ) ? EN_ESQ_CH : EN_DIR_CH;

	if (duty > PWM_PASSOS) {
		duty = PWM_PASSOS;
	}
	TPM0->CONTROLS[ch].CnV = duty;
}

void motores_init(void)
{
	/* clock dos perifericos primeiro: sem isso as escritas se perdem */
	SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTD_MASK;
	SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
	SIM->SOPT2  = (SIM->SOPT2 & ~SIM_SOPT2_TPMSRC_MASK) | SIM_SOPT2_TPMSRC(TPM_SRC_FLL);

	/* sentido: quatro saidas digitais, todas em 0 (motores soltos) */
	pino_saida(PORTD, GPIOD, IN1_PIN);
	pino_saida(PORTD, GPIOD, IN2_PIN);
	pino_saida(PORTB, GPIOB, IN3_PIN);
	pino_saida(PORTB, GPIOB, IN4_PIN);

	/* velocidade: PTD2 e PTD3 saem do GPIO e vao para o TPM0 (ALT4) */
	PORTD->PCR[EN_ESQ_PIN] = PORT_PCR_MUX(4);
	PORTD->PCR[EN_DIR_PIN] = PORT_PCR_MUX(4);

	TPM0->SC  = 0u;                     /* contador parado durante a configuracao */
	TPM0->CNT = 0u;
	TPM0->MOD = PWM_PASSOS - 1u;

	/* MSB|ELSB = PWM alinhado pela borda, alto enquanto CNT < CnV */
	TPM0->CONTROLS[EN_ESQ_CH].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;
	TPM0->CONTROLS[EN_ESQ_CH].CnV  = 0u;
	TPM0->CONTROLS[EN_DIR_CH].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;
	TPM0->CONTROLS[EN_DIR_CH].CnV  = 0u;

	TPM0->SC = TPM_SC_CMOD(1) | TPM_SC_PS(PS_16);
}

void motor_set(motor_t motor, int vel)
{
	int inverte = (motor == MOTOR_ESQ) ? INVERTE_ESQ : INVERTE_DIR;
	int trim    = (motor == MOTOR_ESQ) ? TRIM_ESQ    : TRIM_DIR;

	if (vel > VEL_MAX) {
		vel = VEL_MAX;
	}
	if (vel < -VEL_MAX) {
		vel = -VEL_MAX;
	}
	if (inverte) {
		vel = -vel;
	}

	if (vel == 0) {
		motor_pinos(motor, 0u, 0u);
		motor_duty(motor, 0u);
		return;
	}

	uint32_t modulo = (vel > 0) ? (uint32_t)vel : (uint32_t)(-vel);
	modulo = (modulo * (uint32_t)trim) / 100u;

	motor_pinos(motor, (vel > 0) ? 1u : 0u, (vel > 0) ? 0u : 1u);
	motor_duty(motor, vel_para_duty(modulo));
}

int motor_duty_pct(int vel)
{
	if (vel > VEL_MAX) {
		vel = VEL_MAX;
	}
	if (vel < -VEL_MAX) {
		vel = -VEL_MAX;
	}

	uint32_t modulo = (vel > 0) ? (uint32_t)vel : (uint32_t)(-vel);

	return (int)(vel_para_duty(modulo) / 10u);
}

void carrinho_frente(int vel)
{
	motor_set(MOTOR_ESQ, vel);
	motor_set(MOTOR_DIR, vel);
}

void carrinho_re(int vel)
{
	motor_set(MOTOR_ESQ, -vel);
	motor_set(MOTOR_DIR, -vel);
}

void carrinho_curva_dir(int vel_externa, int vel_interna)
{
	motor_set(MOTOR_ESQ, vel_externa);   /* a roda de fora corre mais */
	motor_set(MOTOR_DIR, vel_interna);
}

void carrinho_curva_esq(int vel_externa, int vel_interna)
{
	motor_set(MOTOR_ESQ, vel_interna);
	motor_set(MOTOR_DIR, vel_externa);
}

void carrinho_gira_dir(int vel)
{
	motor_set(MOTOR_ESQ,  vel);
	motor_set(MOTOR_DIR, -vel);
}

void carrinho_gira_esq(int vel)
{
	motor_set(MOTOR_ESQ, -vel);
	motor_set(MOTOR_DIR,  vel);
}

void carrinho_para(void)
{
	motor_set(MOTOR_ESQ, 0);
	motor_set(MOTOR_DIR, 0);
}

void carrinho_freia(void)
{
	/* as duas entradas em alto curto-circuitam o motor pela ponte: a energia da
	 * inercia queima no proprio enrolamento. O EN tem que estar habilitado,
	 * senao as saidas ficam em alta impedancia e o motor so solta. */
	motor_pinos(MOTOR_ESQ, 1u, 1u);
	motor_pinos(MOTOR_DIR, 1u, 1u);
	motor_duty(MOTOR_ESQ, PWM_PASSOS);
	motor_duty(MOTOR_DIR, PWM_PASSOS);
}
