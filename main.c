/**
 * Sistema Integrado: Sensor de Cor TCS34725 + Sensor de Distância VL53L0X
 * BitDogLab
 * 
 * TCS34725 - Sensor de Cor no I2C0 (GP0/GP1)
 * VL53L0X - Sensor de Distância no I2C1 (GP2/GP3)
 * LED RGB - Controlado pela distância (< 15cm = Vermelho, >= 15cm = Verde)
 */

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "ssd1306.h"
#include "font.h"

// ==================== DEFINIÇÕES I2C ====================
// I2C0 para TCS34725 (Sensor de Cor)
#define I2C0_PORT i2c0
#define I2C0_SDA 0
#define I2C0_SCL 1
#define I2C0_FREQ 400000

// I2C1 para VL53L0X (Sensor de Distância)
#define I2C1_PORT i2c1
#define I2C1_SDA 2
#define I2C1_SCL 3
#define I2C1_FREQ 100000

// ==================== LEDs ====================
// LED RGB (ajuste os pinos conforme seu hardware)
#define LED_RED_PIN 13
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12

// ==================== TCS34725 - SENSOR DE COR ====================
#define TCS34725_ADDR 0x29

// Registradores do TCS34725
#define TCS34725_COMMAND_BIT 0x80
#define TCS34725_ENABLE 0x00
#define TCS34725_ATIME 0x01
#define TCS34725_CONTROL 0x0F
#define TCS34725_ID 0x12
#define TCS34725_STATUS 0x13
#define TCS34725_CDATAL 0x14
#define TCS34725_CDATAH 0x15
#define TCS34725_RDATAL 0x16
#define TCS34725_RDATAH 0x17
#define TCS34725_GDATAL 0x18
#define TCS34725_GDATAH 0x19
#define TCS34725_BDATAL 0x1A
#define TCS34725_BDATAH 0x1B

// Bits de controle
#define TCS34725_ENABLE_PON 0x01
#define TCS34725_ENABLE_AEN 0x02

// Ganho
#define TCS34725_GAIN_1X 0x00
#define TCS34725_GAIN_4X 0x01
#define TCS34725_GAIN_16X 0x02
#define TCS34725_GAIN_60X 0x03

// ==================== VL53L0X - SENSOR DE DISTÂNCIA ====================
#define VL53L0X_ADDR 0x29
#define VL53L0X_REG_IDENTIFICATION_MODEL_ID 0xC0
#define VL53L0X_REG_SYSRANGE_START 0x00
#define VL53L0X_REG_RESULT_INTERRUPT_STATUS 0x13
#define VL53L0X_REG_RESULT_RANGE_STATUS 0x14

// ==================== FUNÇÕES TCS34725 ====================
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
    printf("TCS34725 - ID: 0x%02X ", id);
    
    if (id != 0x44 && id != 0x4D && id != 0x10) {
        printf("(Aviso: ID inesperado, tentando continuar...)\n");
    } else {
        printf("OK\n");
    }
    
    tcs34725_write_byte(TCS34725_ENABLE, TCS34725_ENABLE_PON);
    sleep_ms(3);
    
    tcs34725_write_byte(TCS34725_ENABLE, TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN);
    sleep_ms(3);
    
    // ATIME = 0x00 (máximo tempo de integração: 700ms, melhor precisão)
    // Ou use 0xF6 para ~24ms (mais rápido)
    tcs34725_write_byte(TCS34725_ATIME, 0xF6);  // 24ms de integração
    
    // GAIN = 60x para máxima sensibilidade (melhor para cores)
    tcs34725_write_byte(TCS34725_CONTROL, TCS34725_GAIN_60X);
    
    return true;
}

void tcs34725_read_colors(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c) {
    *c = tcs34725_read_word(TCS34725_CDATAL);
    *r = tcs34725_read_word(TCS34725_RDATAL);
    *g = tcs34725_read_word(TCS34725_GDATAL);
    *b = tcs34725_read_word(TCS34725_BDATAL);
}

