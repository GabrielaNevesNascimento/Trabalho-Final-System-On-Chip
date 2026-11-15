#include <stdio.h> // Para utilizar o printf
#include <math.h>  // Biblioteca de funções matemáticas

#define N 1000   // Número de amostras da simulação
#define DT 0.01  // Intervalo de tempo entre duas amostras consecutivas
#define K 0.02   // Coeficiente de correção do filtro
#define PI 3.14159265358979323846
#define A 30     // Amplitude do movimento
#define EG 1.5        // Erro sistemático do giroscópio

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

int main(void)
{
    double estimado = 0.0; // Armazena o ângulo estimado pelo filtro
    double soma_erro2 = 0.0; // Armazena a soma dos erros ao quadrado

    printf("tempo(s),real(graus),gyro(graus/s),acc(graus),estimado(graus)\n");

    // Cada iteração representa uma nova amostra dos sensores
    for (int i = 0; i < N; i++) {

        double t = i * DT;

        /* Movimento verdadeiro do veículo 
        A representação da inclinação do veículo está sendo feita por uma 
        função senoidal*/
        double real = A * sin(2.0 * PI * 0.2 * t);
        /* A amplitude desse movimento é de 30°, com frequência de 0,2 Hz*/

        /* Representa a velocidade angular que um giroscópio ideal mediria*/
        double gyro_ideal =
            A * 2.0 * PI * 0.2 *
            cos(2.0 * PI * 0.2 * t);

        /* Sensores com erros simulados */
        double gyro = gyro_ideal + EG
            + 0.5 * sin(2.0 * PI * 7.0 * t);

        double acc = real
            + 3.0 * sin(2.0 * PI * 5.3 * t);

        /* Filtro complementar */
        estimado = filtro_complementar(
            estimado, gyro, acc
        );

        /* Cálculo do erro */
        double erro = estimado - real;
        soma_erro2 += erro * erro;

        if (i % 100 == 0) {
            printf("%.2f,%.4f,%.4f,%.4f,%.4f\n",
                   t, real, gyro, acc, estimado);
        }
    }

    printf("Erro RMS (graus): %.6f\n",
           sqrt(soma_erro2 / N));

    return 0;
}
