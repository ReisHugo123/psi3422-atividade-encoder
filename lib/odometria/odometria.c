/*
 * odometria.c - uma malha de controle so, tres geometrias.
 *
 * Reta, giro no eixo e arco diferem em duas coisas: o sentido de cada roda e
 * quantos pulsos cada roda tem de dar. O resto e identico, e por isso e um
 * laco unico (executa()).
 *
 * Aritmetica toda inteira, inclusive o arco: ponto flutuante no Cortex-M0+ e
 * emulado em software. Ver docs/decisoes.md, secao 8.
 */
#include "odometria.h"
#include "motores.h"

#include <zephyr/kernel.h>

#define PI_x1e4   31416u

struct plano {
	int      vel_esq, vel_dir;
	uint32_t alvo_esq, alvo_dir;
	uint32_t margem;
};

uint32_t odo_pulsos_de_mm(uint32_t mm)
{
	return (mm * (uint32_t)ODO_PULSOS_POR_M + 500u) / 1000u;
}

uint32_t odo_mm_de_pulsos(uint32_t pulsos)
{
	return (pulsos * 1000u + (uint32_t)ODO_PULSOS_POR_M / 2u)
	       / (uint32_t)ODO_PULSOS_POR_M;
}

void odo_init(void)
{
	motores_init();
	encoder_init();
	carrinho_para();
}

void odo_para(void)
{
	carrinho_freia();
	k_msleep(ODO_ASSENTA_MS);
	carrinho_para();
}

static int limita(int v)
{
	if (v > VEL_MAX) {
		return VEL_MAX;
	}
	if (v < ODO_VEL_MIN) {
		return ODO_VEL_MIN;
	}
	return v;
}

static void executa(const struct plano *p, odo_relato_t *r)
{
	uint32_t alvo_esq = (p->alvo_esq < 1u) ? 1u : p->alvo_esq;
	uint32_t alvo_dir = (p->alvo_dir < 1u) ? 1u : p->alvo_dir;

	r->res      = ODO_OK;
	r->alvo_esq = alvo_esq;
	r->alvo_dir = alvo_dir;

	/* freia antes do alvo pela escorregada conhecida, para terminar no ponto */
	uint32_t ef_esq = (alvo_esq > p->margem) ? (alvo_esq - p->margem) : 1u;
	uint32_t ef_dir = (alvo_dir > p->margem) ? (alvo_dir - p->margem) : 1u;
	uint32_t meta   = 2u * ef_esq * ef_dir;

	int sinal_esq = (p->vel_esq >= 0) ? 1 : -1;
	int sinal_dir = (p->vel_dir >= 0) ? 1 : -1;
	int base_esq  = (p->vel_esq >= 0) ? p->vel_esq : -p->vel_esq;
	int base_dir  = (p->vel_dir >= 0) ? p->vel_dir : -p->vel_dir;

	uint32_t t0 = k_uptime_get_32();

	encoder_zera();
	motor_set(MOTOR_ESQ, sinal_esq * base_esq);
	motor_set(MOTOR_DIR, sinal_dir * base_dir);

	uint32_t esq = 0u, dir = 0u;

	while (1) {
		esq = encoder_conta(ENC_ESQ);
		dir = encoder_conta(ENC_DIR);

		/* media dos progressos fracionarios chegou a 1, sem divisao:
		 * esq/alvo_esq + dir/alvo_dir >= 2 */
		if ((esq * ef_dir + dir * ef_esq) >= meta) {
			break;
		}

		if ((k_uptime_get_32() - t0) > (uint32_t)ODO_TIMEOUT_MS) {
			r->res = ODO_TIMEOUT;
			break;
		}

		/* roda sem pulso e falha de verdade: encoder solto, fio caido, roda
		 * atolada, carrinho na parede. Tem de parar e aparecer. */
		if (encoder_us_parado(ENC_ESQ) > (uint32_t)ODO_PARADO_MS * 1000u ||
		    encoder_us_parado(ENC_DIR) > (uint32_t)ODO_PARADO_MS * 1000u) {
			r->res = ODO_TRAVOU;
			break;
		}

		/* quanto a esquerda deveria ter contado, dado o que a direita contou */
		int32_t esperado = (int32_t)((dir * alvo_esq) / alvo_dir);
		int32_t ajuste   = ((int32_t)esq - esperado) * ODO_KP;

		motor_set(MOTOR_ESQ, sinal_esq * limita(base_esq - (int)ajuste));
		motor_set(MOTOR_DIR, sinal_dir * limita(base_dir + (int)ajuste));

		k_msleep(ODO_PASSO_MS);
	}

	r->freio_esq = esq;
	r->freio_dir = dir;

	odo_para();

	r->esq = encoder_conta(ENC_ESQ);
	r->dir = encoder_conta(ENC_DIR);
	r->ms  = k_uptime_get_32() - t0;
}

void odo_anda_pulsos(uint32_t pulsos, int vel, int para_tras, odo_relato_t *r)
{
	int s = para_tras ? -1 : 1;
	struct plano p = {
		.vel_esq  = s * vel,
		.vel_dir  = s * vel,
		.alvo_esq = pulsos,
		.alvo_dir = pulsos,
		.margem   = ODO_ESCORREGO_RETO,
	};

	executa(&p, r);
}

