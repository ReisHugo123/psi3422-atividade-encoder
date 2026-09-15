/*
 * PSI3422 - Atividade 4: encoders nas rodas do carrinho 2WD.
 * FRDM-KL25Z + ponte H L298N + dois encoders opticos de canal unico.
 *
 * O #define MODO escolhe o degrau. Os modos 1 a 3 MEDEM (calibracao), os
 * modos 4 a 6 entregam os objetivos 2 e 3 do enunciado, e o 7 e a
 * demonstracao. Roteiro de bancada em docs/roteiro-laboratorio.md.
 *
 *   1 bancada  2 calibra reta  3 calibra giro  4 anda 1 m  5 giro 90
 *   6 arco 90  7 quadrado
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <encoder.h>
#include <motores.h>
#include <odometria.h>

#define MODO   1

#define CAL_PULSOS        400   /* modo 2: pulsos de reta, ~2 m */
#define CAL_PULSOS_GIRO   ODO_PULSOS_90
#define CAL_GIRO_DIREITA    1

/* Modo 3: 4 giros de 90 fecham a volta, entao o desalinhamento final e o erro
 * de um giro multiplicado por 4. Amplificar sai mais barato que medir melhor. */
#define CAL_GIRO_REPETE     4

#define DEMO_RETA_MM     1000
#define DEMO_LADO_MM      500
#define DEMO_RAIO_MM      250
#define PAUSA_MS         4000

/* Aliases da FRDM: led0 verde (PTB19), led1 azul (PTD1), led2 vermelho (PTB18) */
static const struct gpio_dt_spec led_verde = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_azul  = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led_verm  = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

enum cor {
	APAGADO,
	VERDE,
	VERMELHO,
	AZUL,
	AMARELO,
};

static void leds_init(void)
{
	gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_azul,  GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_verm,  GPIO_OUTPUT_INACTIVE);
}

static void led_cor(enum cor c)
{
	/* o Device Tree ja declara active-low, entao 1 acende */
	gpio_pin_set_dt(&led_verde, (c == VERDE)    || (c == AMARELO));
	gpio_pin_set_dt(&led_verm,  (c == VERMELHO) || (c == AMARELO));
	gpio_pin_set_dt(&led_azul,   c == AZUL);
}

static void cartao_de_calibracao(void)
{
	printk("\n=== PSI3422 atividade 4 - encoders (MODO %d) ===\n", MODO);
	printk("encoders: ENC_ESQ=PTD6 (J2-17), ENC_DIR=PTD7 (J2-19), 3V3 e GND\n");
	printk("motores : ENA=PTD2 IN1=PTD0 IN2=PTD5 | ENB=PTD3 IN3=PTE0 IN4=PTE1\n");
	printk("disco %d aberturas x %d bordas = %d pulsos por volta da roda\n",
	       ODO_ABERTURAS_DISCO, ODO_BORDAS_POR_ABERT, ODO_PULSOS_POR_VOLTA);
	printk("roda %d mm | entre-rodas %d mm\n",
	       ODO_DIAM_RODA_MM, ODO_ENTRE_RODAS_MM);
	printk("CALIBRACAO EM USO: %d pulsos/m | %d pulsos por 90 graus\n",
	       ODO_PULSOS_POR_M, ODO_PULSOS_90);
	printk("escorregada compensada: reta %d, giro %d | ODO_KP %d\n",
	       ODO_ESCORREGO_RETO, ODO_ESCORREGO_GIRO, ODO_KP);
}

#if MODO != 1
static void contagem_regressiva(const char *o_que, int segundos)
{
	printk("\n>>> %s - largando em %d s (afaste as maos)\n", o_que, segundos);
	for (int i = segundos; i > 0; i--) {
		led_cor(i % 2 ? AZUL : APAGADO);
		k_msleep(1000);
	}
	led_cor(APAGADO);
}

static void pausa(const char *o_que)
{
	printk("--- %s: %d ms de pausa ---\n", o_que, PAUSA_MS);
	led_cor(APAGADO);
	k_msleep(PAUSA_MS);
}

static void sinaliza(const odo_relato_t *r)
{
	if (r->res != ODO_OK) {
		for (int i = 0; i < 6; i++) {
			led_cor(i % 2 ? VERMELHO : APAGADO);
			k_msleep(200);
		}
	}
}
#endif /* MODO != 1 */

#if MODO == 1
/* Fase A: elimina em 30 s o erro mais comum da montagem, fio na casa errada. */
static void fase_nivel(void)
{
	printk("\n[A] nivel bruto dos pinos - 12 s\n");
	printk("    tape e destape cada sensor com o dedo ou um papel.\n");
	printk("    o numero da roda correspondente TEM de mudar de 0 para 1.\n");
	printk("    se nao muda: pino errado, sem 3V3, ou GND solto.\n");

	for (int i = 0; i < 48; i++) {
		printk("    esq=%d dir=%d   (conta esq=%u dir=%u)\n",
		       encoder_nivel(ENC_ESQ), encoder_nivel(ENC_DIR),
		       encoder_conta(ENC_ESQ), encoder_conta(ENC_DIR));
		k_msleep(250);
	}
}

