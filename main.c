// Código para ser utilizado no Vitis
// Valida o filtro complementar implementado na FPGA e compara
// os resultados e os tempos de execução entre ARM e FPGA via AXI.

#include <stdint.h>
#include "xil_io.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xiltimer.h"


// Endereco-base do filtro complementar
#define FILTRO_BASE 0x43C00000U


// Deslocamentos dos registradores AXI4-Lite
#define REG_CTRL       0x00U
#define REG_STATUS     0x04U
#define REG_ANGULO_ANT 0x08U
#define REG_GYRO       0x0CU
#define REG_ANGULO_ACC 0x10U
#define REG_DT         0x14U
#define REG_K          0x18U
#define REG_ANGULO_OUT 0x1CU


#define TIMEOUT 1000000U
#define Q16 65536LL
#define DT  655
#define K   1311




// ==================================================
// COMUNICACAO COM A FPGA
// ==================================================


void filtro_escrever(uint32_t registrador, int32_t valor)
{
    Xil_Out32(FILTRO_BASE + registrador, (uint32_t)valor);
}


int32_t filtro_ler(uint32_t registrador)
{
    return (int32_t)Xil_In32(FILTRO_BASE + registrador);
}




// Aguarda done assumir o valor desejado
int aguardar_done(uint32_t valor)
{
    uint32_t timeout = TIMEOUT;


    while ((filtro_ler(REG_STATUS) & 0x01U) != valor) {
        if (timeout-- == 0U) {
            return -1;
        }
    }


    return 0;
}




// ==================================================
// FILTRO IMPLEMENTADO EM HARDWARE (FPGA)
// ==================================================


int filtro_calcular(
    int32_t angulo_ant,
    int32_t gyro,
    int32_t angulo_acc,
    int32_t dt,
    int32_t k,
    int32_t *resultado)
{
    // Garantir que start esteja em zero
    filtro_escrever(REG_CTRL, 0);


    // Confirmar que o filtro esta pronto
    if (aguardar_done(0) != 0) {
        return -1;
    }


    // Enviar as entradas
    filtro_escrever(REG_ANGULO_ANT, angulo_ant);
    filtro_escrever(REG_GYRO, gyro);
    filtro_escrever(REG_ANGULO_ACC, angulo_acc);
    filtro_escrever(REG_DT, dt);
    filtro_escrever(REG_K, k);


    // Iniciar o filtro
    filtro_escrever(REG_CTRL, 1);


    // Aguardar a conclusao do calculo
    if (aguardar_done(1) != 0) {
        filtro_escrever(REG_CTRL, 0);
        return -2;
    }


    // Ler o resultado
    *resultado = filtro_ler(REG_ANGULO_OUT);


    // Desativar o comando
    filtro_escrever(REG_CTRL, 0);


    // Confirmar que o filtro retornou ao estado inicial
    if (aguardar_done(0) != 0) {
        return -3;
    }


    return 0;
}




// ==================================================
// FILTRO DE REFERENCIA EM SOFTWARE (ARM)
// ==================================================


int32_t filtro_referencia(
    int32_t angulo_ant,
    int32_t gyro,
    int32_t angulo_acc,
    int32_t dt,
    int32_t k)
{
    int32_t pred;
    int32_t erro;
    int32_t resultado;


    pred = angulo_ant +
           (int32_t)(((int64_t)gyro * dt) / Q16);


    erro = angulo_acc - pred;


    resultado = pred +
                (int32_t)(((int64_t)k * erro) / Q16);


    return resultado;
}

// ==================================================
// AMOSTRAS SIMULADAS DO SENSOR
// ==================================================

typedef struct {
    int32_t gyro;
    int32_t angulo_acc;
} Amostra;

const Amostra amostras[] = {
    { 655360,  65536 },
    { 655360, 131072 },
    { 327680, 196608 },
    { 327680, 262144 },
    {      0, 262144 },
    {-327680, 196608 },
    {-327680, 131072 },
    {-655360,  65536 },
    {-655360,      0 },
    {      0,      0 }
};

#define NUM_AMOSTRAS (sizeof(amostras) / sizeof(amostras[0]))
#define NUM_ITERACOES 10000U


// ==================================================
// TESTE RECURSIVO
// ==================================================

int teste_recursivo(void)
{
    int32_t angulo_arm = 0;
    int32_t angulo_fpga = 0;
    int32_t resultado_fpga;

    int erro;
    int aprovados = 0;

    xil_printf("\r\n====================================\r\n");
    xil_printf(" TESTE RECURSIVO\r\n");
    xil_printf("====================================\r\n");

    for (unsigned int i = 0; i < NUM_AMOSTRAS; i++) {

        // Filtro em software
        angulo_arm = filtro_referencia(
            angulo_arm,
            amostras[i].gyro,
            amostras[i].angulo_acc,
            DT,
            K
        );

        // Filtro em hardware
        erro = filtro_calcular(
            angulo_fpga,
            amostras[i].gyro,
            amostras[i].angulo_acc,
            DT,
            K,
            &resultado_fpga
        );

        if (erro != 0) {
            xil_printf("Timeout: %d\r\n", erro);
            return -1;
        }

        // Realimentacao do resultado
        angulo_fpga = resultado_fpga;

        xil_printf("\r\nAmostra %d:\r\n", (int)(i + 1));
        xil_printf("ARM:  %d\r\n", (int)angulo_arm);
        xil_printf("FPGA: %d\r\n", (int)angulo_fpga);

        if (angulo_arm == angulo_fpga) {
            xil_printf("TESTE PASSOU!\r\n");
            aprovados++;
        } else {
            xil_printf("TESTE FALHOU!\r\n");
        }
    }

    xil_printf("\r\nAprovados: %d de %d\r\n",
               aprovados, (int)NUM_AMOSTRAS);

    return (aprovados == NUM_AMOSTRAS) ? 0 : -1;
}


