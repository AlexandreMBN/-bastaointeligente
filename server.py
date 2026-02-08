#!/usr/bin/env python3
"""
Servidor HTTP para receber dados dos sensores TCS34725 e VL53L0X
Projeto BitDogLab com Pico W
"""

from flask import Flask, request
from datetime import datetime
import csv
import os

app = Flask(__name__)

# Arquivo CSV para salvar dados
CSV_FILE = 'sensor_data.csv'

def init_csv():
    """Inicializa o arquivo CSV com cabeçalhos se não existir"""
    if not os.path.exists(CSV_FILE):
        with open(CSV_FILE, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['Timestamp', 'Cor', 'R', 'G', 'B', 'Clear', 'Distancia_mm', 'LED_Estado'])

def save_to_csv(data):
    """Salva dados no arquivo CSV"""
    with open(CSV_FILE, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(data)

@app.route('/data')
def receive_data():
    """Endpoint para receber dados do Pico W"""
    try:
        # Extrair parâmetros
        r = request.args.get('r', 0)
        g = request.args.get('g', 0)
        b = request.args.get('b', 0)
        c = request.args.get('c', 0)
        dist = request.args.get('dist', 0)
        cor = request.args.get('cor', 'DESCONHECIDO')
        
        # Determinar estado do LED
        try:
            dist_int = int(dist)
            led_estado = "VERMELHO" if dist_int < 150 else "VERDE"
        except:
            led_estado = "DESLIGADO"
        
        # Timestamp
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        # Exibir no console
        print("=" * 70)
        print(f"📅 {timestamp}")
        print(f"🎨 Cor Detectada: {cor}")
        print(f"🔴 R: {r:>5}  🟢 G: {g:>5}  🔵 B: {b:>5}  ⚪ Clear: {c:>5}")
        print(f"📏 Distância: {dist:>4} mm ({int(dist)/10:.1f} cm)")
        print(f"💡 LED: {led_estado}")
        print("=" * 70)
        print()
        
        # Salvar em CSV
        data_row = [timestamp, cor, r, g, b, c, dist, led_estado]
        save_to_csv(data_row)
        
        return "OK", 200
        
    except Exception as e:
        print(f"❌ Erro ao processar dados: {e}")
        return "ERROR", 500

@app.route('/')
def index():
    """Página inicial"""
    return """
    <html>
    <head>
        <title>Servidor BitDogLab</title>
        <meta http-equiv="refresh" content="2">
        <style>
            body {
                font-family: Arial, sans-serif;
                background: #1e1e1e;
                color: #fff;
                padding: 20px;
            }
            h1 { color: #4CAF50; }
            .container {
                max-width: 800px;
                margin: 0 auto;
                background: #2d2d2d;
                padding: 20px;
                border-radius: 10px;
            }
            .status {
                background: #4CAF50;
                padding: 10px;
                border-radius: 5px;
                margin: 10px 0;
            }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>🚀 Servidor BitDogLab - Sensor Data</h1>
            <div class="status">
                ✅ Servidor ativo e aguardando dados...
            </div>
            <p>Endpoint: <code>/data</code></p>
            <p>Dados salvos em: <code>sensor_data.csv</code></p>
            <p>Esta página atualiza automaticamente a cada 2 segundos.</p>
        </div>
    </body>
    </html>
    """

@app.route('/status')
def status():
    """Status do servidor"""
    return {
        "status": "online",
        "timestamp": datetime.now().isoformat(),
        "csv_file": CSV_FILE,
        "file_exists": os.path.exists(CSV_FILE)
    }

if __name__ == '__main__':
    print()
    print("=" * 70)
    print("  🌐 Servidor HTTP BitDogLab - Receptor de Dados dos Sensores")
    print("=" * 70)
    print()
    print("📡 Escutando em: http://0.0.0.0:5000")
    print("📊 Endpoint de dados: http://0.0.0.0:5000/data")
    print("💾 Salvando dados em:", CSV_FILE)
    print()
    print("⚙️  Configure o Pico W com:")
    print("   SERVER_IP = [SEU_IP_LOCAL]")
    print("   SERVER_PORT = 5000")
    print()
    print("🛑 Pressione Ctrl+C para parar o servidor")
    print("=" * 70)
    print()
    
    # Inicializar CSV
    init_csv()
    
    # Iniciar servidor
    try:
        app.run(host='0.0.0.0', port=5000, debug=False)
    except KeyboardInterrupt:
        print("\n\n👋 Servidor encerrado pelo usuário")
