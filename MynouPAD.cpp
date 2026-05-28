/**
 * ======================================================================================
 * MYNOUPAD S3 OPERATING SYSTEM - EDI ANALYST PROFESSIONAL MASTER BUILD
 * ======================================================================================
 * Firmware Version   : 6.0 (Zero-Byte Fallback & Matrix Integrity)
 * Build Status       : Absolute Integrity Mode (High Availability)
 * * * --- INFRASTRUCTURE DOCUMENTATION ---
 * Hardware: ESP32-S3 Dual Core @ 240MHz
 * Flash: 16MB (Dedicated 9.9MB FFat partition for DuckyScripts)
 * USB: TinyUSB Stack (HID + CDC) for simultaneous keyboard and media emulation.
 * Display: SSD1306 OLED (128x64) via I2C bus.
 * * * --- COMPILATION REQUIREMENTS (ARDUINO IDE) ---
 * 1. USB CDC On Boot: Enabled
 * 2. USB Mode: TinyUSB
 * 3. Partition Scheme: 16M Flash (3MB APP / 9.9MB FATFS)
 * * * --- CORRECTIONS IN THIS VERSION ---
 * 1. Macro Fallback: If the file exists but has 0 bytes, sends the default key.
 * 2. Matrix Row Swap: {7, 6, 5} to fix physical inversion (2 triggering 8).
 * 3. Numpad Logic: 7-8-9, 4-5-6, 1-2-3 mapping restored.
 * ======================================================================================
 */

// --------------------------------------------------------------------------------------
// --- SECTION 1: SYSTEM LIBRARIES AND DEPENDENCIES ---
// --------------------------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>
#include <Adafruit_NeoPixel.h>
#include "USB.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDKeyboard.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <FFat.h> 
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include "secrets.h"

// --------------------------------------------------------------------------------------
// --- SECTION 2: HARDWARE DEFINITIONS AND PIN MAPPING (S3) ---
// --------------------------------------------------------------------------------------

#define PIN_ENC_CLK  1
#define PIN_ENC_DT   2
#define PIN_ENC_SW   4 

#define PIN_I2C_SDA  8
#define PIN_I2C_SCL  9

#define PIN_NEO_RGB 48 

#define OLED_W       128
#define OLED_H       64
#define OLED_RST      -1 
#define OLED_I2C_ADDR 0x3C
#define PIN_BUZZER 10
#define PIN_MOTOR 18

// --------------------------------------------------------------------------------------
// --- SECTION 3: GLOBAL VARIABLES AND OBJECT INSTANCES ---
// --------------------------------------------------------------------------------------

Adafruit_NeoPixel pixels(1, PIN_NEO_RGB, NEO_GRB + NEO_KHZ800);
USBHIDConsumerControl ConsumerControl;
USBHIDKeyboard Keyboard;
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RST);
ESP32Encoder encoder;
AsyncWebServer server(80);

const char* wifi_ssid     = SECRET_WIFI_SSID; 
const char* wifi_password = SECRET_WIFI_PASSWORD; 

bool wifiConnected        = false;
bool ffAtMounted          = false;
bool mDNSStarted          = false;

int volumeAtual           = 0;
bool isMuteActive         = false;
bool pythonBridgeActive   = false; 
int vuBarsLevel           = 0;
int frameCounter          = 0;

String traceMacroFile   = "System Idle";
String traceMacroResult = "Ready";
unsigned long traceTimer = 0;

// Physical row correction for 7-8-9 (Top) and 1-2-3 (Base) layout
const int pins_matrix_rows[3] = {7, 6, 5};
const int pins_matrix_cols[3] = {15, 16, 17};

const uint8_t logicalMap[3][3] = {
    { '7', '8', '9' }, 
    { '4', '5', '6' }, 
    { '1', '2', '3' }
};

bool state_lastKeys[3][3] = { {false, false, false}, {false, false, false}, {false, false, false} };
unsigned long time_lastDebounce[3][3] = { {0, 0, 0}, {0, 0, 0}, {0, 0, 0} };
const int DEBOUNCE_MS = 35; 

