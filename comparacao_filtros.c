#include <stdio.h> // Para utilizar o printf
#include <stdint.h> // Disponibiliza tipos inteiros com larguras bem definidas
#include <math.h>  // Biblioteca de funções matemáticas

#define N 1000   // Número de amostras da simulação
#define DT 0.01  // Intervalo de tempo entre duas amostras consecutivas
#define K 0.02   // Coeficiente de correção do filtro
#define PI 3.14159265358979323846
#define A 30     // Amplitude do movimento
#define EG 1.5   // Erro sistemático do giroscópio
#define ESCALA 65536.0

/* Uma atualização do filtro complementar
Recebe a estimativa do ângulo na amostra anterior, a velocidade angular
medida pelo giroscópio e o ângulo estimado a partir do acelerômetro

O tipo double permite representar números com parte fracionária utilizando
ponto flutuante */
double filtro_complementar(double angulo_anterior,
                          double gyro,
                          double angulo_acc)
{
    double pred = angulo_anterior + gyro * DT;

    /* Calcula a diferença entre o ângulo indicado pelo acelerômetro e o
    ângulo previsto, e aplica a correção por K. Esse é o ângulo final.*/
    return pred + K * (angulo_acc - pred);
}

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
    int64_t produto = (int64_t)a * b; // Armazena em 64 bits para evitar overflow do produto
    return (int32_t)(produto / 65536);
    /* Divide por 65536 porque ao multiplicar a e b, acabamos multiplicando
    também dois fatores de escala */
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
    double estimado = 0.0; // Armazena o ângulo estimado pelo filtro double
    int32_t estimado_fixo = para_fixo(0.0); // Armazena o ângulo estimado em Q16.16

    double soma_erro2 = 0.0; // Soma dos erros ao quadrado do filtro double
    double soma_erro2_fixo = 0.0; // Soma dos erros ao quadrado do filtro Q16.16
    double soma_diferenca2 = 0.0; // Soma dos quadrados das diferenças entre filtros
    double maior_diferenca = 0.0;

    /* DT e K são constantes: convertemos apenas uma vez */
    int32_t dt_fixo = para_fixo(DT);
    int32_t k_fixo = para_fixo(K);

    printf("tempo(s),real(graus),gyro(graus/s),acc(graus),estimado_double(graus),estimado_fixo(graus),diferenca(graus)\n");

    // Cada iteração representa uma nova amostra dos sensores
    for (int i = 0; i < N; i++) {

        double t = i * DT;

        /* Movimento verdadeiro do veículo
        A representação da inclinação do veículo está sendo feita por uma
        função senoidal */
        double real = A * sin(2.0 * PI * 0.2 * t);
        /* A amplitude desse movimento é de 30°, com frequência de 0,2 Hz */

        /* Representa a velocidade angular que um giroscópio ideal mediria */
        double gyro_ideal =
            A * 2.0 * PI * 0.2 *
            cos(2.0 * PI * 0.2 * t);

        /* Sensores com erros simulados */
        double gyro = gyro_ideal + EG
            + 0.5 * sin(2.0 * PI * 7.0 * t);

        double acc = real
            + 3.0 * sin(2.0 * PI * 5.3 * t);

        /* Filtro complementar com ponto flutuante */
        estimado = filtro_complementar(
            estimado, gyro, acc
        );

        /* Converte as mesmas medições para Q16.16 */
        int32_t gyro_fixo = para_fixo(gyro);
        int32_t acc_fixo = para_fixo(acc);

        /* Filtro complementar em ponto fixo */
        estimado_fixo = filtro_complementar_fixo(
            estimado_fixo, gyro_fixo, acc_fixo, dt_fixo, k_fixo
        );

        /* Converte o resultado Q16.16 para graus */
        double estimado_fixo_graus = para_double(estimado_fixo);

        /* Cálculo dos erros em relação ao movimento verdadeiro */
        double erro = estimado - real;
        soma_erro2 += erro * erro;

        double erro_fixo = estimado_fixo_graus - real;
        soma_erro2_fixo += erro_fixo * erro_fixo;

        /* Diferença numérica entre as duas implementações */
        double diferenca = estimado_fixo_graus - estimado;
        soma_diferenca2 += diferenca * diferenca;
        if (fabs(diferenca) > maior_diferenca) {
            maior_diferenca = fabs(diferenca);
        }
        
        if (i % 100 == 0) {
            printf("%.2f,%.4f,%.4f,%.4f,%.6f,%.6f,%.6f\n",
                    t, real, gyro, acc,
                    estimado, estimado_fixo_graus,
                    diferenca);

        /* Valores inteiros utilizados pelo filtro Q16.16 */
        printf("Q16.16: gyro=%ld | acc=%ld | dt=%ld | k=%ld | saida=%ld\n\n",
            (long)gyro_fixo,
            (long)acc_fixo,
            (long)dt_fixo,
            (long)k_fixo,
            (long)estimado_fixo);
    }

    }

      printf("\nErro RMS do filtro double (graus): %.6f\n",
           sqrt(soma_erro2 / N));
    printf("Erro RMS do filtro Q16.16 (graus): %.6f\n",
           sqrt(soma_erro2_fixo / N));
    printf("RMS da diferenca entre filtros (graus): %.6f\n",
           sqrt(soma_diferenca2 / N));
    printf("Maior diferenca absoluta entre filtros (graus): %.6f\n",
           maior_diferenca);

    return 0;
}
