/*
 * odometria.h - movimento medido, em cima de lib/motores e lib/encoder.
 *
 * Na atividade 1 o angulo era tempo, e tempo depende de piso, peso e carga da
 * bateria. Aqui o comando e em pulsos: a manobra acaba quando as rodas
 * giraram o quanto se pediu.
 *
 * Os dois numeros que traduzem pulso em mundo real se MEDEM, nao se calculam.
 * Procedimento e tabelas em docs/calibracao.md.
 */
#ifndef ODOMETRIA_H_
#define ODOMETRIA_H_

#include <stdint.h>
#include "encoder.h"

/* ==================== cartao de calibracao ==================== */

/* Geometria, conferir com regua antes de acreditar. O alvo nao e disco
 * vazado: sao 8 marcas impressas em cada roda, iguais nas duas, lidas
 * por refletancia pelo HW-201. Este numero so entra no banner e no valor
 * esperado da fase C, a conta usa os dois numeros medidos abaixo. */
#define ODO_ABERTURAS_DISCO     8
#define ODO_BORDAS_POR_ABERT    2
#define ODO_PULSOS_POR_VOLTA   (ODO_ABERTURAS_DISCO * ODO_BORDAS_POR_ABERT)
#define ODO_DIAM_RODA_MM       72
#define ODO_ENTRE_RODAS_MM     170

/* Os dois numeros medidos. Trocar estes dois E a calibracao.
 *
 * ATENCAO: os valores abaixo sao ESTIMATIVA para o alvo novo, 8 marcas
 * impressas por roda e o mesmo numero nas duas. NAO valem antes de medir no
 * MODO 2 e no MODO 3.
 *
 * A estimativa sai da calibracao anterior: 62 bordas por metro com 14 bordas
 * por volta dao 226 mm de circunferencia de pneu, e o MODO 5 denunciou 170 mm
 * de entre-rodas. Com 32 bordas somadas por volta o passo de decisao cai de
 * 16,1 para 7,1 mm e o degrau do giro de 22 para 9,5 graus.
 *
 * LICAO da calibracao anterior, que ficou com POR_M = 34: as duas medidas dela
 * discordavam 36 por cento (68 pulsos deram 1780 mm e 34 pulsos deram 1212 mm)
 * e isso passou batido. Voltar para a marca de partida na re NAO detecta escala
 * errada, porque o erro e simetrico na ida e na volta. O unico teste que
 * detecta e comandar 1000 mm e medir se andou 1000 mm. */
#define ODO_PULSOS_POR_M       71
#define ODO_PULSOS_90           9

/* Escorregada depois do freio. Comeca em 0 porque o firmware mede a propria
 * escorregada e imprime; chute aqui esconde o efeito. */
#define ODO_ESCORREGO_RETO      0
#define ODO_ESCORREGO_GIRO      0

/* ==================== velocidades ==================== */
#define ODO_VEL_RETO           100
#define ODO_VEL_GIRO           100
#define ODO_VEL_MIN             40

/* Correcao de simetria, em % de velocidade por pulso de erro. Nasce em 0:
 * corrigir e sempre TIRAR velocidade, e com power bank de 5 V o motor ja esta
 * em ~3,2 V no duty maximo, entao baixar mais para a roda e a manobra aborta
 * com ODO_TRAVOU. Com bateria de 7,4 a 9 V, 2 a 4 funciona. Com power bank, a
 * correcao certa e TRIM_ESQ/TRIM_DIR em motores.c. */
#define ODO_KP                    0

/* Protecoes. O que protege de verdade e ODO_PARADO_MS: roda que para de contar
 * (bateu, atolou, encoder solto) aborta sozinha. O valor sobe junto com o
 * tamanho da marca: com 8 marcas cada pulso vale 12,8 mm, e o intervalo entre
 * dois pulsos na arrancada passa folgado de 800 ms, o que abortava manobra boa. */
#define ODO_TIMEOUT_MS        15000
#define ODO_PARADO_MS          1500
#define ODO_ASSENTA_MS          300
#define ODO_PASSO_MS              2

/* ==================== resultado ==================== */

typedef enum {
	ODO_OK = 0,
	ODO_TIMEOUT,
	ODO_TRAVOU,
	ODO_PARAMETRO,
} odo_res_t;

typedef struct {
	odo_res_t res;
	uint32_t  alvo_esq, alvo_dir;
	uint32_t  freio_esq, freio_dir;   /* contagem no instante do freio */
	uint32_t  esq, dir;               /* contagem depois de assentar */
	uint32_t  ms;
} odo_relato_t;

/* ==================== interface ==================== */

uint32_t odo_pulsos_de_mm(uint32_t mm);
uint32_t odo_mm_de_pulsos(uint32_t pulsos);

void odo_init(void);
void odo_para(void);

/* mm > 0 anda para frente, mm < 0 de re. */
void odo_anda_mm(int32_t mm, int vel, odo_relato_t *r);

/* Contagem crua: e o modo de CALIBRAR distancia e angulo. */
void odo_anda_pulsos(uint32_t pulsos, int vel, int para_tras, odo_relato_t *r);
void odo_gira_pulsos(uint32_t pulsos, int vel, int para_direita, odo_relato_t *r);

/* Giro no proprio eixo. graus > 0 para a direita. */
void odo_gira_graus(int graus, int vel, odo_relato_t *r);

/* Arco de raio definido, medido no centro do carrinho. O raio tem de ser maior
 * que meio entre-rodas, senao a roda de dentro teria de girar para tras. */
void odo_curva_graus(int graus, uint32_t raio_mm, int vel_externa, odo_relato_t *r);

void odo_imprime(const char *o_que, const odo_relato_t *r);

#endif /* ODOMETRIA_H_ */