static void gira_um(const char *quem, motor_t m, int vel)
{
	printk("\n    %s a %d%%\n", quem, vel);
	encoder_zera();
	motor_set(m, vel);

	for (int i = 0; i < 8; i++) {
		k_msleep(250);
		uint32_t e = encoder_conta(ENC_ESQ);
		uint32_t d = encoder_conta(ENC_DIR);
		printk("      t=%d ms  esq=%u  dir=%u   (%u e %u pulsos/s)\n",
		       (i + 1) * 250, e, d,
		       (e * 1000u) / (uint32_t)((i + 1) * 250),
		       (d * 1000u) / (uint32_t)((i + 1) * 250));
	}
	carrinho_para();
	k_msleep(700);
}

/* Fase B: amarra cada encoder ao seu motor. Girando a roda esquerda, a
 * contagem da direita tem de ficar em zero. */
static void fase_motores(void)
{
	printk("\n[B] um motor de cada vez, RODAS NO AR - 6 s\n");
	printk("    a contagem da roda parada tem de ficar em zero.\n");
	led_cor(VERDE);
	gira_um("motor esquerdo para frente", MOTOR_ESQ, ODO_VEL_RETO);
	gira_um("motor direito para frente",  MOTOR_DIR, ODO_VEL_RETO);
	led_cor(APAGADO);
}

/* Fase C: 10 voltas na mao diluem por 10 o erro de quem conta a volta. */
static void fase_pulsos_por_volta(void)
{
	printk("\n[C] pulsos por volta, na mao - 20 s\n");
	printk("    motores desligados. Gire UMA roda EXATAMENTE 10 voltas,\n");
	printk("    devagar, marcando o ponto de partida com fita.\n");
	printk("    esperado %d pulsos por volta. Metade disso = uma borda so;\n",
	       ODO_PULSOS_POR_VOLTA);
	printk("    muito mais = ruido, ver docs/calibracao.md\n");

	encoder_zera();
	for (int i = 0; i < 40; i++) {
		k_msleep(500);
		printk("      esq=%u (glitch %u)   dir=%u (glitch %u)\n",
		       encoder_conta(ENC_ESQ), encoder_glitches(ENC_ESQ),
		       encoder_conta(ENC_DIR), encoder_glitches(ENC_DIR));
	}
	printk("    FIM: esq=%u -> %u pulsos/volta | dir=%u -> %u pulsos/volta\n",
	       encoder_conta(ENC_ESQ), encoder_conta(ENC_ESQ) / 10u,
	       encoder_conta(ENC_DIR), encoder_conta(ENC_DIR) / 10u);
}
#endif /* MODO == 1 */

