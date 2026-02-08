/**
 * Sistema Integrado com WiFi/HTTP - TCS34725 + VL53L0X
 * BitDogLab - FreeRTOS Edition
 * 
 * Envia dados dos sensores para servidor HTTP
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "lwip/apps/http_client.h"

// ==================== CONFIGURAÇÕES WiFi/HTTP ====================
#define WIFI_SSID       "DRACON"           // <<<< CONFIGURE AQUI
#define WIFI_PASSWORD   "am3426bn14"          // <<<< CONFIGURE AQUI
#define SERVER_IP       "192.168.1.100"           // <<<< CONFIGURE AQUI
#define SERVER_PORT     5000                      // <<<< CONFIGURE AQUI

// ==================== DEFINIÇÕES I2C ====================
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

// ==================== ESTRUTURA DE DADOS ====================
typedef struct {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t clear;
    uint16_t distance;
    char color_name[20];
} SensorData;

// ==================== VARIÁVEIS GLOBAIS ====================
QueueHandle_t xQueueSensorData;
static volatile bool requisicao_em_curso = false;

// ==================== FUNÇÕES FreeRTOS STATIC MEMORY ====================
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

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
    vTaskDelay(pdMS_TO_TICKS(100));
    uint8_t id = tcs34725_read_byte(TCS34725_ID);
    printf("TCS34725 ID: 0x%02X\n", id);
    
    tcs34725_write_byte(TCS34725_ENABLE, TCS34725_ENABLE_PON);
    vTaskDelay(pdMS_TO_TICKS(3));
    tcs34725_write_byte(TCS34725_ENABLE, TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN);
    vTaskDelay(pdMS_TO_TICKS(3));
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

void get_color_name(uint16_t r, uint16_t g, uint16_t b, uint16_t c, char *name) {
    if (c < 100) {
        strcpy(name, "ESCURO");
        return;
    } else if (c > 10000) {
        strcpy(name, "CLARO");
        return;
    }
    
    float total = r + g + b;
    if (total < 1) total = 1;
    
    float r_ratio = (float)r / total;
    float g_ratio = (float)g / total;
    float b_ratio = (float)b / total;
    
    if (r_ratio > 0.40 && g_ratio < 0.35 && b_ratio < 0.35) {
        strcpy(name, "VERMELHO");
    } else if (g_ratio > 0.40 && r_ratio < 0.35 && b_ratio < 0.35) {
        strcpy(name, "VERDE");
    } else if (b_ratio > 0.40 && r_ratio < 0.35 && g_ratio < 0.35) {
        strcpy(name, "AZUL");
    } else if (r_ratio > 0.40 && g_ratio > 0.40 && b_ratio < 0.30) {
        strcpy(name, "AMARELO");
    } else if (r_ratio > 0.30 && g_ratio > 0.30 && b_ratio > 0.30) {
        strcpy(name, "BRANCO");
    } else {
        strcpy(name, "MISTA");
    }
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
        vTaskDelay(pdMS_TO_TICKS(10));
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
    vTaskDelay(pdMS_TO_TICKS(100));
    uint8_t model_id = vl53l0x_read_byte(VL53L0X_REG_IDENTIFICATION_MODEL_ID);
    printf("VL53L0X ID: 0x%02X\n", model_id);
    
    if (model_id != 0xEE) return false;
    
    vl53l0x_write_byte(0x88, 0x00);
    vl53l0x_write_byte(0x80, 0x01);
    vl53l0x_write_byte(0xFF, 0x01);
    vl53l0x_write_byte(0x00, 0x00);
    vTaskDelay(pdMS_TO_TICKS(10));
    vl53l0x_write_byte(0x00, 0x01);
    vl53l0x_write_byte(0xFF, 0x00);
    vl53l0x_write_byte(0x80, 0x00);
    vl53l0x_write_byte(0x00, 0x02);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    return true;
}

// ==================== LED CONTROL ====================
void led_set_color(bool red, bool green, bool blue) {
    gpio_put(LED_RED_PIN, red);
    gpio_put(LED_GREEN_PIN, green);
    gpio_put(LED_BLUE_PIN, blue);
}

// ==================== HTTP CALLBACK ====================
static void http_client_callback(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err) {
    requisicao_em_curso = false;
    if (httpc_result == HTTPC_RESULT_OK) {
        printf("HTTP: Envio OK! Status: %d\n", (int)srv_res);
    } else {
        printf("HTTP: Falha %d\n", (int)httpc_result);
    }
}

// ==================== TASK: HTTP POST ====================
void http_task(void *pvParameters) {
    SensorData data;
    ip_addr_t server_addr;
    ip4addr_aton(SERVER_IP, &server_addr);

    while (true) {
        if (xQueueReceive(xQueueSensorData, &data, portMAX_DELAY)) {
            
            if (requisicao_em_curso) {
                vTaskDelay(pdMS_TO_TICKS(1500));
                requisicao_em_curso = false;
            }

            if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
                requisicao_em_curso = true;
                
                httpc_connection_t settings;
                memset(&settings, 0, sizeof(settings));
                settings.result_fn = http_client_callback;
                settings.use_proxy = 0;

                char uri[128];
                snprintf(uri, sizeof(uri), 
                    "/data?r=%d&g=%d&b=%d&c=%d&dist=%d&cor=%s",
                    data.red, data.green, data.blue, data.clear, 
                    data.distance, data.color_name);

                printf("HTTP: Enviando %s dist=%dmm...\n", data.color_name, data.distance);

                err_t err = httpc_get_file(&server_addr, SERVER_PORT, uri, 
                                          &settings, NULL, NULL, NULL);

                if (err != ERR_OK) {
                    requisicao_em_curso = false;
                    printf("HTTP Erro: %d\n", (int)err);
                }
            } else {
                printf("WiFi: Sem conexao\n");
            }
            
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

// ==================== TASK: SENSORES ====================
void sensor_task(void *pvParameters) {
    SensorData data;
    
    while (true) {
        // Ler sensores
        tcs34725_read_colors(&data.red, &data.green, &data.blue, &data.clear);
        get_color_name(data.red, data.green, data.blue, data.clear, data.color_name);
        data.distance = vl53l0x_read_distance();
        
        // Controlar LED
        if (data.distance != 0xFFFF && data.distance < 2000) {
            if (data.distance < 150) {
                led_set_color(true, false, false);
            } else {
                led_set_color(false, true, false);
            }
        } else {
            led_set_color(false, false, false);
        }
        
        // Exibir no serial
        printf("+-----------------------------------------------------------+\n");
        printf("| COR: %-18s                              |\n", data.color_name);
        printf("|   R:%5u  G:%5u  B:%5u  C:%5u               |\n", 
               data.red, data.green, data.blue, data.clear);
        if (data.distance != 0xFFFF && data.distance < 2000) {
            printf("| DISTANCIA: %4d mm (%.1f cm)                           |\n", 
                   data.distance, data.distance / 10.0);
            printf("| LED: %s                                              |\n",
                   data.distance < 150 ? "VERMELHO" : "VERDE");
        }
        printf("+-----------------------------------------------------------+\n\n");
        
        // Enviar para fila HTTP
        xQueueOverwrite(xQueueSensorData, &data);
        
        vTaskDelay(pdMS_TO_TICKS(5000));  // Leitura a cada 5 segundos
    }
}

// ==================== TASK: WiFi ====================
void wifi_task(void *pvParameters) {
    if (cyw43_arch_init()) {
        printf("WiFi: Falha ao inicializar!\n");
        vTaskDelete(NULL);
    }
    
    cyw43_arch_enable_sta_mode();
    printf("WiFi: Conectando a %s...\n", WIFI_SSID);
    
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, 
                                           CYW43_AUTH_WPA2_AES_PSK, 30000) == 0) {
        printf("WiFi: Conectado!\n");
    } else {
        printf("WiFi: Falha na conexao!\n");
    }
    
    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ==================== MAIN ====================
int main() {
    stdio_init_all();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    printf("\n");
    printf("========================================================\n");
    printf("  Sistema Integrado WiFi/HTTP - BitDogLab\n");
    printf("  TCS34725 + VL53L0X + FreeRTOS\n");
    printf("========================================================\n\n");
    
    // Configurar LEDs
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    led_set_color(false, false, false);
    
    // Configurar I2C0 (TCS34725)
    i2c_init(I2C0_PORT, I2C0_FREQ);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);
    
    // Configurar I2C1 (VL53L0X)
    i2c_init(I2C1_PORT, I2C1_FREQ);
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);
    
    // Inicializar sensores (antes do FreeRTOS)
    printf("Inicializando sensores...\n");
    if (!tcs34725_init()) {
        printf("ERRO: TCS34725 falhou!\n");
    }
    if (!vl53l0x_init()) {
        printf("ERRO: VL53L0X falhou!\n");
    }
    printf("Sensores OK!\n\n");
    
    // Criar fila
    xQueueSensorData = xQueueCreate(1, sizeof(SensorData));
    
    // Criar tasks
    xTaskCreate(wifi_task, "WiFi_Task", 1024, NULL, 1, NULL);
    xTaskCreate(http_task, "HTTP_Task", 4096, NULL, 2, NULL);
    xTaskCreate(sensor_task, "Sensor_Task", 2048, NULL, 2, NULL);
    
    printf("Iniciando FreeRTOS...\n");
    vTaskStartScheduler();
    
    while(1) { }
}
