/**
 * Programa de Calibração para TCS34725
 * Use este programa para ver os valores brutos e calibrar o sensor
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// I2C0 para TCS34725
#define I2C0_PORT i2c0
#define I2C0_SDA 0
#define I2C0_SCL 1
#define I2C0_FREQ 400000

// TCS34725
#define TCS34725_ADDR 0x29
#define TCS34725_COMMAND_BIT 0x80
#define TCS34725_ENABLE 0x00
#define TCS34725_ATIME 0x01
#define TCS34725_CONTROL 0x0F
#define TCS34725_ID 0x12
#define TCS34725_CDATAL 0x14
#define TCS34725_RDATAL 0x16
#define TCS34725_GDATAL 0x18
#define TCS34725_BDATAL 0x1A
#define TCS34725_ENABLE_PON 0x01
#define TCS34725_ENABLE_AEN 0x02
#define TCS34725_GAIN_1X 0x00
#define TCS34725_GAIN_4X 0x01
#define TCS34725_GAIN_16X 0x02
#define TCS34725_GAIN_60X 0x03

void tcs34725_write_byte(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {TCS34725_COMMAND_BIT | reg, value};
    i2c_write_blocking(I2C0_PORT, TCS34725_ADDR, buf, 2, false);
}

uint8_t tcs34725_read_byte(uint8_t reg) {
    uint8_t value;
    uint8_t cmd = TCS34725_COMMAND_BIT | reg;
    i2c_write_blocking(I2C0_PORT, TCS34725_ADDR, &cmd, 1, true);
    i2c_read_blocking(I2C0_PORT, TCS34725_ADDR, &value, 1, false);
    return value;
}

uint16_t tcs34725_read_word(uint8_t reg) {
    uint8_t buf[2];
    uint8_t cmd = TCS34725_COMMAND_BIT | reg;
    i2c_write_blocking(I2C0_PORT, TCS34725_ADDR, &cmd, 1, true);
    i2c_read_blocking(I2C0_PORT, TCS34725_ADDR, buf, 2, false);
    return (buf[1] << 8) | buf[0];
}

bool tcs34725_init(void) {
    sleep_ms(100);
    uint8_t id = tcs34725_read_byte(TCS34725_ID);
    printf("Sensor ID: 0x%02X ", id);
    
    if (id != 0x44 && id != 0x4D && id != 0x10) {
        printf("(ID inesperado)\n");
    } else {
        printf("OK\n");
    }
    
    tcs34725_write_byte(TCS34725_ENABLE, TCS34725_ENABLE_PON);
    sleep_ms(3);
    tcs34725_write_byte(TCS34725_ENABLE, TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN);
    sleep_ms(3);
    tcs34725_write_byte(TCS34725_ATIME, 0xF6);  // 24ms
    tcs34725_write_byte(TCS34725_CONTROL, TCS34725_GAIN_16X);  // Ganho 16x
    
    return true;
}

void tcs34725_read_colors(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c) {
    *c = tcs34725_read_word(TCS34725_CDATAL);
    *r = tcs34725_read_word(TCS34725_RDATAL);
    *g = tcs34725_read_word(TCS34725_GDATAL);
    *b = tcs34725_read_word(TCS34725_BDATAL);
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    
    printf("\n=== TCS34725 - PROGRAMA DE CALIBRACAO ===\n\n");
    
    // Configurar I2C
    i2c_init(I2C0_PORT, I2C0_FREQ);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);
    
    if (!tcs34725_init()) {
        printf("ERRO ao inicializar sensor!\n");
        return 1;
    }
    
    printf("\nConfiguracao:\n");
    printf("  - Ganho: 16x\n");
    printf("  - Tempo de integracao: 24ms\n");
    printf("\nAproxime objetos coloridos do sensor...\n\n");
    
    sleep_ms(1000);
    
    uint16_t r, g, b, c;
    
    while (true) {
        tcs34725_read_colors(&r, &g, &b, &c);
        
        // Calcular valores normalizados
        float r_norm = (c > 0) ? ((float)r / (float)c) : 0;
        float g_norm = (c > 0) ? ((float)g / (float)c) : 0;
        float b_norm = (c > 0) ? ((float)b / (float)c) : 0;
        
        // Calcular diferença (saturação)
        float max_norm = r_norm;
        if (g_norm > max_norm) max_norm = g_norm;
        if (b_norm > max_norm) max_norm = b_norm;
        
        float min_norm = r_norm;
        if (g_norm < min_norm) min_norm = g_norm;
        if (b_norm < min_norm) min_norm = b_norm;
        
        float diff = max_norm - min_norm;
        
        // Determinar componente dominante
        const char* dominant = "NENHUM";
        if (r_norm > g_norm && r_norm > b_norm) dominant = "VERMELHO";
        else if (g_norm > r_norm && g_norm > b_norm) dominant = "VERDE";
        else if (b_norm > r_norm && b_norm > g_norm) dominant = "AZUL";
        
        // Exibir resultados
        printf("+---------------------------------------------------------------+\n");
        printf("| VALORES BRUTOS:                                               |\n");
        printf("|   R = %5u    G = %5u    B = %5u    C = %5u      |\n", r, g, b, c);
        printf("+---------------------------------------------------------------+\n");
        printf("| VALORES NORMALIZADOS (dividido por C):                       |\n");
        printf("|   R_norm = %.3f    G_norm = %.3f    B_norm = %.3f         |\n", 
               r_norm, g_norm, b_norm);
        printf("+---------------------------------------------------------------+\n");
        printf("| ANALISE:                                                      |\n");
        printf("|   Componente Dominante: %-15s                     |\n", dominant);
        printf("|   Saturacao (diff): %.3f                                    |\n", diff);
        
        if (c < 50) {
            printf("|   Status: MUITO ESCURO (aumente ganho ou iluminacao)         |\n");
        } else if (c > 50000) {
            printf("|   Status: SATURADO (diminua ganho ou iluminacao)             |\n");
        } else {
            printf("|   Status: OK                                                  |\n");
        }
        
        if (diff < 0.08) {
            printf("|   Tipo: BRANCO/CINZA (baixa saturacao)                       |\n");
        } else {
            printf("|   Tipo: COR DEFINIDA (alta saturacao)                        |\n");
        }
        
        printf("+---------------------------------------------------------------+\n\n");
        
        sleep_ms(500);
    }
    
    return 0;
}
