# ⌨️ MynouPAD S3 — Master Console v6.0

**MynouPAD** é um macro pad inteligente e console de automação de alto desempenho desenvolvido para a plataforma **ESP32-S3**. Combinando emulação de teclado e mídia USB nativa, uma partição interna dedicada para scripts de automação, uma dashboard web de administração e integração direta com o **Home Assistant**, ele eleva a produtividade e o controle de infraestrutura ao próximo nível.

---

## 🚀 Principais Recursos

- **⚡ Hardware Poderoso**: Baseado no ESP32-S3 Dual-Core @ 240MHz com 16MB de memória Flash.
- **📁 Armazenamento Dedicado (FFat)**: Partição de 9.9MB para guardar arquivos de macro de forma independente.
- **🖥️ Interface Gráfica OLED**: Display SSD1306 OLED (128x64) via I2C mostrando o status do volume, status do Mute, ações disparadas e menu navegável.
- **🔄 Rotary Encoder Multifuncional**: Controle de volume de alta precisão com suporte a clique curto (Mute), clique duplo (Combo) e clique longo (abrir menu de macros no OLED).
- **🎛️ Matriz de Teclas 3x3**: Mapeamento físico corrigido de 9 teclas com suporte a macros normais e combos avançados em conjunto com o Knob.
- **🌐 Web Admin Console**: Dashboard industrial moderna integrada para mapear botões, gerenciar arquivos na memória flash e criar/editar automações em **DuckyScript** direto do navegador.
- **🏡 Automação Home Assistant**: Comando customizado `HA_TOGGLE` para disparar interruptores, luzes e cenas diretamente na rede local sem depender do PC.
- **🔌 Emulação USB Nativa**: Usa a pilha TinyUSB para se comportar como um dispositivo USB HID (teclado/mídia) e porta serial CDC simultaneamente.
- **💓 Feedback Haptic (Vibração)**: Gerenciador de vibração com 5 padrões de feedbacks táteis distintos para ações (clique normal, mute, erro de macro, rotação do knob, etc.).
- **🌈 Status RGB (NeoPixel)**: Feedback visual em tempo real para indicar o estado da ponte com o Python, mutado/desmutado e carregamento de macros.

---

## 🛠️ Especificações de Pinagem (Hardware)

| Componente | Pinos ESP32-S3 | Função |
| :--- | :--- | :--- |
| **Rotary Encoder CLK/DT** | `GPIO 1` & `GPIO 2` | Leitura de rotação do knob |
| **Rotary Encoder SW (Botão)**| `GPIO 4` | Cliques curto, longo e modo combo |
| **Buzzer** | `GPIO 10` | Avisos sonoros de boot e ações |
| **Motor de Vibração (Haptic)** | `GPIO 18` | Feedback tátil |
| **NeoPixel RGB LED** | `GPIO 48` | Status visual colorido |
| **Matriz Linhas (Rows)** | `GPIO 7`, `6`, `5` | Varredura física das teclas |
| **Matriz Colunas (Cols)** | `GPIO 15`, `16`, `17` | Detecção física das teclas |
| **I2C SDA / SCL** | `GPIO 8` / `GPIO 9` | Comunicação com o display OLED |

---

## 📁 Estrutura de Arquivos

* `MynouPAD v1.txt`: Código-fonte principal do firmware (arquivo principal C++/Arduino).
* `secrets.h`: Contém credenciais sigilosas (Wi-Fi e Tokens). **(Ignorado pelo Git por segurança)**.
* `secrets.h.example`: Modelo de configuração pública.
* `.gitignore`: Configuração para ignorar arquivos de build, IDEs e credenciais.

---

## ⚙️ Como Configurar e Compilar

### 1. Criar o Arquivo de Credenciais
Copie o arquivo `src/secrets.h.example` para `src/secrets.h` e insira suas credenciais:

```cpp
// src/secrets.h
#ifndef SECRETS_H
#define SECRETS_H

#define SECRET_WIFI_SSID     "NOME_DA_SUA_REDE"
#define SECRET_WIFI_PASSWORD "SENHA_DA_SUA_REDE"
#define SECRET_HA_TOKEN      "Bearer SEU_TOKEN_DO_HOME_ASSISTANT"

#endif
```

### 2. Configurações da Arduino IDE (Se aplicável)
* **Placa**: ESP32S3 Dev Module
* **USB CDC On Boot**: Enabled
* **USB Mode**: TinyUSB
* **Partition Scheme**: 16M Flash (3MB APP / 9.9MB FATFS)

### 3. Configurações do PlatformIO
O arquivo `platformio.ini` deve estar configurado para utilizar a pilha USB adequada e a partição customizada de 16MB.

---

## ✍️ Sintaxe DuckyScript Suportada

A engine nativa do MynouPAD suporta os seguintes comandos nos scripts armazenados no Flash:

* `STRING <texto>`: Digita o texto especificado.
* `DELAY <milisegundos>`: Pausa a execução pelo tempo definido.
* `ENTER`, `TAB`, `ESC`: Pressiona as respectivas teclas especiais.
* `GUI` ou `GUI <tecla>`: Pressiona a tecla Windows/Command (sozinha ou combinada com outra).
* `CTRL <tecla>` ou `CONTROL <tecla>`: Pressiona a tecla Control combinada com outra.
* `HA_TOGGLE <entity_id>`: **(Customizado)** Alterna o estado de um dispositivo no Home Assistant diretamente via API local. Exemplo:
  ```text
  HA_TOGGLE light.luminaria_escritorio
  ```

---

Desenvolvido com ☕ e 💻 por [Mynoush](https://github.com/Mynoush).
