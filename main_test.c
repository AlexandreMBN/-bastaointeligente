/**
 * Código de Teste Simples - TCS34725 + VL53L0X
 * SEM WiFi, SEM FreeRTOS - Apenas sensores e LED
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// ==================== I2C ====================
#define I2C0_PORT i2c0
#define I2C0_SDA 0
#define I2C0_SCL 1
#define I2C0_FREQ 400000

#define I2C1_PORT i2c1
#define I2C1_SDA 2
#define I2C1_SCL 3
#define I2C1_FREQ 100000

// ==================== LEDs ====================
#define LED_RED_PIN 13
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12

// ==================== TCS34725 ====================
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
#define TCS34725_GAIN_4X 0x01

// ==================== VL53L0X ====================
#define VL53L0X_ADDR 0x29
#define VL53L0X_REG_IDENTIFICATION_MODEL_ID 0xC0
#define VL53L0X_REG_SYSRANGE_START 0x00
#define VL53L0X_REG_RESULT_RANGE_STATUS 0x14

// ==================== TCS34725 Functions ====================
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

bool tcs34725_init() {
    sleep_ms(100);
    uint8_t id = tcs34725_read_byte(TCS34725_ID);
    printf("TCS34725 ID: 0x%02X\n", id);
    
    tcs34725_write_byte(TCS34725_ENABLE, TCS34725_ENABLE_PON);
    sleep_ms(3);
    tcs34725_write_byte(TCS34725_ENABLE, TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN);
    sleep_ms(3);
    tcs34725_write_byte(TCS34725_ATIME, 0xC0);
    tcs34725_write_byte(TCS34725_CONTROL, TCS34725_GAIN_4X);
    
    return true;
}

void tcs34725_read_colors(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c) {
    *c = tcs34725_read_word(TCS34725_CDATAL);
    *r = tcs34725_read_word(TCS34725_RDATAL);
    *g = tcs34725_read_word(TCS34725_GDATAL);
    *b = tcs34725_read_word(TCS34725_BDATAL);
}

// ==================== VL53L0X Functions ====================
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
        if (timeout > 100) return 0xFFFF;
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
    printf("VL53L0X ID: 0x%02X\n", model_id);
    
    if (model_id != 0xEE) return false;
    
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

// ==================== LED Control ====================
void led_set_color(bool red, bool green, bool blue) {
    gpio_put(LED_RED_PIN, red);
    gpio_put(LED_GREEN_PIN, green);
    gpio_put(LED_BLUE_PIN, blue);
}

// ==================== MAIN ====================
int main() {
    stdio_init_all();
    
    // Aguardar serial conectar
    sleep_ms(2000);
    
    printf("\n");
    printf("========================================================\n");
    printf("  TESTE SIMPLES - TCS34725 + VL53L0X\n");
    printf("  Sem WiFi, Sem FreeRTOS\n");
    printf("========================================================\n\n");
    
    // Configurar LEDs
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    led_set_color(false, false, false);
    
    // LED teste inicial - pisca azul 3x
    printf("Teste LED...\n");
    for(int i = 0; i < 3; i++) {
        led_set_color(false, false, true);
        sleep_ms(200);
        led_set_color(false, false, false);
        sleep_ms(200);
    }
    
    // Configurar I2C0 para TCS34725
    printf("Configurando I2C0...\n");
    i2c_init(I2C0_PORT, I2C0_FREQ);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);
    
    // Configurar I2C1 para VL53L0X
    printf("Configurando I2C1...\n");
    i2c_init(I2C1_PORT, I2C1_FREQ);
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);
    
    // Inicializar sensores
    printf("Inicializando TCS34725...\n");
    if (!tcs34725_init()) {
        printf("ERRO: TCS34725 falhou!\n");
    } else {
        printf("TCS34725 OK!\n");
    }
    
    printf("Inicializando VL53L0X...\n");
    if (!vl53l0x_init()) {
        printf("ERRO: VL53L0X falhou!\n");
    } else {
        printf("VL53L0X OK!\n");
    }
    
    printf("\nIniciando leituras a cada 3 segundos...\n\n");
    
    // Loop principal
    int counter = 0;
    while (true) {
        counter++;
        
        // Ler TCS34725
        uint16_t r, g, b, c;
        tcs34725_read_colors(&r, &g, &b, &c);
        
        // Ler VL53L0X
        uint16_t distance = vl53l0x_read_distance();
        
        // Controlar LED baseado em distância
        if (distance != 0xFFFF && distance < 2000) {
            if (distance < 150) {
                led_set_color(true, false, false);  // Vermelho
            } else {
                led_set_color(false, true, false);  // Verde
            }
        } else {
            led_set_color(false, false, false);  // Desligado
        }
        
        // Exibir no serial
        printf("+-----------------------------------------------------------+\n");
        printf("| Leitura #%d\n", counter);
        printf("| RGB: R=%5u  G=%5u  B=%5u  C=%5u\n", r, g, b, c);
        
        if (distance != 0xFFFF && distance < 2000) {
            printf("| Distancia: %4d mm (%.1f cm)\n", distance, distance / 10.0);
            printf("| LED: %s\n", distance < 150 ? "VERMELHO" : "VERDE");
        } else {
            printf("| Distancia: FORA DE ALCANCE\n");
            printf("| LED: DESLIGADO\n");
        }
        
        printf("+-----------------------------------------------------------+\n\n");
        
        sleep_ms(3000);
    }
    
    return 0;
}