const char* get_color_name(uint16_t r, uint16_t g, uint16_t b, uint16_t c) {
    // Verificar se há luz suficiente
    if (c < 50) {
        return "MUITO ESCURO";
    }
    
    // Verificar saturação (evita leituras inválidas com ganho alto)
    if (c > 60000) {
        return "SATURADO (diminua iluminacao)";
    }
    
    // Normalizar valores RGB pelo Clear (compensar iluminação)
    float r_norm = (c > 0) ? ((float)r / (float)c) : 0;
    float g_norm = (c > 0) ? ((float)g / (float)c) : 0;
    float b_norm = (c > 0) ? ((float)b / (float)c) : 0;
    
    // Calcular saturação (diferença entre maior e menor componente)
    float max_norm = r_norm;
    if (g_norm > max_norm) max_norm = g_norm;
    if (b_norm > max_norm) max_norm = b_norm;
    
    float min_norm = r_norm;
    if (g_norm < min_norm) min_norm = g_norm;
    if (b_norm < min_norm) min_norm = b_norm;
    
    float diff = max_norm - min_norm;
    
    // DEBUG: Imprimir valores normalizados
    printf("   [Debug] R:%.3f G:%.3f B:%.3f C:%u Dif:%.3f\n", r_norm, g_norm, b_norm, c, diff);
    
    // Detectar cores ANTES de branco/cinza (prioridade para cores definidas)
    
    // Vermelho/Rosa: R é dominante
    if (r_norm > g_norm && r_norm > b_norm && diff > 0.02) {
        // Rosa: R dominante mas com saturação baixa ou valores altos de G e B
        if (diff < 0.08 && c > 10000) {
            return "ROSA";
        }
        if (r_norm > g_norm * 1.05) {
            if (g_norm > b_norm * 1.3) {
                return "LARANJA";
            } else if (b_norm > g_norm * 1.2) {
                return "MAGENTA";
            } else if (g_norm > b_norm * 1.05 && (g_norm / r_norm) > 0.75) {
                return "ROSA";
            } else {
                return "VERMELHO";
            }
        }
    }
    
    // Verde/Amarelo: G é dominante ou similar a R
    if ((g_norm >= r_norm || (r_norm > g_norm * 0.85 && r_norm < g_norm * 1.15)) && diff > 0.02) {
        // Amarelo: R e G altos, B baixo
        if (r_norm > b_norm * 1.3 && g_norm > b_norm * 1.3) {
            // Amarelo tem R e G similares
            if (r_norm > g_norm * 0.85 && r_norm < g_norm * 1.15) {
                return "AMARELO";
            }
        }
    }
    
    // Verde: G dominante sobre R e B
    if (g_norm > r_norm && g_norm > b_norm && diff > 0.02) {
        if (g_norm > r_norm * 1.05 && g_norm > b_norm * 1.05) {
            if (b_norm > r_norm * 1.3) {
                return "CIANO";
            } else {
                return "VERDE";
            }
        }
    }
    
    // Azul/Ciano: B é dominante
    if (b_norm > r_norm && b_norm > g_norm && diff > 0.02) {
        if (b_norm > r_norm * 1.05 && b_norm > g_norm * 1.05) {
            if (r_norm > g_norm * 1.3) {
                return "MAGENTA";
            } else if (g_norm > r_norm * 1.3) {
                return "CIANO";
            } else {
                return "AZUL";
            }
        }
    }
    
    // Se chegou aqui, verificar branco/cinza (baixa saturação)
    // Branco: todos os canais muito altos e próximos
    if (diff < 0.015 && r_norm > 0.28 && g_norm > 0.28 && b_norm > 0.28 && c > 42000) {
        return "BRANCO";
    }
    // Cinza: todos os canais médios e praticamente idênticos
    if (diff < 0.002 && fabs(r_norm - g_norm) < 0.002 && fabs(r_norm - b_norm) < 0.002 && fabs(g_norm - b_norm) < 0.002 && r_norm > 0.12 && r_norm < 0.28 && c > 2000 && c < 42000) {
        return "CINZA";
    }
    // Se não for cinza real, considerar vermelho escuro se R for maior que B e próximo de G
    if (!(diff < 0.002 && fabs(r_norm - g_norm) < 0.002 && fabs(r_norm - b_norm) < 0.002 && fabs(g_norm - b_norm) < 0.002)) {
        if (r_norm > b_norm && r_norm >= g_norm * 0.95) {
            return "VERMELHO ESCURO";
        }
    }
    // Se não for branco/cinza/vermelho escuro, prioriza cor dominante mesmo em tons claros ou escuros
    return "COR MISTA";
}