long lastEncoderValue     = 0;
bool isComboModeActive    = false; 
bool knobStateHistory     = false;

unsigned long timer_ledFlash = 0;
unsigned long timer_volFlash = 0;
unsigned long timer_vibration = 0;
int state_vibration = 0; 
uint8_t rgb_R = 0, rgb_G = 0, rgb_B = 255;
int breathIntensity = 0;
int breathDirection = 15;

int attempts_autoStart    = 0;
const int MAX_RETRY_AUTO  = 3; 
const unsigned long BOOT_DELAY_HID   = 4500;
const unsigned long RETRY_DELAY_HID  = 20000;
unsigned long bootTimerTimestamp     = 0;
unsigned long lastRetryTimestamp     = 0;

// --- OLED MENU AND LONG PRESS VARIABLES ---
bool isMenuMode = false;
int menuSelectedIndex = 0;
int menuScrollOffset = 0;
String menuItems[18]; // Capacity for 18 files (9 macros + 9 combos)
int menuCount = 0;
unsigned long menuTimeoutTimer = 0;
bool ignoreNextKnobRelease = false; // Prevents triggering action on knob release
unsigned long timer_knobPress = 0;         // Moment of the click
bool isKnobBeingLongPressed = false;       // Prevents multiple triggers
const int KNOB_LONG_PRESS_MS = 600;        // Long press duration (600ms)

