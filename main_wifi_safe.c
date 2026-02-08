/**
 * Sistema WiFi/HTTP SEGURO - TCS34725 + VL53L0X
 * Com inicialização USB prioritária e debug completo
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
#define WIFI_SSID       "DRACON"
#define WIFI_PASSWORD   "am3426bn14"
#define SERVER_IP       "192.168.1.100"
#define SERVER_PORT     5000

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

// ==================== Estrutura de Dados ====================
typedef struct {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t clear;
    uint16_t distance;
} SensorData;

// ==================== Variáveis Globais ====================
QueueHandle_t xQueueSensorData;
static volatile bool wifi_connected = false;
static volatile bool requisicao_em_curso = false;

// ==================== FreeRTOS Static Memory ====================
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

// ==================== TCS34725 Functions ====================
void tcs34725_write_byte(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {TCS34725_COMMAND_BIT | reg, value};
    i2c_write_blocking(I2C0_PORT, TCS34725_ADDR, buf, 2, false);
}

uint16_t tcs34725_read_word(uint8_t reg) {
    uint8_t buf[2];
    uint8_t cmd = TCS34725_COMMAND_BIT | reg;
    i2c_write_blocking(I2C0_PORT, TCS34725_ADDR, &cmd, 1, true);
    i2c_read_blocking(I2C0_PORT, TCS34725_ADDR, buf, 2, false);
    return (buf[1] << 8) | buf[0];
}

bool tcs34725_init() {
    vTaskDelay(pdMS_TO_TICKS(100));
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

// ==================== LED ====================
void led_set_color(bool red, bool green, bool blue) {
    gpio_put(LED_RED_PIN, red);
    gpio_put(LED_GREEN_PIN, green);
    gpio_put(LED_BLUE_PIN, blue);
}

// ==================== Detectar Nome da Cor ====================
const char* get_color_name(uint16_t r, uint16_t g, uint16_t b) {
    // Encontrar o valor máximo
    uint16_t max_val = r;
    if (g > max_val) max_val = g;
    if (b > max_val) max_val = b;
    
    // Se todos os valores são muito baixos, é preto
    if (max_val < 50) return "PRETO";
    
    // Se todos os valores são altos e próximos, é branco
    if (r > 200 && g > 200 && b > 200) return "BRANCO";
    
    // Calcular diferenças relativas
    float r_ratio = (float)r / max_val;
    float g_ratio = (float)g / max_val;
    float b_ratio = (float)b / max_val;
    
    // Vermelho dominante
    if (r_ratio > 0.8 && g_ratio < 0.5 && b_ratio < 0.5) return "VERMELHO";
    
    // Verde dominante
    if (g_ratio > 0.8 && r_ratio < 0.5 && b_ratio < 0.5) return "VERDE";
    
    // Azul dominante
    if (b_ratio > 0.8 && r_ratio < 0.5 && g_ratio < 0.5) return "AZUL";
    
    // Amarelo (R+G altos, B baixo)
    if (r_ratio > 0.7 && g_ratio > 0.7 && b_ratio < 0.5) return "AMARELO";
    
    // Ciano (G+B altos, R baixo)
    if (g_ratio > 0.7 && b_ratio > 0.7 && r_ratio < 0.5) return "CIANO";
    
    // Magenta (R+B altos, G baixo)
    if (r_ratio > 0.7 && b_ratio > 0.7 && g_ratio < 0.5) return "MAGENTA";
    
    // Laranja (R alto, G médio, B baixo)
    if (r_ratio > 0.9 && g_ratio > 0.4 && g_ratio < 0.7 && b_ratio < 0.4) return "LARANJA";
    
    // Cinza (todos próximos mas não muito altos)
    if (r > 80 && g > 80 && b > 80 && r < 200 && g < 200 && b < 200) {
        float diff_rg = (r > g) ? (float)(r - g) / max_val : (float)(g - r) / max_val;
        float diff_rb = (r > b) ? (float)(r - b) / max_val : (float)(b - r) / max_val;
        float diff_gb = (g > b) ? (float)(g - b) / max_val : (float)(b - g) / max_val;
        if (diff_rg < 0.2 && diff_rb < 0.2 && diff_gb < 0.2) return "CINZA";
    }
    
    // Marrom (R>G>B, valores médios)
    if (r > g && g > b && r < 150 && g < 100) return "MARROM";
    
    return "INDEFINIDO";
}

// ==================== HTTP Callback ====================
static void http_client_callback(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err) {
    requisicao_em_curso = false;
    if (httpc_result == HTTPC_RESULT_OK) {
        printf("HTTP: OK (Status %d)\n", (int)srv_res);
    } else {
        printf("HTTP: Falha %d\n", (int)httpc_result);
    }
}

// ==================== TASK: HTTP ====================
void http_task(void *pvParameters) {
    SensorData data;
    ip_addr_t server_addr;
    ip4addr_aton(SERVER_IP, &server_addr);

    printf("HTTP Task: Aguardando WiFi...\n");
    
    // Aguardar WiFi conectar
    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    printf("HTTP Task: WiFi OK, iniciando envios...\n");

    while (true) {
        if (xQueueReceive(xQueueSensorData, &data, portMAX_DELAY)) {
            
            // NÃO enviar se distância inválida ou fora de alcance
            if (data.distance == 0xFFFF || data.distance >= 2000) {
                printf("HTTP: Dados fora de alcance, pulando envio\n");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            
            // Aguardar requisição anterior terminar
            int wait_count = 0;
            while (requisicao_em_curso && wait_count < 10) {
                vTaskDelay(pdMS_TO_TICKS(500));
                wait_count++;
            }
            
            // Forçar reset se travou
            if (requisicao_em_curso) {
                printf("HTTP: Timeout aguardando requisicao anterior, resetando...\n");
                requisicao_em_curso = false;
            }

            if (wifi_connected) {
                requisicao_em_curso = true;
                
                httpc_connection_t settings;
                memset(&settings, 0, sizeof(settings));
                settings.result_fn = http_client_callback;
                settings.use_proxy = 0;

                char uri[128];
                snprintf(uri, sizeof(uri), 
                    "/data?r=%d&g=%d&b=%d&c=%d&dist=%d",
                    data.red, data.green, data.blue, data.clear, data.distance);

                printf("HTTP: Enviando dist=%dmm...\n", data.distance);

                err_t err = httpc_get_file(&server_addr, SERVER_PORT, uri, 
                                          &settings, NULL, NULL, NULL);

                if (err != ERR_OK) {
                    requisicao_em_curso = false;
                    if (err == -16) {
                        printf("HTTP Erro -16: Requisicao em andamento (aguarde)\n");
                    } else {
                        printf("HTTP Erro: %d\n", (int)err);
                    }
                }
            }
            
            // Delay maior entre requisições
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
}

// ==================== TASK: Sensores ====================
void sensor_task(void *pvParameters) {
    SensorData data;
    
    printf("Sensor Task: Configurando I2C...\n");
    
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
    
    printf("Sensor Task: Inicializando sensores...\n");
    tcs34725_init();
    vl53l0x_init();
    printf("Sensor Task: Sensores OK!\n");
    
    int counter = 0;
    while (true) {
        counter++;
        
        // Ler sensores
        tcs34725_read_colors(&data.red, &data.green, &data.blue, &data.clear);
        data.distance = vl53l0x_read_distance();
        
        // Controlar LED
        if (data.distance != 0xFFFF && data.distance < 2000) {
            if (data.distance < 150) {
                led_set_color(true, false, false);  // Vermelho
            } else {
                led_set_color(false, true, false);  // Verde
            }
        } else {
            led_set_color(false, false, false);
        }
        
        // Detectar nome da cor
        const char* color_name = get_color_name(data.red, data.green, data.blue);
        
        // Exibir
        printf("+-----------------------------------------------------------+\n");
        printf("| #%d RGB: R=%5u G=%5u B=%5u C=%5u | Cor: %s\n", 
               counter, data.red, data.green, data.blue, data.clear, color_name);
        if (data.distance != 0xFFFF && data.distance < 2000) {
            printf("| Distancia: %4d mm (%d cm) | LED: %s\n", 
                   data.distance, data.distance / 10, data.distance < 150 ? "VERMELHO" : "VERDE");
        } else {
            printf("| Distancia: FORA DE ALCANCE | LED: DESLIGADO\n");
        }
        printf("+-----------------------------------------------------------+\n");
        
        // Enviar para fila HTTP
        xQueueOverwrite(xQueueSensorData, &data);
        
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

// ==================== TASK: WiFi ====================
void wifi_task(void *pvParameters) {
    printf("WiFi Task: Inicializando...\n");
    
    if (cyw43_arch_init()) {
        printf("WiFi: ERRO ao inicializar!\n");
        // Continua sem WiFi
        while(true) {
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
    
    cyw43_arch_enable_sta_mode();
    printf("WiFi: Conectando a '%s'...\n", WIFI_SSID);
    
    int result = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, 
                                                     CYW43_AUTH_WPA2_AES_PSK, 30000);
    
    if (result == 0) {
        printf("WiFi: CONECTADO!\n");
        wifi_connected = true;
    } else {
        printf("WiFi: Falha na conexao (erro %d)\n", result);
        printf("WiFi: Continuando SEM WiFi (apenas leitura sensores)\n");
    }
    
    // LED interno pisca
    while (true) {
        if (wifi_connected) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

// ==================== MAIN ====================
int main() {
    // PRIORIDADE 1: Inicializar USB Serial
    stdio_init_all();
    
    // Aguardar serial conectar (CRÍTICO para debug)
    sleep_ms(3000);
    
    printf("\n\n");
    printf("========================================================\n");
    printf("  Sistema WiFi/HTTP SEGURO - BitDogLab\n");
    printf("  TCS34725 + VL53L0X + FreeRTOS\n");
    printf("========================================================\n");
    printf("USB Serial: OK\n");
    printf("Versao: SAFE (inicializacao controlada)\n\n");
    
    // LED teste - pisca azul 3x
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    
    printf("Teste LED azul...\n");
    for(int i = 0; i < 3; i++) {
        led_set_color(false, false, true);
        sleep_ms(200);
        led_set_color(false, false, false);
        sleep_ms(200);
    }
    printf("LED: OK\n\n");
    
    // Criar fila
    xQueueSensorData = xQueueCreate(1, sizeof(SensorData));
    
    printf("Criando tasks FreeRTOS...\n");
    // Tasks com prioridades ajustadas
    xTaskCreate(sensor_task, "Sensores", 2048, NULL, 3, NULL);  // Maior prioridade
    xTaskCreate(wifi_task, "WiFi", 1024, NULL, 1, NULL);        // Menor prioridade
    xTaskCreate(http_task, "HTTP", 4096, NULL, 2, NULL);        // Média prioridade
    
    printf("Iniciando scheduler FreeRTOS...\n\n");
    vTaskStartScheduler();
    
    // Nunca deve chegar aqui
    printf("ERRO: Scheduler falhou!\n");
    while(1) { 
        tight_loop_contents();
    }
}
