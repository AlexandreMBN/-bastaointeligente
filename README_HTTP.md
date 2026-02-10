# Sistema Integrado WiFi/HTTP - TCS34725 + VL53L0X
**BitDogLab - Versão com FreeRTOS e HTTP**

## 📋 Resumo do Projeto

Este projeto integra:
- **Sensor de Cor TCS34725** (I2C0 - GP0/GP1)
- **Sensor de Distância VL53L0X** (I2C1 - GP2/GP3)
- **LED RGB** com controle baseado em distância
- **WiFi** (Pico W)
- **Envio de dados via HTTP** para servidor remoto
- **FreeRTOS** para multitarefa

## ⚙️ Configuração Necessária

### 1. Hardware Requerido
- **Raspberry Pi Pico W** (obrigatório para WiFi)
- Sensor TCS34725 (cor)
- Sensor VL53L0X (distância)
- LED RGB (or 3 LEDs separados)
- Cabos de conexão

### 2. Conexões Físicas

**TCS34725 (Sensor de Cor):**
```
VCC → 3.3V
GND → GND
SDA → GP0
SCL → GP1
```

**VL53L0X (Sensor de Distância):**
```
VCC → 3.3V
GND → GND
SDA → GP2
SCL → GP3
```

**LED RGB:**
```
LED Vermelho → GP13
LED Verde    → GP11
LED Azul     → GP12
```

### 3. Configurar WiFi e Servidor

Edite o arquivo `main_http.c` nas linhas 19-22:

```c
#define WIFI_SSID       "SEU_WIFI_AQUI"           // <<<< CONFIGURE AQUI
#define WIFI_PASSWORD   "SUA_SENHA_AQUI"          // <<<< CONFIGURE AQUI
#define SERVER_IP       "192.168.1.100"           // <<<< CONFIGURE AQUI
#define SERVER_PORT     5000                      // <<<< CONFIGURE AQUI
```

## 🌐 Servidor HTTP Receptor

Crie um servidor HTTP simples para receber os dados. Exemplo em Python:

```python
from flask import Flask, request
app = Flask(__name__)

@app.route('/data')
def receive_data():
    r = request.args.get('r')
    g = request.args.get('g')
    b = request.args.get('b')
    c = request.args.get('c')
    dist = request.args.get('dist')
    cor = request.args.get('cor')
    
    print(f"Cor: {cor} | RGB: ({r},{g},{b}) Clear:{c} | Distância: {dist}mm")
    return "OK", 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
```

Execute com: `python server.py`

## 📡 Formato dos Dados HTTP

O Pico W envia requisições GET no formato:
```
http://SERVER_IP:PORT/data?r=1234&g=5678&b=9012&c=3456&dist=150&cor=VERMELHO
```

**Parâmetros:**
- `r`: Valor vermelho (0-65535)
- `g`: Valor verde (0-65535)
- `b`: Valor azul (0-65535)
- `c`: Valor clear/luminosidade (0-65535)
- `dist`: Distância em milímetros
- `cor`: Nome da cor detectada

## 🔨 Compilação

### Problema Atual - FreeRTOS + WiFi

O projeto está configurado mas há um conflito entre `pico_cyw43_arch_lwip_sys_freertos` e as configurações do lwIP.

**Soluções:**

### Opção 1: Usar Projeto de Referência (Recomendado)
Copie o projeto funcionando de:
```
c:\Users\Alexandre\Desktop\ExemploFreeRTOSHTTP\rp2040-freertos-template-main
```

E adapte o código dos sensores TCS34725 e VL53L0X para ele.

### Opção 2: Compilar Manualmente (Para Desenvolvedores)

1. Certifique-se que tem `PICO_BOARD` definido como `pico_w` no CMakeLists.txt
2. FreeRTOS-Kernel deve estar em `lib/FreeRTOS-Kernel`
3. Use o código de referência do FreeRTOS que já funciona

### Opção 3: Versão Simplificada sem FreeRTOS

Se você não precisa de FreeRTOS, use o arquivo `main.c` original que já compila e funciona com os sensores via serial.

## 🚀 Uso

1. Compile o projeto
2. Inicie o servidor HTTP no seu computador
3. Grave o firmware no Pico W
4. Conecte via monitor serial USB (115200 baud)
5. Observe as leituras sendo enviadas via HTTP

## 📊 Comportamento

- **Leituras a cada 5 segundos**
- **LED Vermelho**: Distância < 15cm
- **LED Verde**: Distância ≥ 15cm  
- **Serial Monitor**: Exibe tabela formatada com todos os dados
- **HTTP**: Envia dados para o servidor configurado

## 📁 Arquivos do Projeto

- `main_http.c` - Código principal com FreeRTOS e HTTP
- `main.c` - Código simples sem WiFi (apenas serial)
- `FreeRTOSConfig.h` - Configuração do FreeRTOS
- `lwipopts.h` - Configuração do lwIP (TCP/IP stack)
- `CMakeLists.txt` - Configuração de build
- `lib/FreeRTOS-Kernel/` - Biblioteca FreeRTOS

## ⚠️ Troubleshooting

### Erro: "CYW43_LWIP requires NO_SYS=1"
- Problema de configuração entre FreeRTOS e WiFi
- Use o projeto de referência que já funciona

### WiFi não conecta
1. Verifique SSID e senha
2. Certifique-se que está usando Pico W (não Pico normal)
3. Verifique se o roteador está acessível

### Sensores não detectados
1. Verifique as conexões físicas
2. Ambos sensores usam endereço I2C 0x29
3. Por isso estão em barramentos I2C diferentes

### HTTP não envia dados
1. Verifique se WiFi conectou (LED interno piscará)
2. Confirme que servidor está rodando
3. Teste o IP do servidor com ping
4. Verifique firewall do computador

## 📚 Referências

- [Pico W Datasheet](https://datasheets.raspberrypi.com/picow/pico-w-datasheet.pdf)
- [FreeRTOS](https://www.freertos.org/)
- [lwIP Documentation](https://www.nongnu.org/lwip/)
- [TCS34725 Datasheet](https://cdn-shop.adafruit.com/datasheets/TCS34725.pdf)
- [VL53L0X Datasheet](https://www.st.com/resource/en/datasheet/vl53l0x.pdf)

## 📝 Notas Importantes

1. **Dois Sensores, Mesmo Endereço**: TCS34725 e VL53L0X usam 0x29, por isso estão em barramentos I2C separados
2. **Pico W Obrigatório**: WiFi só funciona com Pico W, não com Pico normal
3. **FreeRTOS + WiFi**: Configuração complexa, use projeto de referência se tiver prolemas
4. **Alternativa Simples**: Use `main.c` sem WiFi para apenas testar sensores

---

**Autor**: Alexandre Magno Braga do Nascimento  
**Data**: Fevereiro 2026  
**Versão**: 1.0 - FreeRTOS + HTTP