// ==================================================
// TESTE DE DESEMPENHO
// ==================================================

void teste_desempenho(void)
{
    XTime inicio;
    XTime fim;

    uint64_t ciclos_arm;
    uint64_t ciclos_fpga;

    uint64_t tempo_arm_us;
    uint64_t tempo_fpga_us;

    uint64_t media_arm_ns;
    uint64_t media_fpga_ns;

    volatile int32_t angulo_arm = 0;
    int32_t angulo_fpga = 0;
    int32_t resultado_fpga;

    unsigned int indice;
    int erro;

    xil_printf("\r\n====================================\r\n");
    xil_printf(" TESTE DE DESEMPENHO\r\n");
    xil_printf("====================================\r\n");

    // ----------------------------------------------
    // Medicao do ARM
    // ----------------------------------------------

    XTime_GetTime(&inicio);
    // Habilitar o Global Timer do Cortex-A9
    Xil_Out32(0xF8F00208U,
            Xil_In32(0xF8F00208U) | 1U);

    for (unsigned int i = 0; i < NUM_ITERACOES; i++) {

        indice = i % NUM_AMOSTRAS;

        angulo_arm = filtro_referencia(
            angulo_arm,
            amostras[indice].gyro,
            amostras[indice].angulo_acc,
            DT,
            K
        );
    }

    XTime_GetTime(&fim);

    ciclos_arm = fim - inicio;

    // ----------------------------------------------
    // Medicao da FPGA
    // ----------------------------------------------

    XTime_GetTime(&inicio);

    for (unsigned int i = 0; i < NUM_ITERACOES; i++) {

        indice = i % NUM_AMOSTRAS;

        erro = filtro_calcular(
            angulo_fpga,
            amostras[indice].gyro,
            amostras[indice].angulo_acc,
            DT,
            K,
            &resultado_fpga
        );

        if (erro != 0) {
            xil_printf("Timeout: %d\r\n", erro);
            return;
        }

        angulo_fpga = resultado_fpga;
    }

    XTime_GetTime(&fim);

    ciclos_fpga = fim - inicio;

    // ----------------------------------------------
    // Conversao dos tempos
    // ----------------------------------------------

    // Frequencia do temporizador
    uint64_t frequencia_timer = (uint64_t)COUNTS_PER_SECOND;

    // Tempo total em nanossegundos
    uint64_t total_arm_ns =
        (ciclos_arm * 1000000000ULL) / frequencia_timer;

    uint64_t total_fpga_ns =
        (ciclos_fpga * 1000000000ULL) / frequencia_timer;

    // Tempo total em microssegundos
    tempo_arm_us = total_arm_ns / 1000ULL;
    tempo_fpga_us = total_fpga_ns / 1000ULL;

    // Tempo medio por iteracao em nanossegundos
    media_arm_ns = total_arm_ns / NUM_ITERACOES;
    media_fpga_ns = total_fpga_ns / NUM_ITERACOES;

    xil_printf("\r\nFrequencia do timer: %u Hz\r\n",
           (unsigned int)frequencia_timer);

    xil_printf("Contagens ARM: %u\r\n",
            (unsigned int)ciclos_arm);

    xil_printf("Contagens FPGA: %u\r\n",
            (unsigned int)ciclos_fpga);

    // ----------------------------------------------
    // Resultados
    // ----------------------------------------------

    xil_printf("\r\nIteracoes: %d\r\n",
               (int)NUM_ITERACOES);

    xil_printf("\r\nARM:\r\n");
    xil_printf("Tempo total: %u us\r\n",
               (unsigned int)tempo_arm_us);
    xil_printf("Tempo medio: %u ns\r\n",
               (unsigned int)media_arm_ns);

    xil_printf("\r\nFPGA + AXI:\r\n");
    xil_printf("Tempo total: %u us\r\n",
               (unsigned int)tempo_fpga_us);
    xil_printf("Tempo medio: %u ns\r\n",
               (unsigned int)media_fpga_ns);

    xil_printf("\r\nAngulo final ARM:  %d\r\n",
               (int)angulo_arm);

    xil_printf("Angulo final FPGA: %d\r\n",
               (int)angulo_fpga);

    if (angulo_arm == angulo_fpga) {
        xil_printf("RESULTADOS IGUAIS!\r\n");
    } else {
        xil_printf("RESULTADOS DIFERENTES!\r\n");
    }
}


// ==================================================
// PROGRAMA PRINCIPAL
// ==================================================

int main(void)
{
    xil_printf("\r\n====================================\r\n");
    xil_printf(" FILTRO COMPLEMENTAR - CORA Z7\r\n");
    xil_printf("====================================\r\n");

    if (teste_recursivo() == 0) {
        teste_desempenho();
    } else {
        xil_printf("\r\nFalha no teste recursivo.\r\n");
    }

    xil_printf("\r\nFim do programa.\r\n");

    while (1) {
        // Mantem o programa em execucao
    }

    return 0;
}
