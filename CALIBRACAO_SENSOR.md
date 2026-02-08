# Calibração do Sensor de Cores TCS34725

## Melhorias Implementadas

1. **Ganho aumentado de 4x para 16x** - Maior sensibilidade
2. **Tempo de integração otimizado** - 24ms (0xF6) para leituras mais rápidas
3. **Algoritmo melhorado** - Normalização pelo valor Clear (C) em vez de total RGB
4. **Detecção mais precisa** - Usa diferenças relativas entre componentes

## Como Usar

### 1. Compile e carregue o código atualizado
```bash
ninja -C build
picotool load build/blink.uf2 -fx
```

### 2. Teste com diferentes cores

Aproxime objetos das seguintes cores e observe os valores:
- Vermelho puro
- Verde puro
- Azul puro
- Amarelo
- Branco
- Preto

### 3. Ajustes Finos

Se ainda não detectar corretamente, você pode ajustar:

#### A. **Ganho do Sensor** (linha ~118)
```c
// Opções de ganho:
TCS34725_GAIN_1X   // Ambientes muito claros
TCS34725_GAIN_4X   // Iluminação normal
TCS34725_GAIN_16X  // Iluminação moderada (ATUAL)
TCS34725_GAIN_60X  // Ambientes escuros
```

#### B. **Tempo de Integração** (linha ~115)
```c
// Valores de ATIME (menor = mais tempo):
0xF6  // ~24ms - Rápido (ATUAL)
0xEB  // ~50ms - Balanceado
0xC0  // ~154ms - Mais preciso
0x00  // 700ms - Máxima precisão (muito lento)
```

#### C. **Limiar de Escuridão** (linha ~128)
```c
if (c < 50) {  // Ajuste este valor
    return "MUITO ESCURO";
}
```

## Problemas Comuns

### Sensor detecta tudo como BRANCO/CINZA
- **Solução**: Diminua o ganho ou aumente ATIME (ex: use GAIN_4X)

### Sensor detecta tudo como MUITO ESCURO
- **Solução**: Aumente o ganho (ex: use GAIN_60X)

### Cores ainda não são detectadas corretamente
1. **Verifique a iluminação**: Use luz branca uniforme
2. **Distância do objeto**: Mantenha 2-5mm do sensor
3. **Calibre os limiares**: Veja seção abaixo

## Calibração Avançada

### Verificando valores brutos

Observe os valores RGBC no monitor serial. Para cada cor:

**Exemplo - Objeto Vermelho:**
```
R:2500  G:800  B:600  C:4000
```

Calcule as razões normalizadas:
- r_norm = 2500/4000 = 0.625 (vermelho dominante ✓)
- g_norm = 800/4000 = 0.20
- b_norm = 600/4000 = 0.15

### Ajustando o Fator de Diferença

Na função `get_color_name`, linha ~147:
```c
if (diff < 0.08) {  // Ajuste este valor
    // 0.05 = mais sensível a pequenas diferenças
    // 0.10 = menos sensível, menos falsos positivos
```

### Ajustando Limiares de Cores Secundárias

Linha ~161 e similares:
```c
if (g_norm > b_norm * 1.3) {  // Fator multiplicador
    return "LARANJA";
}
```
- Aumente (ex: 1.5) para exigir maior diferença
- Diminua (ex: 1.1) para ser mais sensível

## Dicas de Uso

1. ✅ Use iluminação branca e uniforme
2. ✅ Mantenha o objeto a 2-5mm do sensor
3. ✅ Evite reflexos ou superfícies brilhantes
4. ✅ Teste com objetos de cores sólidas primeiro
5. ❌ Evite luz solar direta ou sombras

## Suporte

Se precisar de ajustes específicos para suas cores, compartilhe os valores RGBC que está obtendo!