void odo_anda_mm(int32_t mm, int vel, odo_relato_t *r)
{
	int      tras = (mm < 0);
	uint32_t mod  = (uint32_t)(tras ? -mm : mm);

	odo_anda_pulsos(odo_pulsos_de_mm(mod), vel, tras, r);
}

void odo_gira_pulsos(uint32_t pulsos, int vel, int para_direita, odo_relato_t *r)
{
	int s = para_direita ? 1 : -1;
	struct plano p = {
		.vel_esq  =  s * vel,      /* rodas em sentidos opostos */
		.vel_dir  = -s * vel,
		.alvo_esq = pulsos,
		.alvo_dir = pulsos,
		.margem   = ODO_ESCORREGO_GIRO,
	};

	executa(&p, r);
}

void odo_gira_graus(int graus, int vel, odo_relato_t *r)
{
	int      dir    = (graus >= 0);
	uint32_t mod    = (uint32_t)(dir ? graus : -graus);
	uint32_t pulsos = (mod * (uint32_t)ODO_PULSOS_90 + 45u) / 90u;

	odo_gira_pulsos(pulsos, vel, dir, r);
}

void odo_curva_graus(int graus, uint32_t raio_mm, int vel_externa, odo_relato_t *r)
{
	uint32_t meia_bitola = (uint32_t)ODO_ENTRE_RODAS_MM / 2u;

	/* raio menor que meia bitola e giro, nao curva: recusar em vez de fazer
	 * silenciosamente outra coisa */
	if (raio_mm <= meia_bitola) {
		r->res = ODO_PARAMETRO;
		r->alvo_esq  = r->alvo_dir  = 0u;
		r->freio_esq = r->freio_dir = 0u;
		r->esq = r->dir = r->ms = 0u;
		return;
	}

	int      direita = (graus >= 0);
	uint32_t mod     = (uint32_t)(direita ? graus : -graus);

	/* s = graus * pi/180 * raio, com pi*1e4 para caber em inteiro */
	uint32_t s_ext = (uint32_t)(((uint64_t)mod * PI_x1e4
				     * (raio_mm + meia_bitola)) / 1800000u);
	uint32_t s_int = (uint32_t)(((uint64_t)mod * PI_x1e4
				     * (raio_mm - meia_bitola)) / 1800000u);

	uint32_t alvo_ext = odo_pulsos_de_mm(s_ext);
	uint32_t alvo_int = odo_pulsos_de_mm(s_int);

	/* a razao entre as velocidades e a razao entre os arcos, e e ela que fecha
	 * o raio pedido */
	int vel_int = limita((int)(((uint32_t)vel_externa * s_int)
				   / (s_ext ? s_ext : 1u)));

	struct plano p;

	if (direita) {                 /* a esquerda e a roda de fora */
		p.vel_esq  = vel_externa;
		p.vel_dir  = vel_int;
		p.alvo_esq = alvo_ext;
		p.alvo_dir = alvo_int;
	} else {
		p.vel_esq  = vel_int;
		p.vel_dir  = vel_externa;
		p.alvo_esq = alvo_int;
		p.alvo_dir = alvo_ext;
	}
	p.margem = ODO_ESCORREGO_RETO;

	executa(&p, r);
}

static const char *nome_res(odo_res_t res)
{
	switch (res) {
	case ODO_OK:        return "ok";
	case ODO_TIMEOUT:   return "FALHOU: estourou o tempo sem chegar no alvo";
	case ODO_TRAVOU:    return "FALHOU: roda sem pulso (encoder solto? atolou?)";
	case ODO_PARAMETRO: return "FALHOU: parametro impossivel";
	}
	return "?";
}

void odo_imprime(const char *o_que, const odo_relato_t *r)
{
	uint32_t media_alvo  = (r->alvo_esq  + r->alvo_dir  + 1u) / 2u;
	uint32_t media_freio = (r->freio_esq + r->freio_dir + 1u) / 2u;
	uint32_t media_fim   = (r->esq       + r->dir       + 1u) / 2u;

	printk("[%s] %s\n", o_que, nome_res(r->res));

	if (r->res == ODO_PARAMETRO) {
		return;
	}

	printk("   alvo   esq %u  dir %u   media %u pulsos = %u mm\n",
	       r->alvo_esq, r->alvo_dir, media_alvo, odo_mm_de_pulsos(media_alvo));
	printk("   freio  esq %u  dir %u   media %u\n",
	       r->freio_esq, r->freio_dir, media_freio);
	printk("   fim    esq %u  dir %u   media %u pulsos = %u mm\n",
	       r->esq, r->dir, media_fim, odo_mm_de_pulsos(media_fim));
	printk("   escorregou %d pulsos depois do freio | erro final %d pulsos\n",
	       (int32_t)media_fim - (int32_t)media_freio,
	       (int32_t)media_fim - (int32_t)media_alvo);
	printk("   desvio entre rodas %d pulsos | %u ms | glitches esq %u dir %u\n",
	       (int32_t)r->esq - (int32_t)r->dir, r->ms,
	       encoder_glitches(ENC_ESQ), encoder_glitches(ENC_DIR));
}