// ==================== FUNÇÕES VL53L0X ====================
void vl53l0x_write_byte(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    i2c_write_blocking(I2C1_PORT, VL53L0X_ADDR, data, 2, false);
}

uint8_t vl53l0x_read_byte(uint8_t reg) {
    uint8_t value;
    i2c_write_blocking(I2C1_PORT, VL53L0X_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C1_PORT, VL53L0X_ADDR, &value, 1, false);
    return value;
}

void vl53l0x_start_measurement() {
    vl53l0x_write_byte(0x80, 0x01);
    vl53l0x_write_byte(0xFF, 0x01);
    vl53l0x_write_byte(0x00, 0x00);
    vl53l0x_write_byte(0x91, 0x3C);
    vl53l0x_write_byte(0x00, 0x01);
    vl53l0x_write_byte(0xFF, 0x00);
    vl53l0x_write_byte(0x80, 0x00);
    vl53l0x_write_byte(VL53L0X_REG_SYSRANGE_START, 0x01);
}

uint16_t vl53l0x_read_distance() {
    vl53l0x_start_measurement();
    
    uint8_t status;
    int timeout = 0;
    do {
        status = vl53l0x_read_byte(VL53L0X_REG_RESULT_RANGE_STATUS);
        sleep_ms(10);
        timeout++;
        if (timeout > 100) {
            return 0xFFFF;
        }
    } while ((status & 0x01) == 0);
    
    uint8_t range_high = vl53l0x_read_byte(0x1E);
    uint8_t range_low = vl53l0x_read_byte(0x1F);
    uint16_t distance = (range_high << 8) | range_low;
    
    vl53l0x_write_byte(0x0B, 0x01);
    
    return distance;
}

bool vl53l0x_init() {
    sleep_ms(100);
    
    uint8_t model_id = vl53l0x_read_byte(VL53L0X_REG_IDENTIFICATION_MODEL_ID);
    printf("VL53L0X - ID: 0x%02X ", model_id);
    
    if (model_id != 0xEE) {
        printf("(Erro: esperado 0xEE)\n");
        return false;
    }
    printf("OK\n");
    
    vl53l0x_write_byte(0x88, 0x00);
    vl53l0x_write_byte(0x80, 0x01);
    vl53l0x_write_byte(0xFF, 0x01);
    vl53l0x_write_byte(0x00, 0x00);
    sleep_ms(10);
    vl53l0x_write_byte(0x00, 0x01);
    vl53l0x_write_byte(0xFF, 0x00);
    vl53l0x_write_byte(0x80, 0x00);
    vl53l0x_write_byte(0x00, 0x02);
    sleep_ms(100);
    
    return true;
}

// ==================== FUNÇÕES DE UTILIDADE ====================
void i2c_scan(i2c_inst_t *port, const char *name) {
    printf("\nScaneando %s...\n", name);
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
    
        bool found = false;
        for (int addr = 0x03; addr <= 0x77; addr++) {
            int ret = i2c_write_timeout_us(port, addr, NULL, 0, false, 1000);
            if (ret >= 0) {
                printf("0x%02X ", addr);
                found = true;
            }
        }
        if (!found) {
            printf("(nenhum dispositivo encontrado)");
        }
        printf("\n");
}

