#include <stdio.h>
#include <stdint.h> // Disponibiliza tipos inteiros com larguras bem definidas
#include <math.h>

#define ESCALA 65536.0

/* Converte double para Q16.16 */
int32_t para_fixo(double valor)
{
    return (int32_t)round(valor * ESCALA);
}

/* Converte Q16.16 para double */
double para_double(int32_t valor)
{
    return (double)valor / ESCALA;
}

/* Multiplica dois valores Q16.16 */
int32_t multiplicar_fixo(int32_t a, int32_t b)
{
    int64_t produto = (int64_t)a * b; // Armazena em 64 bits para evitar overflow
    return (int32_t)(produto / 65536); 
    /* Divide por 65536 porque ao multiplicar a e b, acabamos multiplicando
    também dois fatores de escala*/
}

/* Filtro complementar em ponto fixo */
int32_t filtro_complementar_fixo(
    int32_t angulo_anterior,
    int32_t gyro,
    int32_t angulo_acc,
    int32_t dt,
    int32_t k)
{
    int32_t pred = angulo_anterior
                 + multiplicar_fixo(gyro, dt);

    int32_t erro = angulo_acc - pred;

    int32_t correcao = multiplicar_fixo(k, erro);

    return pred + correcao;
}

int main(void)
{
    int32_t angulo_anterior = para_fixo(0.0);
    int32_t gyro = para_fixo(10.0);
    int32_t acc = para_fixo(1.0);
    int32_t dt = para_fixo(0.01);
    int32_t k = para_fixo(0.02);

    int32_t resultado = filtro_complementar_fixo(
        angulo_anterior, gyro, acc, dt, k
    );

    printf("Angulo estimado: %.6f graus\n",
           para_double(resultado));

    return 0;
}