int main(void)
{
	leds_init();
	odo_init();
	cartao_de_calibracao();

#if MODO != 1
	odo_relato_t r;
#endif

#if MODO == 1
	while (1) {
		fase_nivel();
		fase_motores();
		fase_pulsos_por_volta();
		printk("\n=== reiniciando o roteiro de bancada ===\n");
	}

#elif MODO == 2
	/* ODO_PULSOS_POR_M = CAL_PULSOS * 1000 / distancia_medida_mm */
	printk("\nMODO 2 - calibracao da reta: %d pulsos por corrida\n", CAL_PULSOS);
	printk("marque onde o EIXO DAS RODAS esta antes de largar e onde ele para,\n");
	printk("e meca a distancia entre as duas marcas.\n");
	printk("depois: ODO_PULSOS_POR_M = %d * 1000 / mm_medidos\n", CAL_PULSOS);

	while (1) {
		contagem_regressiva("reta de calibracao", 5);
		led_cor(VERDE);
		odo_anda_pulsos(CAL_PULSOS, ODO_VEL_RETO, 0, &r);
		odo_imprime("calibra reta", &r);
		sinaliza(&r);
		pausa("meca e anote");
	}

#elif MODO == 3
	/* ODO_PULSOS_90 = pulsos * 4 * 90 / graus_totais_girados */
	printk("\nMODO 3 - calibracao do giro: %d giros de %d pulsos por roda\n",
	       CAL_GIRO_REPETE, CAL_PULSOS_GIRO);
	printk("alinhe a lateral do chassi com uma fita reta no chao. Depois dos\n");
	printk("%d giros ele deveria ter fechado %d graus e voltado ao rumo.\n",
	       CAL_GIRO_REPETE, CAL_GIRO_REPETE * 90);
	printk("o que sobrar e o erro de UM giro multiplicado por %d.\n",
	       CAL_GIRO_REPETE);
	printk("depois: ODO_PULSOS_90 = %d * %d * 90 / graus_totais\n",
	       CAL_PULSOS_GIRO, CAL_GIRO_REPETE);

	while (1) {
		contagem_regressiva("sequencia de giros", 5);

		for (int i = 1; i <= CAL_GIRO_REPETE; i++) {
			led_cor(AMARELO);
			odo_gira_pulsos(CAL_PULSOS_GIRO, ODO_VEL_GIRO,
					CAL_GIRO_DIREITA, &r);
			printk("giro %d de %d:\n", i, CAL_GIRO_REPETE);
			odo_imprime("calibra giro", &r);
			sinaliza(&r);
			if (r.res != ODO_OK) {
				break;
			}
			led_cor(APAGADO);
			k_msleep(800);
		}
		pausa("meca o desalinhamento em relacao a fita e anote");
	}

#elif MODO == 4
	printk("\nMODO 4 - objetivo 2: %d mm medidos, ida e volta\n", DEMO_RETA_MM);

	while (1) {
		contagem_regressiva("ida", 5);
		led_cor(VERDE);
		odo_anda_mm(DEMO_RETA_MM, ODO_VEL_RETO, &r);
		odo_imprime("ida", &r);
		sinaliza(&r);
		pausa("meca o erro da ida");

		contagem_regressiva("volta", 3);
		led_cor(VERDE);
		odo_anda_mm(-DEMO_RETA_MM, ODO_VEL_RETO, &r);
		odo_imprime("volta", &r);
		sinaliza(&r);
		pausa("o carrinho deveria estar de volta na marca");
	}

#elif MODO == 5
	printk("\nMODO 5 - objetivo 3: giro de 90 graus no eixo\n");

	while (1) {
		contagem_regressiva("90 graus a DIREITA", 5);
		led_cor(AMARELO);
		odo_gira_graus(90, ODO_VEL_GIRO, &r);
		odo_imprime("90 graus direita", &r);
		sinaliza(&r);
		pausa("confira com o esquadro");

		contagem_regressiva("90 graus a ESQUERDA", 3);
		led_cor(AMARELO);
		odo_gira_graus(-90, ODO_VEL_GIRO, &r);
		odo_imprime("90 graus esquerda", &r);
		sinaliza(&r);
		pausa("deveria ter voltado ao rumo original");
	}

#elif MODO == 6
	/* Precisa de bateria de 7,4 a 9 V: a roda de dentro vai a ~56% e com
	 * power bank de 5 V isso nao sai do lugar. */
	printk("\nMODO 6 - objetivo 3: curva de 90 graus em arco, raio %d mm\n",
	       DEMO_RAIO_MM);

	while (1) {
		contagem_regressiva("arco de 90 graus a DIREITA", 5);
		led_cor(AMARELO);
		odo_curva_graus(90, DEMO_RAIO_MM, ODO_VEL_RETO, &r);
		odo_imprime("arco 90 direita", &r);
		sinaliza(&r);
		pausa("confira o rumo e o raio");

		contagem_regressiva("arco de 90 graus a ESQUERDA", 3);
		led_cor(AMARELO);
		odo_curva_graus(-90, DEMO_RAIO_MM, ODO_VEL_RETO, &r);
		odo_imprime("arco 90 esquerda", &r);
		sinaliza(&r);
		pausa("deveria ter voltado ao rumo original");
	}

#else
	/* O quadrado testa as duas calibracoes juntas: erro de distancia deforma
	 * os lados, erro de angulo abre o quadrado, e as duas coisas aparecem em
	 * quanto ele errou o ponto de partida. */
	printk("\nMODO 7 - quadrado de %d mm, 4 lados e 4 giros de 90 graus\n",
	       DEMO_LADO_MM);
	printk("marque o ponto e o rumo de partida antes de largar.\n");

	while (1) {
		contagem_regressiva("quadrado", 5);

		for (int lado = 1; lado <= 4; lado++) {
			led_cor(VERDE);
			odo_anda_mm(DEMO_LADO_MM, ODO_VEL_RETO, &r);
			printk("lado %d de 4:\n", lado);
			odo_imprime("reta", &r);
			sinaliza(&r);
			if (r.res != ODO_OK) {
				break;
			}
			k_msleep(400);

			led_cor(AMARELO);
			odo_gira_graus(90, ODO_VEL_GIRO, &r);
			odo_imprime("canto", &r);
			sinaliza(&r);
			if (r.res != ODO_OK) {
				break;
			}
			k_msleep(400);
		}
		pausa("meca a distancia do ponto final ate a marca de partida");
	}
#endif

	return 0;
}