// ==================== CONTROLE DE LED ====================
void led_set_color(bool red, bool green, bool blue) {
    gpio_put(LED_RED_PIN, red);
    gpio_put(LED_GREEN_PIN, green);
    gpio_put(LED_BLUE_PIN, blue);
}

// ==================== MAIN ====================
int main() {
    // SSD1306 config
    #define SSD1306_WIDTH 128
    #define SSD1306_HEIGHT 64
    ssd1306_t disp;
    uint8_t ssd1306_addr = 0x3C;
    stdio_init_all();
    sleep_ms(1000); // Aguarda USB/serial estabilizar
    // Espera até a porta serial estar pronta
    int tries = 0;
    while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT && tries < 100) {
        sleep_ms(50);
        tries++;
    }
    printf("\n==== DEBUG: INICIO MAIN ====");
    fflush(stdout);
    sleep_ms(200);
    printf("\n==== SCANNER I2C0 INICIAL ====");
    fflush(stdout);
    i2c_scan(I2C0_PORT, "I2C0 (TCS34725/SSD1306)");
    fflush(stdout);
    printf("\n==== DEBUG: FIM SCANNER ====");
    fflush(stdout);
    sleep_ms(200);
    sleep_ms(2000);
    
    printf("\n");
    printf("========================================================\n");
    printf("  Sistema Integrado - BitDogLab\n");
    printf("  Sensor de Cor TCS34725 + Sensor de Distancia VL53L0X\n");
    printf("========================================================\n\n");
    
    // Configurar LEDs
    printf("Configurando LEDs...\n");
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    led_set_color(false, false, false);
    printf("LEDs configurados (R=GP%d, G=GP%d, B=GP%d)\n\n", 
           LED_RED_PIN, LED_GREEN_PIN, LED_BLUE_PIN);
    
    // Configurar I2C0 (TCS34725 e SSD1306)
    printf("Configurando I2C0 (Sensor de Cor)...\n");
    i2c_init(I2C0_PORT, I2C0_FREQ);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);
    printf("I2C0 OK - SDA=GP%d, SCL=GP%d\n", I2C0_SDA, I2C0_SCL);
    
    // Configurar I2C1 (VL53L0X)
    printf("Configurando I2C1 (Sensor de Distancia)...\n");
    i2c_init(I2C1_PORT, I2C1_FREQ);
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);
    printf("I2C1 OK - SDA=GP%d, SCL=GP%d\n\n", I2C1_SDA, I2C1_SCL);
    
        // Inicializa I2C0 (TCS34725 e SSD1306)
        printf("Configurando I2C0 (TCS34725/SSD1306)...\n");
        i2c_init(I2C0_PORT, I2C0_FREQ);
        gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
        gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
        gpio_pull_up(I2C0_SDA);
        gpio_pull_up(I2C0_SCL);
        printf("I2C0 OK - SDA=GP%d, SCL=GP%d\n", I2C0_SDA, I2C0_SCL);

        sleep_ms(500); // Delay extra para garantir I2C estável

        // Inicializa o display SSD1306 antes de qualquer sensor
        printf("Inicializando SSD1306 no endereço 0x3C...\n");
        ssd1306_init(&disp, I2C0_PORT, 0x3C);
        printf("SSD1306 inicializado!\n");
        ssd1306_clear(&disp);
        ssd1306_draw_string(&disp, 0, 0, "BitDogLab");
        ssd1306_draw_string(&disp, 0, 16, "Sistema Inicializando...");
        ssd1306_show(&disp);
        sleep_ms(1000);

        // Scanner I2C0 para detectar endereços
        i2c_scan(I2C0_PORT, "I2C0 (TCS34725/SSD1306)");

        // Escanear barramentos
        i2c_scan(I2C0_PORT, "I2C0 (TCS34725)");
        i2c_scan(I2C1_PORT, "I2C1 (VL53L0X)");

    // Inicializar TCS34725
    printf("Inicializando TCS34725 (Sensor de Cor)...\n");
    if (!tcs34725_init()) {
        printf("ERRO: Falha ao inicializar TCS34725!\n");
        while (1) {
            led_set_color(true, false, false);
            sleep_ms(200);
            led_set_color(false, false, false);
            sleep_ms(200);
        }
    }
    printf("TCS34725 inicializado\n\n");
    
    // Inicializar VL53L0X
    printf("Inicializando VL53L0X (Sensor de Distancia)...\n");
    if (!vl53l0x_init()) {
        printf("ERRO: Falha ao inicializar VL53L0X!\n");
        while (1) {
            led_set_color(true, false, false);
            sleep_ms(500);
            led_set_color(false, false, false);
            sleep_ms(500);
        }
    }
    printf("VL53L0X inicializado\n\n");
    
    printf("========================================================\n");
    printf("  Sistema pronto!\n");
    printf("  - Distancia < 15cm  -> LED VERMELHO\n");
    printf("  - Distancia >= 15cm -> LED VERDE\n");
    printf("========================================================\n\n");
    
    sleep_ms(1000);
    
    uint16_t r, g, b, c;
    uint16_t distance;
    uint8_t current_gain = TCS34725_GAIN_16X;
    // Loop principal
    while (true) {
        // Ler sensor de cor
        tcs34725_read_colors(&r, &g, &b, &c);
        // Ajuste dinâmico de ganho
        if (c > 30000 && current_gain > TCS34725_GAIN_1X) {
            current_gain--;
            tcs34725_write_byte(TCS34725_CONTROL, current_gain);
            printf("[AutoGain] Ambiente claro, reduzindo ganho para %dx\n", (1 << (2*current_gain)));
            sleep_ms(100);
            continue;
        } else if (c < 2000 && current_gain < TCS34725_GAIN_60X) {
            current_gain++;
            tcs34725_write_byte(TCS34725_CONTROL, current_gain);
            printf("[AutoGain] Ambiente escuro, aumentando ganho para %dx\n", (1 << (2*current_gain)));
            sleep_ms(100);
            continue;
        }
        const char* cor = get_color_name(r, g, b, c);
        // Ler sensor de distância
        distance = vl53l0x_read_distance();
        // Controlar LED baseado na distância
        if (distance != 0xFFFF && distance < 2000) {
            if (distance < 150) {  // Menos de 15cm (150mm)
                led_set_color(true, false, false);  // Vermelho
            } else {
                led_set_color(false, true, false);  // Verde
            }
        } else {
            led_set_color(false, false, false);  // Desligado
        }
        // Exibir no serial monitor
        printf("+-----------------------------------------------------------+\n");
        // Informações de Cor
        printf("| COR: %-18s                              |\n", cor);
        printf("|   R:%5u  G:%5u  B:%5u  C:%5u  GANHO:%dx         |\n", r, g, b, c, (1 << (2*current_gain)));

        // Exibir distância no display
        char linha1[22], linha2[22];
        snprintf(linha1, sizeof(linha1), "Dist: %4d mm", distance == 0xFFFF ? -1 : distance);
        snprintf(linha2, sizeof(linha2), "Cor: %s", cor);
        ssd1306_clear(&disp);
        ssd1306_draw_string(&disp, 0, 0, 2, linha1);
        ssd1306_draw_string(&disp, 0, 24, 1, linha2);
        ssd1306_show(&disp);
        // Informações de Distância
        if (distance == 0xFFFF) {
            printf("| DISTANCIA: ERRO (Timeout)                                |\n");
        } else if (distance > 2000) {
            printf("| DISTANCIA: Fora de alcance (> 200cm)                     |\n");
        } else {
            printf("| DISTANCIA: %4d mm (%.1f cm)                           |\n", 
                   distance, distance / 10.0);
            if (distance < 150) {
                printf("| LED: VERMELHO (< 15cm)                                   |\n");
            } else {
                printf("| LED: VERDE (>= 15cm)                                     |\n");
            }
        }
        printf("+-----------------------------------------------------------+\n\n");
        sleep_ms(500);
    }
    
    return 0;
}