// --------------------------------------------------------------------------------------
// --- SECTION 4: ADMINISTRATIVE DASHBOARD (COMPLETE INDUSTRIAL CSS) ---
// --------------------------------------------------------------------------------------

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>MynouPAD Master Console v6.0</title>
    <style>
        * { box-sizing: border-box; transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1); }
        body { background-color: #0c0c0e; color: #e0e0e0; font-family: 'Segoe UI', sans-serif; margin: 0; display: flex; flex-direction: column; align-items: center; min-height: 100vh; padding: 45px; }
        .main-card { max-width: 750px; width: 100%%; background: #16161a; padding: 55px; border-radius: 20px; box-shadow: 0 50px 100px rgba(0,0,0,0.9); border: 1px solid #2d2d35; position: relative; overflow: hidden; }
        .main-card::before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 6px; background: linear-gradient(90deg, #0078d7, #ff9900); }
        h2 { color: #0078d7; border-bottom: 2px solid #222; padding-bottom: 30px; font-weight: 300; text-transform: uppercase; letter-spacing: 5px; margin: 0 0 50px 0; text-align: center; }
        .label-section { font-size: 0.85em; text-transform: uppercase; color: #555; letter-spacing: 2px; font-weight: 800; margin: 45px 0 20px 0; display: block; }
        .explorer-box { background: #0d0d0f; padding: 30px; border-radius: 12px; border-left: 10px solid #0078d7; margin-bottom: 45px; max-height: 350px; overflow-y: auto; box-shadow: inset 0 10px 40px rgba(0,0,0,0.9); }
        .file-item { font-family: 'Consolas', monospace; font-size: 1.1em; padding: 16px 0; border-bottom: 1px solid #1e1e24; display: flex; justify-content: space-between; align-items: center; }
        .name-macro { color: #00ff66; font-weight: bold; } .name-combo { color: #ff9900; font-weight: bold; }
        .size-badge { background: #22222b; color: #777; padding: 5px 18px; border-radius: 15px; font-size: 0.85em; border: 1px solid #333; }
        .form-row { margin-bottom: 40px; }
        .input-ui { width: 100%%; border-radius: 10px; background: #1e1e24; color: #fff; border: 1px solid #333; padding: 22px; font-size: 1.15em; outline: none; }
        textarea.ducky-editor { height: 380px; font-family: 'Consolas', monospace; color: #00ff66; line-height: 1.8; background: #09090b; border-left: 5px solid #00ff66; }
        .btn-burn { width: 100%%; background: #0078d7; color: white; border: none; padding: 30px; border-radius: 15px; font-weight: 800; margin-top: 60px; cursor: pointer; text-transform: uppercase; letter-spacing: 4px; font-size: 1.4em; }
        #sync-overlay { display: none; color: #0078d7; font-size: 1.1em; margin-top: 25px; text-align: center; font-weight: bold; }
    </style>
    <script>
        async function syncWithFlash() {
            const btn = document.getElementById('btn_sel').value;
            const combo = document.getElementById('combo_ck').checked;
            const editor = document.getElementById('editor');
            const status = document.getElementById('sync-overlay');
            status.style.display = 'block';
            try {
                const url = `/get_macro?btn=${btn}&mode=${combo ? 'combo' : 'macro'}`;
                const res = await fetch(url);
                if (res.ok) editor.value = await res.text();
                else { editor.value = ""; editor.placeholder = "Sem comandos vinculados."; }
            } catch (e) { editor.placeholder = "Erro no S3."; }
            finally { status.style.display = 'none'; }
        }
        window.onload = syncWithFlash;
    </script>
</head>
<body>
    <div class="main-card">
        <h2>MynouPAD Admin v6.0</h2>
        <span class="label-section">FFat Partition (9.9MB)</span>
        <div class="explorer-box">%MACROLIST%</div>
        <form action="/save" method="POST">
            <span class="label-section">Mapping Configuration</span>
            <div class="form-row">
                <select name="btn" id="btn_sel" class="input-ui" onchange="syncWithFlash()">
                    <option value="7">Tecla 7</option><option value="8">Tecla 8</option><option value="9">Tecla 9</option>
                    <option value="4">Tecla 4</option><option value="5">Tecla 5</option><option value="6">Tecla 6</option>
                    <option value="1">Tecla 1</option><option value="2">Tecla 2</option><option value="3">Tecla 3</option>
                </select>
                <div style="margin-top:25px;">
                    <input type="checkbox" name="isCombo" id="combo_ck" value="true" onchange="syncWithFlash()">
                    <label for="combo_ck" style="color:#ff9900; font-weight:bold; cursor:pointer; text-transform:uppercase; letter-spacing:1px;"> Ativar Modo Combo (Knob)</label>
                </div>
            </div>
            <div id="sync-overlay">Sincronizando Barramento de Dados...</div>
            <span class="label-section">DuckyScript Editor</span>
            <textarea name="script" id="editor" class="input-ui ducky-editor"></textarea>
            <input type="submit" value="Fazer Upload para Flash" class="btn-burn">
        </form>
    </div>
</body>
</html>
)rawliteral";

// --------------------------------------------------------------------------------------
// --- SECTION 5: WEB SERVER TEMPLATE PROCESSING ---
// --------------------------------------------------------------------------------------

String processor(const String& var) {
    if (var == "MACROLIST") {
        String htmlPayload = "";
        File rootDir = FFat.open("/");
        if (!rootDir) return "FFat Error";
        File fileEntry = rootDir.openNextFile();
        while (fileEntry) {
            String fName = String(fileEntry.name());
            
            // New Rule: Only add to the list if size is greater than 0
            if ((fName.indexOf("macro_") != -1 || fName.indexOf("combo_") != -1) && fileEntry.size() > 0) {
                bool isC = fName.indexOf("combo_") != -1;
                htmlPayload += "<div class='file-item'><span class='" + String(isC ? "name-combo" : "name-macro") + "'>" + fName + "</span>";
                htmlPayload += "<span class='size-badge'>" + String(fileEntry.size()) + " b</span></div>";
            }
            fileEntry = rootDir.openNextFile();
        }
        return htmlPayload;
    }
    return String();
}

// --------------------------------------------------------------------------------------
// --- SECTION 6: DUCKYSCRIPT ENGINE ---
// --------------------------------------------------------------------------------------

void processarLinhaDucky(String linha) {
    linha.trim();
    if (linha.length() == 0 || linha.startsWith("REM ")) return;

    if (linha.startsWith("HA_TOGGLE ")) {
        String entity = linha.substring(10);
        entity.trim();
        
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin("http://192.168.3.3:8123/api/services/homeassistant/toggle"); 
            
            // Remember to insert your new token if you have revoked the previous one
            http.addHeader("Authorization", SECRET_HA_TOKEN);
            http.addHeader("Content-Type", "application/json");
            
            String payload = "{\"entity_id\": \"" + entity + "\"}";
            int httpResponseCode = http.POST(payload);
            
            http.end();
        }
        return; 
    }

    if (linha.startsWith("STRING ")) Keyboard.print(linha.substring(7));
    else if (linha.startsWith("DELAY ")) delay(linha.substring(6).toInt());
    else if (linha == "ENTER") Keyboard.write(KEY_RETURN);
    else if (linha == "TAB")   Keyboard.write(KEY_TAB);
    else if (linha == "ESC")   Keyboard.write(KEY_ESC);
    else if (linha.startsWith("GUI ") || linha == "GUI") {
        Keyboard.press(KEY_LEFT_GUI);
        if (linha.length() > 4) Keyboard.press(linha.substring(linha.lastIndexOf(' ') + 1)[0]);
        delay(85); Keyboard.releaseAll();
    }
    else if (linha.startsWith("CTRL ") || linha.startsWith("CONTROL ")) {
        Keyboard.press(KEY_LEFT_CTRL);
        if (linha.length() > 5) Keyboard.press(linha.substring(linha.lastIndexOf(' ') + 1)[0]);
        delay(85); Keyboard.releaseAll();
    }
}

// The function is now boolean and integrates size validation
bool executarArquivoDucky(String fileName) {
    String fullPath = fileName.startsWith("/") ? fileName : "/" + fileName;
    File scriptFile = FFat.open(fullPath, "r");
    
    // Opens only once and ensures there is content
    if (!scriptFile || scriptFile.size() == 0) {
        if (scriptFile) scriptFile.close();
        traceMacroResult = "NOT FOUND/EMPTY"; traceMacroFile = fullPath; traceTimer = millis();
        return false;
    }
    
    traceMacroResult = "EXECUTING..."; traceMacroFile = fullPath; traceTimer = millis();
    while (scriptFile.available()) {
        processarLinhaDucky(scriptFile.readStringUntil('\n'));
    }
    scriptFile.close();
    traceMacroResult = "SUCCESS";
    return true;
}

// --------------------------------------------------------------------------------------
// --- SECTION 7: OLED GRAPHIC INTERFACE (STARTUP & TELEMETRY) ---
// --------------------------------------------------------------------------------------

void drawFace(bool isBlinking) {
    display.clearDisplay();
    if (isBlinking) display.fillRect(41, 24, 12, 2, SSD1306_WHITE); 
    else            display.fillRect(41, 19, 12, 12, SSD1306_WHITE); 
    display.fillRect(74, 19, 12, 12, SSD1306_WHITE); 
    display.fillRect(52, 48, 25, 4, SSD1306_WHITE);
    for(int i = 0; i < 4; i++) {
        display.drawLine(43 + i, 40, 52 + i, 51, SSD1306_WHITE);
        display.drawLine(85 - i, 40, 76 - i, 51, SSD1306_WHITE);
    }
    display.display();
}

void showSplash() {
    display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
    String b = "MynouPAD"; display.setTextSize(2);
    for(int i = 0; i <= b.length(); i++) {
        display.clearDisplay(); display.setCursor(18, 25);
        display.print(b.substring(0, i)); display.display(); delay(125);
    }
    delay(600); drawFace(false); delay(1000); drawFace(true); delay(220); drawFace(false); delay(800);
}

void drawHeader() {
    display.setTextSize(1); 
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 3); 
    display.print("MynouPAD");

    // Wi-Fi Status (Side Icon - 1 Quadrant only)
    if (wifiConnected) { 
        int x = 120; // Horizontal origin
        int y = 9; // Vertical origin (base)
        
        // Draw only quadrant 1 (Top-Right) for the "fan" look
        display.drawCircleHelper(x, y, 4, 1, SSD1306_WHITE); 
        display.drawCircleHelper(x, y, 7, 1, SSD1306_WHITE);  
        display.drawPixel(x, y, SSD1306_WHITE);            
    }

    // Animated Scanner Line and Divider
    display.drawFastHLine((frameCounter * 3) % 128, 13, 15, SSD1306_WHITE);
    display.drawFastHLine(0, 15, 128, SSD1306_WHITE);
}

// --- NAME TRANSLATION FUNCTION ---
String traduzirNome(String fileName) {
    // Cleans the file name to make comparison easier
    fileName.replace(".txt", "");
    if (fileName.startsWith("/")) fileName = fileName.substring(1);

    // Name Dictionary (Add yours here)
    if (fileName == "combo_8") return "AC Escritorio";
    if (fileName == "combo_1") return "Toggle Python";
    if (fileName == "combo_9") return "Toggle Monitor";
    // ... continue adding as needed ...

    // Fallback: if no translation, show clean original name
    return fileName;
}

void atualizaDisplay() {
    display.clearDisplay(); 
    drawHeader();
    
    // The traceTimer > 0 lock prevents false screens during board boot
    if (traceTimer > 0 && millis() - traceTimer < 3000) {
        
        // Checks if macro was not found or is 0 bytes
        if (traceMacroResult == "NOT FOUND/EMPTY") {
            char keyDigit = ' ';
            
            // Scan file name to find the corresponding number
            for (int i = 0; i < traceMacroFile.length(); i++) {
                if (isDigit(traceMacroFile[i])) {
                    keyDigit = traceMacroFile[i];
                    break;
                }
            }
            
            // Draw the number in large size on the lower (blue) area of the OLED
            display.setTextSize(5);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(51, 24); // Coordinates to center the digit
            display.print(keyDigit);
            
        } else {
            // Find the friendly name in the dictionary for existing macros
            String friendlyName = traduzirNome(traceMacroFile);
            
            display.setCursor(0, 24); 
            display.print("Acao disparada:");
            
            // Create a solid white bar for emphasis
            display.fillRect(0, 35, 128, 15, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK); 
            display.setCursor(4, 39); 
            display.print(friendlyName);
            
            // Print status at the bottom of the screen
            display.setTextColor(SSD1306_WHITE); 
            display.setCursor(0, 54); 
            display.print("Status: ");
            display.print(traceMacroResult);
        }
    } 
    else if (!pythonBridgeActive) {
        display.setCursor(18, 28); display.print("Aguardando Python");
        int cX = 64, cY = 48;
        for (int i = 0; i < 8; i++) {
            float rad = i * (PI / 4);
            int px = cX + cos(rad) * 8, py = cY + sin(rad) * 8;
            if (i == ((frameCounter / 2) % 8)) display.fillCircle(px, py, 2, SSD1306_WHITE);
            else display.drawPixel(px, py, SSD1306_WHITE);
        }
    } 
    else {
        // Separate Mute logic from Volume logic
        if (isMuteActive) { 
            display.setTextSize(2); 
            display.setCursor(40, 32); // X axis locked in the exact center of the screen
            display.print("MUTE"); 
        }
        else { 
            int dCount = (volumeAtual < 10) ? 1 : (volumeAtual < 100) ? 2 : 3;
            int sX = (128 - (40 + (dCount * 18))) / 2;
            display.setTextSize(4); 
            display.setCursor(sX + 35, 24); 
            display.print(volumeAtual); 
            display.setTextSize(1); 
            display.print("%"); 
        }
        
        display.drawFastHLine(9, 60, 110, SSD1306_WHITE); 
        if (!isMuteActive && volumeAtual > 0) {
            display.fillRoundRect(9, 58, map(volumeAtual, 0, 100, 0, 110), 4, 1, SSD1306_WHITE);
        }
    }
    
    display.display(); 
}

void populateMenu() {
    menuCount = 0;
    File rootDir = FFat.open("/");
    if (!rootDir) return;
    File fileEntry = rootDir.openNextFile();
    while (fileEntry && menuCount < 18) {
        
        // New Rule: Ignores 0-byte files in OLED menu construction
        if (fileEntry.size() > 0) { 
            String fName = String(fileEntry.name());
            if (fName.indexOf("macro_") != -1 || fName.indexOf("combo_") != -1) {
                menuItems[menuCount++] = fName;
            }
        }
        fileEntry = rootDir.openNextFile();
    }
}

// --- UPDATED MENU (GREATER LEGIBILITY) ---
void drawMenu() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("--- MENU DE MACROS ---");

    if (menuCount == 0) {
        display.setCursor(10, 25);
        display.print("Nenhuma macro salva.");
        display.display();
        return;
    }

    // Reduced to 4 items to give more breathing room between lines
    int visibleItems = 4; 
    for (int i = 0; i < visibleItems; i++) {
        int itemIndex = menuScrollOffset + i;
        if (itemIndex >= menuCount) break;

        // Increase base spacing and line jump (from 10 to 13)
        int yPos = 14 + (i * 13); 
        
        // Higher and more comfortable selection box
        if (itemIndex == menuSelectedIndex) {
            display.fillRect(0, yPos - 1, 128, 13, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        
        // Center text vertically inside the new line
        display.setCursor(2, yPos + 2); 
        
        // Apply translation before rendering
        String displayName = traduzirNome(menuItems[itemIndex]);
        display.print(displayName);
    }
    display.display();
}

// --------------------------------------------------------------------------------------
// --- SECTION 8: INITIAL CONFIGURATION (SETUP) ---
// --------------------------------------------------------------------------------------

// --- HAPTIC FEEDBACK MANAGER (NON-BLOCKING) ---
void triggerHaptic(int pattern) {
    if (pattern == 1) { // 1. Normal Click (Short)
        digitalWrite(PIN_MOTOR, HIGH);
        timer_vibration = millis() + 85;
        state_vibration = 1; 
    }
    else if (pattern == 2) { // 2. Mute (Long)
        digitalWrite(PIN_MOTOR, HIGH);
        timer_vibration = millis() + 150;
        state_vibration = 1;
    }
    else if (pattern == 3) { // 3. Combo Macro (Double Fast)
        digitalWrite(PIN_MOTOR, HIGH);
        timer_vibration = millis() + 85;
        state_vibration = 2; // Send to state 2 (Pause)
    }
    else if (pattern == 4) { // 4. Fallback / Empty File (Long and Heavy)
        digitalWrite(PIN_MOTOR, HIGH);
        timer_vibration = millis() + 300; 
        state_vibration = 1;
    }
    else if (pattern == 5) { // 5. Knob Turn (Extra short and dry)
        digitalWrite(PIN_MOTOR, HIGH);
        timer_vibration = millis() + 25; 
        state_vibration = 1;
    }
}

void setup() {
    Serial.begin(115200); 
    ConsumerControl.begin(); Keyboard.begin(); USB.begin();
    pixels.begin(); pixels.setBrightness(15); pixels.show();
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL); display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
    
    showSplash();
    if (!FFat.begin(true)) Serial.println("FFat Error"); else ffAtMounted = true;

    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_MOTOR, OUTPUT);
    digitalWrite(PIN_MOTOR, LOW); 
    // Ensure the motor starts off

    // Startup sound: Tech Double Beep
    tone(PIN_BUZZER, 988); // B5
    delay(80);
    noTone(PIN_BUZZER); // Quick pause to separate notes
    delay(40);
    tone(PIN_BUZZER, 1319); // High E (E6)
    delay(150);
    noTone(PIN_BUZZER);

    WiFi.begin(wifi_ssid, wifi_password);
    unsigned long sW = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - sW < 8000) delay(500);

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true; MDNS.begin("mynoupad");
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send(200, "text/html", index_html, processor); });
        server.on("/get_macro", HTTP_GET, [](AsyncWebServerRequest *r){
            String b = r->getParam("btn")->value();
            bool isC = r->hasParam("mode") && r->getParam("mode")->value() == "combo";
            String p = isC ? "/combo_" + b + ".txt" : "/macro_" + b + ".txt";
            if(FFat.exists(p)) r->send(FFat, p, "text/plain"); else r->send(204);
        });
        server.on("/save", HTTP_POST, [](AsyncWebServerRequest *r){
            String b = r->getParam("btn", true)->value();
            bool isC = r->hasParam("isCombo", true);
            String p = isC ? "/combo_" + b + ".txt" : "/macro_" + b + ".txt";
            
            String scriptContent = r->getParam("script", true)->value();
            scriptContent.trim(); 

            if (scriptContent.length() == 0) {
                // If text is empty, delete the file from flash memory
                if (FFat.exists(p)) {
                    FFat.remove(p);
                }
            } else {
                // Otherwise, save the file with the content
                File f = FFat.open(p, "w"); 
                if(f){ 
                    f.print(scriptContent); 
                    f.close(); 
                }
            }
            r->redirect("/");
        });
        server.begin();
    }

    encoder.attachHalfQuad(PIN_ENC_DT, PIN_ENC_CLK);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);
    for (int i = 0; i < 3; i++) { pinMode(pins_matrix_cols[i], INPUT_PULLUP); pinMode(pins_matrix_rows[i], INPUT); }
    bootTimerTimestamp = millis();
}

// --------------------------------------------------------------------------------------
// --- SECTION 9: MAIN OPERATION LOOP ---
// --------------------------------------------------------------------------------------

bool readKnobStable() {
    int count = 0;
    for (int i = 0; i < 5; i++) { if (digitalRead(PIN_ENC_SW) == LOW) count++; delayMicroseconds(200); }
    return (count >= 3);
}

void loop() {
    if (!pythonBridgeActive && attempts_autoStart < MAX_RETRY_AUTO) {
        unsigned long now = millis();
        if (now - (attempts_autoStart == 0 ? bootTimerTimestamp : lastRetryTimestamp) > (attempts_autoStart == 0 ? 4500 : 20000)) {
            attempts_autoStart++; lastRetryTimestamp = now;
            Keyboard.press(KEY_LEFT_CTRL); Keyboard.press(KEY_LEFT_ALT); Keyboard.press(KEY_LEFT_SHIFT); Keyboard.press('m');
            delay(130); Keyboard.releaseAll();
        }
    }

    bool knob = readKnobStable();

    // --- KNOB LONG PRESS DETECTION ---
    if (knob && !knobStateHistory) {
        timer_knobPress = millis();
        isKnobBeingLongPressed = false;
    }

    if (knob && !isKnobBeingLongPressed && !isComboModeActive && (millis() - timer_knobPress > KNOB_LONG_PRESS_MS)) {
        isKnobBeingLongPressed = true;
        ignoreNextKnobRelease = true; 
        
        if (!isMenuMode) {
            isMenuMode = true;
            populateMenu();
            menuSelectedIndex = 0;
            menuScrollOffset = 0;
            menuTimeoutTimer = millis();
            triggerHaptic(3); 
        } else {
            isMenuMode = false;
            triggerHaptic(2);
        }
    }
    // ------------------------------------------

    for (int r = 0; r < 3; r++) {
        pinMode(pins_matrix_rows[r], OUTPUT); 
        digitalWrite(pins_matrix_rows[r], LOW); 
        
        for (int c = 0; c < 3; c++) {
            bool reading = (digitalRead(pins_matrix_cols[c]) == LOW); 
            
            // Debounce logic restored to prevent multiple triggers
            if (reading != state_lastKeys[r][c] && (millis() - time_lastDebounce[r][c] > DEBOUNCE_MS)) {
                
                time_lastDebounce[r][c] = millis(); 
                state_lastKeys[r][c] = reading;
                
                if (reading) {
                    
                    // Trigger haptic feedback immediately to simulate physical click
                    if (!knob) {
                        triggerHaptic(1);
                    }

                    char k = logicalMap[r][c]; 
                    String path = knob ? "/combo_" + String(k) + ".txt" : "/macro_" + String(k) + ".txt";
                    
                    if (knob) isComboModeActive = true;

                    // Execute and evaluate success
                    if (executarArquivoDucky(path)) {
                        tone(PIN_BUZZER, 600, 50);
                        tone(PIN_BUZZER, 400, 80); 
                        // If it's a combo via knob, overlay with double vibration
                        if (knob) triggerHaptic(3); 
                    } else {
                        Keyboard.press(k);
                        // Error vibration only if via knob, since normal key already vibrated
                        if (knob) triggerHaptic(4); 
                    }
                    timer_ledFlash = millis() + 120; 
                } else {
                    Keyboard.release(logicalMap[r][c]);
                }
            }
        }
        pinMode(pins_matrix_rows[r], INPUT); 
    }

    long p = encoder.getCount();
    if (p != lastEncoderValue) {
        if (isMenuMode) {
            if (p > lastEncoderValue) menuSelectedIndex++;
            else menuSelectedIndex--;

            if (menuSelectedIndex < 0) menuSelectedIndex = 0;
            if (menuSelectedIndex >= menuCount) menuSelectedIndex = menuCount - 1;

            if (menuSelectedIndex < menuScrollOffset) menuScrollOffset = menuSelectedIndex;
            if (menuSelectedIndex >= menuScrollOffset + 5) menuScrollOffset = menuSelectedIndex - 4;

            menuTimeoutTimer = millis(); 
            triggerHaptic(5); // <-- CHANGED TO 5
        } else {
            vuBarsLevel = 10; 
            if (p > lastEncoderValue) ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
            else ConsumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
            ConsumerControl.release(); 
            timer_volFlash = millis() + 200;
            triggerHaptic(5); // <-- CHANGED TO 5
        }
        lastEncoderValue = p; 
    }

    if (state_vibration > 0 && millis() > timer_vibration) {
        if (state_vibration == 1) { digitalWrite(PIN_MOTOR, LOW); state_vibration = 0; }
        else if (state_vibration == 2) { digitalWrite(PIN_MOTOR, LOW); timer_vibration = millis() + 60; state_vibration = 3; }
        else if (state_vibration == 3) { digitalWrite(PIN_MOTOR, HIGH); timer_vibration = millis() + 40; state_vibration = 1; }
    }
   
    if (knobStateHistory && !knob) {
        if (ignoreNextKnobRelease) {
            ignoreNextKnobRelease = false;
        }
        else if (isMenuMode) {
            if (menuCount > 0) {
                String path = "/" + menuItems[menuSelectedIndex];
                
                // Execute automation and evaluate success
                if (executarArquivoDucky(path)) {
                    
                    // Restore sound feedback
                    tone(PIN_BUZZER, 600, 50);
                    tone(PIN_BUZZER, 400, 80);
                    
                    // Haptic intelligence: double vibration for combo, simple for macro
                    if (path.indexOf("combo_") != -1) {
                        triggerHaptic(3);
                    } else {
                        triggerHaptic(1);
                    }
                } else {
                    // Failure feedback (empty or corrupted file)
                    triggerHaptic(4); 
                }
            }
            isMenuMode = false; 
        }
        else if (!isComboModeActive && !isKnobBeingLongPressed) {
            static unsigned long lastM = 0;
            if (millis() - lastM > 400) { 
                ConsumerControl.press(CONSUMER_CONTROL_MUTE); 
                ConsumerControl.release(); 
                lastM = millis(); 
                triggerHaptic(2); 
            }
        }
        knobStateHistory = false; isComboModeActive = false; 
    }
    knobStateHistory = knob;

    if (Serial.available() > 0) {
        String msg = Serial.readStringUntil('\n'); msg.trim();
        if (msg.startsWith("V")) {
            volumeAtual = msg.substring(1).toInt(); isMuteActive = (volumeAtual == -1);
            if (!pythonBridgeActive) { pythonBridgeActive = true; pixels.setPixelColor(0, 0, 255, 0); pixels.show(); }
            if (!isMenuMode) atualizaDisplay();
        }
    }

    if (isMenuMode && (millis() - menuTimeoutTimer > 5000)) {
        isMenuMode = false;
    }

    static unsigned long lT = 0;
    if (millis() - lT > 60) {
        frameCounter++; if (vuBarsLevel > 0) vuBarsLevel--; 
        if (millis() < timer_ledFlash) pixels.setPixelColor(0, 150, 255, 150);
        else if (pythonBridgeActive) {
            if (isMuteActive) pixels.setPixelColor(0, 255, 0, 0); else pixels.setPixelColor(0, 0, 180, 255);
        } else pixels.setPixelColor(0, 50, 50, 0);
        pixels.show(); 
        
        if (isMenuMode) {
            drawMenu();
        } else {
            atualizaDisplay(); 
        }
        lT = millis();
    }
}
