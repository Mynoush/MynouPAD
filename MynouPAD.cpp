/**
 * ======================================================================================
 * MYNOUPAD S3 — MASTER CONSOLE v7.0
 * ======================================================================================
 * Hardware: ESP32-S3 Dual Core @ 240MHz
 * Flash: 16MB (9.9MB FFat partition for DuckyScripts & labels)
 * USB: TinyUSB Stack (HID + CDC) for simultaneous keyboard and media emulation
 * Display: SSD1306 OLED (128x64) via I2C bus
 *
 * Compilation Requirements:
 * - USB CDC On Boot: Enabled
 * - USB Mode: TinyUSB
 * - Partition Scheme: 16M Flash (3MB APP / 9.9MB FATFS)
 * ======================================================================================
 */

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
#include <ArduinoJson.h>
#include <map>
#include "secrets.h"

// Hardware definitions and pin mapping
#define PIN_ENC_CLK  1
#define PIN_ENC_DT   2
#define PIN_ENC_SW   4

#define PIN_I2C_SDA  8
#define PIN_I2C_SCL  9

#define PIN_NEO_RGB  48

#define OLED_W       128
#define OLED_H       64
#define OLED_RST     -1
#define OLED_I2C_ADDR 0x3C
#define PIN_BUZZER   10
#define PIN_MOTOR    18

#define FIRMWARE_VERSION "7.1"
#ifndef SECRET_HA_URL
#define SECRET_HA_URL    "http://homeassistant.local:8123"
#endif
#define HA_BASE_URL      SECRET_HA_URL

// Global objects
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

int volumeAtual           = 0;
bool isMuteActive         = false;
bool pythonBridgeActive   = false;
int vuBarsLevel           = 0;
int frameCounter          = 0;

String traceMacroFile     = "System Idle";
String traceMacroResult   = "Ready";
String lastFriendlyMacro  = "Pronto";
unsigned long traceTimer  = 0;

// Pending actions queued from AsyncWebServer to run safely on loop()
String pendingTriggerPath = "";
String pendingActionCmd   = "";
bool pendingHaSync        = false;
unsigned long lastHaSyncMs = 0;
const unsigned long HA_SYNC_INTERVAL_MS = 30000;

// Friendly label cache (key = "macro_9" / "combo_9")
std::map<String, String> labelOverrides;

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

unsigned long timer_ledFlash  = 0;
unsigned long timer_volFlash  = 0;
unsigned long timer_vibration = 0;
int state_vibration           = 0;

int attempts_autoStart    = 0;
const int MAX_RETRY_AUTO  = 3;
unsigned long bootTimerTimestamp = 0;
unsigned long lastRetryTimestamp = 0;

// OLED Menu & Long Press
bool isMenuMode           = false;
int menuSelectedIndex     = 0;
int menuScrollOffset      = 0;
String menuItems[18];
int menuCount             = 0;
unsigned long menuTimeoutTimer = 0;
bool ignoreNextKnobRelease     = false;
unsigned long timer_knobPress  = 0;
bool isKnobBeingLongPressed    = false;
const int KNOB_LONG_PRESS_MS   = 600;

// --------------------------------------------------------------------------------------
// Official Mascot SVG (pixel-perfect match of drawFace() OLED boot animation)
// --------------------------------------------------------------------------------------
const char mascot_svg[] PROGMEM = R"rawliteral(<svg xmlns="http://www.w3.org/2000/svg" viewBox="27.5 -0.5 72 72" width="128" height="128"><rect x="28.5" y="0.5" width="70" height="70" rx="16" fill="#090b10" stroke="#232b3e" stroke-width="2"/><rect x="41" y="19" width="12" height="12" rx="1.5" fill="#38bdf8"/><rect x="74" y="19" width="12" height="12" rx="1.5" fill="#38bdf8"/><rect x="52" y="48" width="25" height="4" fill="#e6ecf5"/><polygon points="43,40 47,40 56,52 52,52" fill="#e6ecf5"/><polygon points="81,40 85,40 76,52 72,52" fill="#e6ecf5"/></svg>)rawliteral";

// --------------------------------------------------------------------------------------
// MynouPAD Studio v7.1 Web UI (Served directly without template escaping issues)
// --------------------------------------------------------------------------------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MynouPAD Studio v7.1</title>
<link rel="icon" type="image/svg+xml" href="/mascot.svg">
<style>
:root{
  --bg:#090b10;--panel:#11151f;--panel-2:#171d2b;--border:#232b3e;
  --text:#e6ecf5;--muted:#8795b0;--accent:#38bdf8;--accent-2:#818cf8;
  --ok:#34d399;--warn:#fbbf24;--err:#f87171;--combo:#fb923c;
  --radius:14px;--mono:'JetBrains Mono','Cascadia Code','Consolas',monospace;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:radial-gradient(1200px 600px at 15% -10%,rgba(56,189,248,.10),transparent),radial-gradient(900px 500px at 90% 0%,rgba(129,140,248,.10),transparent),var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,-apple-system,sans-serif;min-height:100vh;padding:24px}
.shell{max-width:1180px;margin:0 auto;display:flex;flex-direction:column;gap:18px}
.topbar{display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:14px;background:var(--panel);border:1px solid var(--border);border-radius:var(--radius);padding:16px 22px}
.brand{display:flex;align-items:center;gap:12px}
.mascot-logo{width:44px;height:44px;flex-shrink:0;filter:drop-shadow(0 0 10px rgba(56,189,248,.22))}
.mascot-logo .eye-l{transform-box:fill-box;transform-origin:center;animation:blinkEye 4.2s infinite}
@keyframes blinkEye{0%,45%,51%,100%{transform:scaleY(1)}48%{transform:scaleY(.16)}}
.brand h1{font-size:18px;font-weight:700;letter-spacing:.4px}
.brand small{display:block;color:var(--muted);font-size:12px}
.pills{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.pill{display:inline-flex;align-items:center;gap:7px;background:var(--panel-2);border:1px solid var(--border);border-radius:999px;padding:6px 12px;font-size:12px;color:var(--text)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--muted)}
.dot.ok{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.dot.warn{background:var(--warn);box-shadow:0 0 8px var(--warn)}
.ha-link{text-decoration:none;color:#08101d;background:linear-gradient(135deg,var(--accent),#0ea5e9);font-weight:700;border:none}
.grid-main{display:grid;grid-template-columns:420px 1fr;gap:18px}
@media(max-width:960px){.grid-main{grid-template-columns:1fr}}
.card{background:var(--panel);border:1px solid var(--border);border-radius:var(--radius);padding:20px;display:flex;flex-direction:column;gap:16px}
.card-title{display:flex;justify-content:space-between;align-items:center;font-size:13px;text-transform:uppercase;letter-spacing:1.3px;color:var(--muted);font-weight:700}
.mode-switch{display:grid;grid-template-columns:1fr 1fr;gap:8px;background:var(--bg);padding:5px;border-radius:11px;border:1px solid var(--border)}
.mode-btn{border:none;background:transparent;color:var(--muted);padding:9px;border-radius:8px;font-weight:700;font-size:12px;cursor:pointer;text-transform:uppercase;letter-spacing:.7px;transition:.15s}
.mode-btn.active-macro{background:rgba(56,189,248,.16);color:var(--accent);border:1px solid rgba(56,189,248,.35)}
.mode-btn.active-combo{background:rgba(251,146,60,.16);color:var(--combo);border:1px solid rgba(251,146,60,.35)}
.pad-wrap{display:flex;flex-direction:column;gap:12px}
.knob-row{display:flex;align-items:center;justify-content:space-between;gap:12px;background:var(--panel-2);border:1px solid var(--border);border-radius:12px;padding:12px 14px}
.knob-dial{width:44px;height:44px;border-radius:50%;background:radial-gradient(circle at 35% 35%,#2b364d,#121722);border:2px solid #33415c;display:grid;place-items:center;font-size:11px;font-weight:700;color:var(--accent);flex-shrink:0}
.media-btns{display:flex;gap:6px}
.mini-btn{background:var(--bg);border:1px solid var(--border);color:var(--text);padding:7px 10px;border-radius:8px;font-size:12px;font-weight:600;cursor:pointer}
.mini-btn:hover{border-color:var(--accent);color:var(--accent)}
.matrix{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.keycap{position:relative;background:linear-gradient(180deg,#1a2130,#121722);border:1px solid var(--border);border-radius:12px;padding:12px 10px;min-height:88px;display:flex;flex-direction:column;justify-content:space-between;cursor:pointer;text-align:left;color:var(--text);transition:.15s}
.keycap:hover{border-color:var(--accent);transform:translateY(-1px)}
.keycap.selected{border-color:var(--accent);box-shadow:0 0 0 2px rgba(56,189,248,.22),inset 0 0 18px rgba(56,189,248,.08)}
.keycap.combo-mode.selected{border-color:var(--combo);box-shadow:0 0 0 2px rgba(251,146,60,.22),inset 0 0 18px rgba(251,146,60,.08)}
.keycap.has-file::after{content:'';position:absolute;top:10px;right:10px;width:7px;height:7px;border-radius:50%;background:var(--ok)}
.keycap.combo-mode.has-file::after{background:var(--combo)}
.key-num{font-family:var(--mono);font-size:11px;color:var(--muted);font-weight:700}
.key-lbl{font-size:12px;font-weight:600;line-height:1.25;word-break:break-word;margin-top:4px}
.key-sz{font-family:var(--mono);font-size:10px;color:var(--muted);margin-top:6px}
.field{display:flex;flex-direction:column;gap:6px}
.field label{font-size:12px;color:var(--muted);font-weight:600}
.input{background:var(--bg);border:1px solid var(--border);color:var(--text);border-radius:10px;padding:11px 13px;font-size:14px;outline:none;width:100%}
.input:focus,textarea:focus{border-color:var(--accent)}
.chips{display:flex;flex-wrap:wrap;gap:6px}
.chip{background:var(--panel-2);border:1px solid var(--border);color:var(--muted);padding:5px 9px;border-radius:7px;font-family:var(--mono);font-size:11px;cursor:pointer}
.chip:hover{color:var(--accent);border-color:var(--accent)}
textarea.editor{width:100%;min-height:240px;background:#06080c;border:1px solid var(--border);border-left:4px solid var(--accent);border-radius:10px;padding:14px;color:#7dd3fc;font-family:var(--mono);font-size:13.5px;line-height:1.6;resize:vertical;outline:none}
.actions{display:flex;flex-wrap:wrap;gap:10px}
.btn{border:none;border-radius:10px;padding:12px 18px;font-weight:700;font-size:13px;cursor:pointer;display:inline-flex;align-items:center;gap:8px;transition:.15s}
.btn-primary{background:linear-gradient(135deg,var(--accent),#0284c7);color:#04111d;flex:1;justify-content:center}
.btn-run{background:rgba(52,211,153,.15);color:var(--ok);border:1px solid rgba(52,211,153,.35)}
.btn-del{background:rgba(248,113,113,.12);color:var(--err);border:1px solid rgba(248,113,113,.3)}
.btn:hover{filter:brightness(1.1)}
.statusbar{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}
@media(max-width:800px){.statusbar{grid-template-columns:repeat(2,1fr)}}
.stat{background:var(--panel);border:1px solid var(--border);border-radius:12px;padding:12px 15px}
.stat small{display:block;color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:.7px}
.stat strong{display:block;margin-top:4px;font-size:15px;font-family:var(--mono)}
.toast{position:fixed;bottom:20px;right:20px;background:var(--panel-2);border:1px solid var(--accent);color:var(--text);padding:11px 16px;border-radius:10px;font-size:13px;font-weight:600;opacity:0;transform:translateY(10px);transition:.2s;pointer-events:none}
.toast.show{opacity:1;transform:translateY(0)}
</style>
</head>
<body>
<div class="shell">
  <header class="topbar">
    <div class="brand">
      <svg class="mascot-logo" viewBox="27.5 -0.5 72 72" aria-label="MynouPAD Mascot">
        <rect x="28.5" y="0.5" width="70" height="70" rx="16" fill="#090b10" stroke="#232b3e" stroke-width="2"/>
        <rect class="eye-l" x="41" y="19" width="12" height="12" rx="1.5" fill="#38bdf8"/>
        <rect x="74" y="19" width="12" height="12" rx="1.5" fill="#38bdf8"/>
        <rect x="52" y="48" width="25" height="4" fill="#e6ecf5"/>
        <polygon points="43,40 47,40 56,52 52,52" fill="#e6ecf5"/>
        <polygon points="81,40 85,40 76,52 72,52" fill="#e6ecf5"/>
      </svg>
      <div>
        <h1>MynouPAD Studio <span style="color:var(--accent)">v7.1</span></h1>
        <small id="subline">ESP32-S3 TinyUSB HID + Home Assistant Console</small>
      </div>
    </div>
    <div class="pills">
      <span class="pill"><span class="dot" id="dot-wifi"></span><span id="pill-wifi">Wi-Fi...</span></span>
      <span class="pill"><span class="dot" id="dot-py"></span><span id="pill-py">Python Bridge...</span></span>
      <span class="pill" id="pill-vol">Vol: --%</span>
      <a class="pill ha-link" id="ha-link" href="#" target="_blank">Home Assistant ↗</a>
    </div>
  </header>

  <section class="statusbar">
    <div class="stat"><small>Última Ação</small><strong id="st-macro">--</strong></div>
    <div class="stat"><small>Resultado</small><strong id="st-result">--</strong></div>
    <div class="stat"><small>Flash FFat</small><strong id="st-ffat">--</strong></div>
    <div class="stat"><small>Uptime</small><strong id="st-up">--</strong></div>
  </section>

  <main class="grid-main">
    <section class="card">
      <div class="card-title">
        <span>Matriz Física 3×3</span>
        <span id="mode-badge" style="color:var(--accent)">MACRO NORMAL</span>
      </div>
      <div class="mode-switch">
        <button type="button" id="m-macro" class="mode-btn active-macro" onclick="setMode('macro')">Normal</button>
        <button type="button" id="m-combo" class="mode-btn" onclick="setMode('combo')">Combo (Knob)</button>
      </div>
      <div class="knob-row">
        <div style="display:flex;align-items:center;gap:10px">
          <div class="knob-dial" id="knob-vol">--%</div>
          <div>
            <div style="font-size:13px;font-weight:700">Rotary Encoder</div>
            <div style="font-size:11px;color:var(--muted)">Volume / Mute / Long-press Menu</div>
          </div>
        </div>
        <div class="media-btns">
          <button type="button" class="mini-btn" onclick="sendAction('vol_down')">Vol-</button>
          <button type="button" class="mini-btn" onclick="sendAction('mute')">Mute</button>
          <button type="button" class="mini-btn" onclick="sendAction('vol_up')">Vol+</button>
          <button type="button" class="mini-btn" onclick="sendAction('beep')">Beep</button>
        </div>
      </div>
      <div class="pad-wrap">
        <div class="matrix" id="matrix"></div>
      </div>
    </section>

    <section class="card">
      <div class="card-title">
        <span id="editor-title">Editor • Tecla 9 (Macro)</span>
        <span id="file-badge" style="font-family:var(--mono);font-size:11px">/macro_9.txt</span>
      </div>
      <div class="field">
        <label for="label-input">Nome Amigável (OLED + Painel)</label>
        <input id="label-input" class="input" maxlength="20" placeholder="Ex: Alternar Monitor">
      </div>
      <div class="field">
        <label>Inserir Comando Rápido</label>
        <div class="chips">
          <button type="button" class="chip" onclick="ins('CTRL ALT F12')">CTRL ALT F12</button>
          <button type="button" class="chip" onclick="ins('GUI r')">GUI r</button>
          <button type="button" class="chip" onclick="ins('HA_TOGGLE light.desk_lamp')">HA_TOGGLE</button>
          <button type="button" class="chip" onclick="ins('HA_SERVICE scene.turn_on scene.work')">HA_SERVICE</button>
          <button type="button" class="chip" onclick="ins('DELAY 150')">DELAY 150</button>
          <button type="button" class="chip" onclick="ins('STRING ')">STRING</button>
          <button type="button" class="chip" onclick="ins('ENTER')">ENTER</button>
        </div>
      </div>
      <div class="field">
        <label for="editor">Script DuckyScript</label>
        <textarea id="editor" class="editor" spellcheck="false" placeholder="Digite os comandos (1 por linha)...&#10;Ex: CTRL ALT F12"></textarea>
      </div>
      <div class="actions">
        <button type="button" class="btn btn-primary" onclick="saveCurrent()">Salvar na Flash</button>
        <button type="button" class="btn btn-run" onclick="triggerCurrent()">▶ Testar Agora</button>
        <button type="button" class="btn btn-del" onclick="clearCurrent()">Limpar</button>
      </div>
    </section>
  </main>
</div>
<div class="toast" id="toast"></div>
<script>
const KEYS=['7','8','9','4','5','6','1','2','3'];
let curBtn='9', curMode='macro', statusData={macros:{},labels:{}};

function toast(msg){
  const t=document.getElementById('toast');
  t.textContent=msg; t.classList.add('show');
  setTimeout(()=>t.classList.remove('show'),2200);
}
function fmtUptime(s){
  const h=Math.floor(s/3600), m=Math.floor((s%3600)/60), sec=s%60;
  return h>0 ? `${h}h ${m}m` : `${m}m ${sec}s`;
}
function renderMatrix(){
  const box=document.getElementById('matrix');
  box.innerHTML='';
  KEYS.forEach(k=>{
    const id=`${curMode}_${k}`;
    const sz=(statusData.macros||{})[id]||0;
    const lbl=(statusData.labels||{})[id]||(sz>0?id:'Livre');
    const b=document.createElement('button');
    b.type='button';
    b.className=`keycap ${curMode==='combo'?'combo-mode':''} ${k===curBtn?'selected':''} ${sz>0?'has-file':''}`;
    b.onclick=()=>selectKey(k);
    b.innerHTML=`<div class="key-num">TECLA ${k}</div><div class="key-lbl">${lbl}</div><div class="key-sz">${sz>0?sz+' B':'vazio'}</div>`;
    box.appendChild(b);
  });
}
async function fetchStatus(){
  try{
    const r=await fetch('/api/status');
    if(!r.ok) return;
    statusData=await r.json();
    if(statusData.ha_url) document.getElementById('ha-link').href=statusData.ha_url;
    document.getElementById('dot-wifi').className='dot ok';
    document.getElementById('pill-wifi').textContent=`${statusData.ip} (${statusData.rssi} dBm)`;
    document.getElementById('dot-py').className='dot '+(statusData.python_bridge?'ok':'warn');
    document.getElementById('pill-py').textContent=statusData.python_bridge?'Python Bridge ON':'Aguardando Bridge';
    const vText=statusData.muted?'MUTE':`${statusData.volume}%`;
    document.getElementById('pill-vol').textContent=`Vol: ${vText}`;
    document.getElementById('knob-vol').textContent=vText;
    document.getElementById('st-macro').textContent=statusData.last_macro||'--';
    document.getElementById('st-result').textContent=statusData.last_result||'--';
    const kb=((statusData.ffat_used||0)/1024).toFixed(1), totMb=((statusData.ffat_total||0)/1048576).toFixed(1);
    document.getElementById('st-ffat').textContent=`${kb} KB / ${totMb} MB`;
    document.getElementById('st-up').textContent=fmtUptime(statusData.uptime_s||0);
    renderMatrix();
  }catch(e){}
}
async function loadEditor(){
  const id=`${curMode}_${curBtn}`;
  document.getElementById('editor-title').textContent=`Editor • Tecla ${curBtn} (${curMode==='combo'?'Combo':'Macro'})`;
  document.getElementById('file-badge').textContent=`/${id}.txt`;
  document.getElementById('label-input').value=(statusData.labels||{})[id]||'';
  const ed=document.getElementById('editor');
  try{
    const r=await fetch(`/get_macro?btn=${curBtn}&mode=${curMode}`);
    ed.value=(r.status===200)?await r.text():'';
  }catch(e){ ed.value=''; }
}
function selectKey(k){ curBtn=k; renderMatrix(); loadEditor(); }
function setMode(m){
  curMode=m;
  document.getElementById('m-macro').className='mode-btn '+(m==='macro'?'active-macro':'');
  document.getElementById('m-combo').className='mode-btn '+(m==='combo'?'active-combo':'');
  const mb=document.getElementById('mode-badge');
  mb.textContent=m==='combo'?'MODO COMBO (KNOB)':'MACRO NORMAL';
  mb.style.color=m==='combo'?'var(--combo)':'var(--accent)';
  renderMatrix(); loadEditor();
}
function ins(txt){
  const ed=document.getElementById('editor');
  const val=ed.value.trim();
  ed.value=val ? val+'\n'+txt : txt;
  ed.focus();
}
async function saveCurrent(){
  const script=document.getElementById('editor').value;
  const label=document.getElementById('label-input').value.trim();
  const body=new URLSearchParams({btn:curBtn,script});
  if(curMode==='combo') body.append('isCombo','true');
  await fetch('/save',{method:'POST',body});
  await fetch('/api/labels',{method:'POST',body:new URLSearchParams({key:`${curMode}_${curBtn}`,label})});
  await fetchStatus();
  toast(`Salvo: ${curMode}_${curBtn}`);
}
async function clearCurrent(){
  document.getElementById('editor').value='';
  await saveCurrent();
}
async function triggerCurrent(){
  const r=await fetch(`/api/trigger?btn=${curBtn}&mode=${curMode}`);
  if(r.ok) toast(`Executado: ${curMode}_${curBtn}`);
  setTimeout(fetchStatus,300);
}
async function sendAction(cmd){
  await fetch(`/api/action?cmd=${cmd}`);
  setTimeout(fetchStatus,250);
}
window.onload=async()=>{ await fetchStatus(); await loadEditor(); setInterval(fetchStatus,3000); };
</script>
</body>
</html>
)rawliteral";

// --------------------------------------------------------------------------------------
// Friendly Labels & Flash Persistence
// --------------------------------------------------------------------------------------
void loadLabelsFromFFat() {
    labelOverrides.clear();
    if (!ffAtMounted || !FFat.exists("/labels.json")) return;
    File f = FFat.open("/labels.json", "r");
    if (!f) return;
    JsonDocument doc;
    if (!deserializeJson(doc, f)) {
        for (JsonPair kv : doc.as<JsonObject>()) {
            String val = kv.value().as<String>();
            val.trim();
            if (val.length() > 0) labelOverrides[String(kv.key().c_str())] = val;
        }
    }
    f.close();
}

void saveLabelsToFFat() {
    if (!ffAtMounted) return;
    JsonDocument doc;
    for (const auto& kv : labelOverrides) {
        doc[kv.first] = kv.second;
    }
    File f = FFat.open("/labels.json", "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
}

String traduzirNome(String fileName) {
    fileName.replace(".txt", "");
    if (fileName.startsWith("/")) fileName = fileName.substring(1);

    auto it = labelOverrides.find(fileName);
    if (it != labelOverrides.end() && it->second.length() > 0) {
        return it->second;
    }

    if (fileName == "combo_8") return "AC Escritorio";
    if (fileName == "combo_1") return "Bridge Python";
    if (fileName == "macro_9" || fileName == "combo_9") return "Alternar Monitor";
    return fileName;
}

// --------------------------------------------------------------------------------------
// Multi-Modifier DuckyScript Engine
// --------------------------------------------------------------------------------------
uint8_t mapSpecialKey(const String& tok) {
    if (tok == "ENTER" || tok == "RETURN") return KEY_RETURN;
    if (tok == "TAB")   return KEY_TAB;
    if (tok == "ESC" || tok == "ESCAPE") return KEY_ESC;
    if (tok == "SPACE") return KEY_SPACE;
    if (tok == "BACKSPACE" || tok == "BKSP") return KEY_BACKSPACE;
    if (tok == "DELETE" || tok == "DEL") return KEY_DELETE;
    if (tok == "INSERT" || tok == "INS") return KEY_INSERT;
    if (tok == "HOME")  return KEY_HOME;
    if (tok == "END")   return KEY_END;
    if (tok == "PAGEUP" || tok == "PGUP") return KEY_PAGE_UP;
    if (tok == "PAGEDOWN" || tok == "PGDN") return KEY_PAGE_DOWN;
    if (tok == "UP" || tok == "UPARROW") return KEY_UP_ARROW;
    if (tok == "DOWN" || tok == "DOWNARROW") return KEY_DOWN_ARROW;
    if (tok == "LEFT" || tok == "LEFTARROW") return KEY_LEFT_ARROW;
    if (tok == "RIGHT" || tok == "RIGHTARROW") return KEY_RIGHT_ARROW;
    if (tok == "PRINTSCREEN" || tok == "PRTSC") return KEY_PRINT_SCREEN;
    if (tok == "PAUSE" || tok == "BREAK") return KEY_PAUSE;
    if (tok == "CAPSLOCK") return KEY_CAPS_LOCK;
    if (tok == "NUMLOCK") return KEY_NUM_LOCK;
    if (tok == "SCROLLLOCK") return KEY_SCROLL_LOCK;

    if (tok.startsWith("F") && tok.length() >= 2 && tok.length() <= 3) {
        int fn = tok.substring(1).toInt();
        if (fn >= 1 && fn <= 12) return KEY_F1 + (fn - 1);
        if (fn >= 13 && fn <= 24) return KEY_F13 + (fn - 13);
    }
    return 0;
}

bool callHomeAssistantService(const String& servicePath, const String& entityId) {
    if (WiFi.status() != WL_CONNECTED) return false;
    HTTPClient http;
    http.setTimeout(1500);
    String url = String(HA_BASE_URL) + "/api/services/" + servicePath;
    if (!http.begin(url)) return false;
    http.addHeader("Authorization", SECRET_HA_TOKEN);
    http.addHeader("Content-Type", "application/json");
    String payload = entityId.length() > 0 ? ("{\"entity_id\": \"" + entityId + "\"}") : "{}";
    int code = http.POST(payload);
    http.end();
    return (code >= 200 && code < 300);
}

void processarLinhaDucky(String linha) {
    linha.trim();
    if (linha.length() == 0 || linha.startsWith("REM ")) return;

    if (linha.startsWith("HA_TOGGLE ")) {
        String entity = linha.substring(10);
        entity.trim();
        callHomeAssistantService("homeassistant/toggle", entity);
        return;
    }

    if (linha.startsWith("HA_SERVICE ")) {
        String rest = linha.substring(11);
        rest.trim();
        int sp = rest.indexOf(' ');
        String svc = (sp > 0) ? rest.substring(0, sp) : rest;
        String ent = (sp > 0) ? rest.substring(sp + 1) : "";
        ent.trim();
        svc.replace(".", "/");
        callHomeAssistantService(svc, ent);
        return;
    }

    if (linha.startsWith("STRING ")) {
        Keyboard.print(linha.substring(7));
        return;
    }

    if (linha.startsWith("DELAY ")) {
        delay(linha.substring(6).toInt());
        return;
    }

    String upperLine = linha;
    upperLine.toUpperCase();
    if (upperLine == "VOLUME_UP" || upperLine == "VOL_UP") {
        ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
        ConsumerControl.release();
        return;
    }
    if (upperLine == "VOLUME_DOWN" || upperLine == "VOL_DOWN") {
        ConsumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
        ConsumerControl.release();
        return;
    }
    if (upperLine == "MUTE" || upperLine == "VOLUME_MUTE") {
        ConsumerControl.press(CONSUMER_CONTROL_MUTE);
        ConsumerControl.release();
        return;
    }

    // Tokenize combo/key sequence separated by spaces or '+'
    String norm = linha;
    norm.replace("+", " ");
    bool hasModifier = false;
    bool pressedAny  = false;

    int start = 0;
    while (start < (int)norm.length()) {
        while (start < (int)norm.length() && norm[start] == ' ') start++;
        if (start >= (int)norm.length()) break;
        int end = norm.indexOf(' ', start);
        if (end < 0) end = norm.length();
        String rawTok = norm.substring(start, end);
        String tok = rawTok;
        tok.toUpperCase();
        start = end + 1;

        if (tok == "CTRL" || tok == "CONTROL" || tok == "LCTRL") {
            Keyboard.press(KEY_LEFT_CTRL);
            hasModifier = true; pressedAny = true;
        } else if (tok == "RCTRL") {
            Keyboard.press(KEY_RIGHT_CTRL);
            hasModifier = true; pressedAny = true;
        } else if (tok == "ALT" || tok == "LALT") {
            Keyboard.press(KEY_LEFT_ALT);
            hasModifier = true; pressedAny = true;
        } else if (tok == "RALT" || tok == "ALTGR") {
            Keyboard.press(KEY_RIGHT_ALT);
            hasModifier = true; pressedAny = true;
        } else if (tok == "SHIFT" || tok == "LSHIFT") {
            Keyboard.press(KEY_LEFT_SHIFT);
            hasModifier = true; pressedAny = true;
        } else if (tok == "RSHIFT") {
            Keyboard.press(KEY_RIGHT_SHIFT);
            hasModifier = true; pressedAny = true;
        } else if (tok == "GUI" || tok == "WIN" || tok == "WINDOWS" || tok == "SUPER") {
            Keyboard.press(KEY_LEFT_GUI);
            hasModifier = true; pressedAny = true;
        } else {
            uint8_t special = mapSpecialKey(tok);
            if (special != 0) {
                Keyboard.press(special);
                pressedAny = true;
            } else if (rawTok.length() == 1) {
                char ch = hasModifier ? (char)tolower(rawTok[0]) : rawTok[0];
                Keyboard.press((uint8_t)ch);
                pressedAny = true;
            }
        }
    }

    if (pressedAny) {
        delay(95);
        Keyboard.releaseAll();
    }
}

bool executarArquivoDucky(String fileName) {
    String fullPath = fileName.startsWith("/") ? fileName : "/" + fileName;
    File scriptFile = FFat.open(fullPath, "r");

    if (!scriptFile || scriptFile.size() == 0) {
        if (scriptFile) scriptFile.close();
        traceMacroResult = "NOT FOUND/EMPTY";
        traceMacroFile = fullPath;
        traceTimer = millis();
        return false;
    }

    traceMacroResult = "EXECUTING...";
    traceMacroFile = fullPath;
    lastFriendlyMacro = traduzirNome(fullPath);
    traceTimer = millis();
    while (scriptFile.available()) {
        processarLinhaDucky(scriptFile.readStringUntil('\n'));
    }
    scriptFile.close();
    traceMacroResult = "SUCCESS";
    pendingHaSync = true;
    return true;
}

// --------------------------------------------------------------------------------------
// Home Assistant Telemetry Sync (Executed asynchronously in loop)
// --------------------------------------------------------------------------------------
void syncHomeAssistantStatus() {
    if (WiFi.status() != WL_CONNECTED) return;

    JsonDocument doc;
    doc["state"] = lastFriendlyMacro;
    JsonObject attrs = doc["attributes"].to<JsonObject>();
    attrs["friendly_name"]  = "MynouPAD S3 Console";
    attrs["icon"]           = "mdi:keyboard-variant";
    attrs["entity_picture"] = "http://" + WiFi.localIP().toString() + "/mascot.svg";
    attrs["firmware"]       = FIRMWARE_VERSION;
    attrs["ip"]             = WiFi.localIP().toString();
    attrs["rssi"]           = WiFi.RSSI();
    attrs["volume"]         = isMuteActive ? 0 : volumeAtual;
    attrs["muted"]          = isMuteActive;
    attrs["python_bridge"]  = pythonBridgeActive;
    attrs["last_macro"]     = lastFriendlyMacro;
    attrs["last_file"]      = traceMacroFile;
    attrs["last_result"]    = traceMacroResult;
    attrs["uptime_s"]       = millis() / 1000;

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.setTimeout(800);
    if (http.begin(String(HA_BASE_URL) + "/api/states/sensor.mynoupad_status")) {
        http.addHeader("Authorization", SECRET_HA_TOKEN);
        http.addHeader("Content-Type", "application/json");
        http.POST(body);
        http.end();
    }
}

String buildStatusJson() {
    JsonDocument doc;
    doc["version"]       = FIRMWARE_VERSION;
    doc["ha_url"]        = HA_BASE_URL;
    doc["ip"]            = wifiConnected ? WiFi.localIP().toString() : "0.0.0.0";
    doc["rssi"]          = wifiConnected ? WiFi.RSSI() : 0;
    doc["uptime_s"]      = millis() / 1000;
    doc["volume"]        = volumeAtual;
    doc["muted"]         = isMuteActive;
    doc["python_bridge"] = pythonBridgeActive;
    doc["last_macro"]    = lastFriendlyMacro;
    doc["last_file"]     = traceMacroFile;
    doc["last_result"]   = traceMacroResult;
    doc["ffat_total"]    = ffAtMounted ? (uint32_t)FFat.totalBytes() : 0;
    doc["ffat_used"]     = ffAtMounted ? (uint32_t)FFat.usedBytes() : 0;

    JsonObject macrosObj = doc["macros"].to<JsonObject>();
    JsonObject labelsObj = doc["labels"].to<JsonObject>();

    const char* modes[2] = {"macro", "combo"};
    for (int m = 0; m < 2; m++) {
        for (int b = 1; b <= 9; b++) {
            String key = String(modes[m]) + "_" + String(b);
            String path = "/" + key + ".txt";
            size_t sz = 0;
            if (ffAtMounted && FFat.exists(path)) {
                File f = FFat.open(path, "r");
                if (f) { sz = f.size(); f.close(); }
            }
            if (sz > 0) macrosObj[key] = (uint32_t)sz;
            String lbl = traduzirNome(key);
            if (lbl != key) labelsObj[key] = lbl;
        }
    }

    String out;
    serializeJson(doc, out);
    return out;
}

// --------------------------------------------------------------------------------------
// OLED Interface & Haptics
// --------------------------------------------------------------------------------------
void drawFace(bool isBlinking) {
    display.clearDisplay();
    if (isBlinking) display.fillRect(41, 24, 12, 2, SSD1306_WHITE);
    else            display.fillRect(41, 19, 12, 12, SSD1306_WHITE);
    display.fillRect(74, 19, 12, 12, SSD1306_WHITE);
    display.fillRect(52, 48, 25, 4, SSD1306_WHITE);
    for (int i = 0; i < 4; i++) {
        display.drawLine(43 + i, 40, 52 + i, 51, SSD1306_WHITE);
        display.drawLine(85 - i, 40, 76 - i, 51, SSD1306_WHITE);
    }
    display.display();
}

void showSplash() {
    display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
    String b = "MynouPAD"; display.setTextSize(2);
    for (int i = 0; i <= (int)b.length(); i++) {
        display.clearDisplay(); display.setCursor(18, 25);
        display.print(b.substring(0, i)); display.display(); delay(110);
    }
    delay(500); drawFace(false); delay(800); drawFace(true); delay(200); drawFace(false); delay(600);
}

void drawHeader() {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 3);
    display.print("MynouPAD v7");

    if (wifiConnected) {
        int x = 120, y = 9;
        display.drawCircleHelper(x, y, 4, 1, SSD1306_WHITE);
        display.drawCircleHelper(x, y, 7, 1, SSD1306_WHITE);
        display.drawPixel(x, y, SSD1306_WHITE);
    }

    display.drawFastHLine((frameCounter * 3) % 128, 13, 15, SSD1306_WHITE);
    display.drawFastHLine(0, 15, 128, SSD1306_WHITE);
}

void atualizaDisplay() {
    display.clearDisplay();
    drawHeader();

    if (traceTimer > 0 && millis() - traceTimer < 3000) {
        if (traceMacroResult == "NOT FOUND/EMPTY") {
            char keyDigit = ' ';
            for (int i = 0; i < (int)traceMacroFile.length(); i++) {
                if (isDigit(traceMacroFile[i])) { keyDigit = traceMacroFile[i]; break; }
            }
            display.setTextSize(5);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(51, 24);
            display.print(keyDigit);
        } else {
            String friendlyName = traduzirNome(traceMacroFile);
            display.setCursor(0, 24);
            display.print("Acao disparada:");
            display.fillRect(0, 35, 128, 15, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(4, 39);
            display.print(friendlyName);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 54);
            display.print("Status: ");
            display.print(traceMacroResult);
        }
    } else if (!pythonBridgeActive) {
        display.setCursor(18, 28); display.print("Aguardando Python");
        int cX = 64, cY = 48;
        for (int i = 0; i < 8; i++) {
            float rad = i * (PI / 4);
            int px = cX + cos(rad) * 8, py = cY + sin(rad) * 8;
            if (i == ((frameCounter / 2) % 8)) display.fillCircle(px, py, 2, SSD1306_WHITE);
            else display.drawPixel(px, py, SSD1306_WHITE);
        }
    } else {
        if (isMuteActive) {
            display.setTextSize(2);
            display.setCursor(40, 32);
            display.print("MUTE");
        } else {
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
        if (fileEntry.size() > 0) {
            String fName = String(fileEntry.name());
            if (fName.indexOf("macro_") != -1 || fName.indexOf("combo_") != -1) {
                menuItems[menuCount++] = fName;
            }
        }
        fileEntry = rootDir.openNextFile();
    }
}

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

    int visibleItems = 4;
    for (int i = 0; i < visibleItems; i++) {
        int itemIndex = menuScrollOffset + i;
        if (itemIndex >= menuCount) break;
        int yPos = 14 + (i * 13);
        if (itemIndex == menuSelectedIndex) {
            display.fillRect(0, yPos - 1, 128, 13, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(2, yPos + 2);
        display.print(traduzirNome(menuItems[itemIndex]));
    }
    display.display();
}

void triggerHaptic(int pattern) {
    if (pattern == 1) {
        digitalWrite(PIN_MOTOR, HIGH); timer_vibration = millis() + 85;  state_vibration = 1;
    } else if (pattern == 2) {
        digitalWrite(PIN_MOTOR, HIGH); timer_vibration = millis() + 150; state_vibration = 1;
    } else if (pattern == 3) {
        digitalWrite(PIN_MOTOR, HIGH); timer_vibration = millis() + 85;  state_vibration = 2;
    } else if (pattern == 4) {
        digitalWrite(PIN_MOTOR, HIGH); timer_vibration = millis() + 300; state_vibration = 1;
    } else if (pattern == 5) {
        digitalWrite(PIN_MOTOR, HIGH); timer_vibration = millis() + 25;  state_vibration = 1;
    }
}

// --------------------------------------------------------------------------------------
// Setup
// --------------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    ConsumerControl.begin();
    Keyboard.begin();
    USB.begin();

    pixels.begin(); pixels.setBrightness(15); pixels.show();
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);

    showSplash();
    if (!FFat.begin(true)) Serial.println("FFat Error"); else ffAtMounted = true;
    loadLabelsFromFFat();

    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_MOTOR, OUTPUT);
    digitalWrite(PIN_MOTOR, LOW);

    tone(PIN_BUZZER, 988); delay(80); noTone(PIN_BUZZER); delay(40);
    tone(PIN_BUZZER, 1319); delay(150); noTone(PIN_BUZZER);

    WiFi.begin(wifi_ssid, wifi_password);
    unsigned long sW = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - sW < 8000) delay(500);

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        MDNS.begin("mynoupad");
        DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
            r->send(200, "text/html", index_html);
        });

        server.on("/mascot.svg", HTTP_GET, [](AsyncWebServerRequest *r) {
            AsyncWebServerResponse *res = r->beginResponse(200, "image/svg+xml", mascot_svg);
            res->addHeader("Cache-Control", "public, max-age=86400");
            r->send(res);
        });

        server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r) {
            r->send(200, "application/json", buildStatusJson());
        });

        server.on("/get_macro", HTTP_GET, [](AsyncWebServerRequest *r) {
            if (!r->hasParam("btn")) { r->send(400); return; }
            String b = r->getParam("btn")->value();
            bool isC = r->hasParam("mode") && r->getParam("mode")->value() == "combo";
            String p = isC ? "/combo_" + b + ".txt" : "/macro_" + b + ".txt";
            if (FFat.exists(p)) r->send(FFat, p, "text/plain"); else r->send(204);
        });

        server.on("/save", HTTP_POST, [](AsyncWebServerRequest *r) {
            if (!r->hasParam("btn", true) || !r->hasParam("script", true)) { r->send(400); return; }
            String b = r->getParam("btn", true)->value();
            bool isC = r->hasParam("isCombo", true);
            String p = isC ? "/combo_" + b + ".txt" : "/macro_" + b + ".txt";

            String scriptContent = r->getParam("script", true)->value();
            scriptContent.trim();

            if (scriptContent.length() == 0) {
                if (FFat.exists(p)) FFat.remove(p);
            } else {
                File f = FFat.open(p, "w");
                if (f) { f.print(scriptContent); f.close(); }
            }
            r->send(200, "application/json", "{\"ok\":true}");
        });

        server.on("/api/labels", HTTP_POST, [](AsyncWebServerRequest *r) {
            if (!r->hasParam("key", true)) { r->send(400); return; }
            String key = r->getParam("key", true)->value();
            String val = r->hasParam("label", true) ? r->getParam("label", true)->value() : "";
            key.trim(); val.trim();
            if (val.length() == 0) labelOverrides.erase(key);
            else labelOverrides[key] = val;
            saveLabelsToFFat();
            r->send(200, "application/json", "{\"ok\":true}");
        });

        auto handleTrigger = [](AsyncWebServerRequest *r) {
            String b = r->hasParam("btn") ? r->getParam("btn")->value() : (r->hasParam("btn", true) ? r->getParam("btn", true)->value() : "");
            if (b.length() == 0) { r->send(400, "application/json", "{\"error\":\"missing btn\"}"); return; }
            String mode = r->hasParam("mode") ? r->getParam("mode")->value() : (r->hasParam("mode", true) ? r->getParam("mode", true)->value() : "macro");
            pendingTriggerPath = (mode == "combo") ? ("/combo_" + b + ".txt") : ("/macro_" + b + ".txt");
            r->send(200, "application/json", "{\"ok\":true,\"queued\":\"" + pendingTriggerPath + "\"}");
        };
        server.on("/api/trigger", HTTP_GET, handleTrigger);
        server.on("/api/trigger", HTTP_POST, handleTrigger);

        auto handleAction = [](AsyncWebServerRequest *r) {
            String cmd = r->hasParam("cmd") ? r->getParam("cmd")->value() : (r->hasParam("cmd", true) ? r->getParam("cmd", true)->value() : "");
            if (cmd.length() == 0) { r->send(400, "application/json", "{\"error\":\"missing cmd\"}"); return; }
            pendingActionCmd = cmd;
            r->send(200, "application/json", "{\"ok\":true}");
        };
        server.on("/api/action", HTTP_GET, handleAction);
        server.on("/api/action", HTTP_POST, handleAction);

        server.begin();
        pendingHaSync = true;
    }

    encoder.attachHalfQuad(PIN_ENC_DT, PIN_ENC_CLK);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);
    for (int i = 0; i < 3; i++) {
        pinMode(pins_matrix_cols[i], INPUT_PULLUP);
        pinMode(pins_matrix_rows[i], INPUT);
    }
    bootTimerTimestamp = millis();
}

bool readKnobStable() {
    int count = 0;
    for (int i = 0; i < 5; i++) {
        if (digitalRead(PIN_ENC_SW) == LOW) count++;
        delayMicroseconds(200);
    }
    return (count >= 3);
}

// --------------------------------------------------------------------------------------
// Main Loop
// --------------------------------------------------------------------------------------
void loop() {
    if (!pythonBridgeActive && attempts_autoStart < MAX_RETRY_AUTO) {
        unsigned long now = millis();
        if (now - (attempts_autoStart == 0 ? bootTimerTimestamp : lastRetryTimestamp) > (attempts_autoStart == 0 ? 4500 : 20000)) {
            attempts_autoStart++;
            lastRetryTimestamp = now;
            Keyboard.press(KEY_LEFT_CTRL); Keyboard.press(KEY_LEFT_ALT); Keyboard.press(KEY_LEFT_SHIFT); Keyboard.press('m');
            delay(130); Keyboard.releaseAll();
        }
    }

    // Process queued remote trigger from Web Studio or Home Assistant
    if (pendingTriggerPath.length() > 0) {
        String p = pendingTriggerPath;
        pendingTriggerPath = "";
        if (executarArquivoDucky(p)) {
            tone(PIN_BUZZER, 600, 50);
            tone(PIN_BUZZER, 400, 80);
            triggerHaptic(p.indexOf("combo_") != -1 ? 3 : 1);
        } else {
            triggerHaptic(4);
        }
        timer_ledFlash = millis() + 120;
    }

    // Process queued remote media/beep command
    if (pendingActionCmd.length() > 0) {
        String cmd = pendingActionCmd;
        pendingActionCmd = "";
        if (cmd == "mute") {
            ConsumerControl.press(CONSUMER_CONTROL_MUTE); ConsumerControl.release();
            triggerHaptic(2);
        } else if (cmd == "vol_up") {
            ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT); ConsumerControl.release();
            triggerHaptic(5);
        } else if (cmd == "vol_down") {
            ConsumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT); ConsumerControl.release();
            triggerHaptic(5);
        } else if (cmd == "beep") {
            tone(PIN_BUZZER, 1046, 90);
            triggerHaptic(1);
        }
        pendingHaSync = true;
    }

    bool knob = readKnobStable();

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

    for (int r = 0; r < 3; r++) {
        pinMode(pins_matrix_rows[r], OUTPUT);
        digitalWrite(pins_matrix_rows[r], LOW);

        for (int c = 0; c < 3; c++) {
            bool reading = (digitalRead(pins_matrix_cols[c]) == LOW);

            if (reading != state_lastKeys[r][c] && (millis() - time_lastDebounce[r][c] > DEBOUNCE_MS)) {
                time_lastDebounce[r][c] = millis();
                state_lastKeys[r][c] = reading;

                if (reading) {
                    if (!knob) triggerHaptic(1);

                    char k = logicalMap[r][c];
                    String path = knob ? "/combo_" + String(k) + ".txt" : "/macro_" + String(k) + ".txt";
                    if (knob) isComboModeActive = true;

                    if (executarArquivoDucky(path)) {
                        tone(PIN_BUZZER, 600, 50);
                        tone(PIN_BUZZER, 400, 80);
                        if (knob) triggerHaptic(3);
                    } else {
                        Keyboard.press(k);
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
            if (p > lastEncoderValue) menuSelectedIndex++; else menuSelectedIndex--;
            if (menuSelectedIndex < 0) menuSelectedIndex = 0;
            if (menuSelectedIndex >= menuCount) menuSelectedIndex = menuCount - 1;
            if (menuSelectedIndex < menuScrollOffset) menuScrollOffset = menuSelectedIndex;
            if (menuSelectedIndex >= menuScrollOffset + 4) menuScrollOffset = menuSelectedIndex - 3;
            menuTimeoutTimer = millis();
            triggerHaptic(5);
        } else {
            vuBarsLevel = 10;
            if (p > lastEncoderValue) ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
            else ConsumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
            ConsumerControl.release();
            timer_volFlash = millis() + 200;
            triggerHaptic(5);
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
        } else if (isMenuMode) {
            if (menuCount > 0) {
                String path = "/" + menuItems[menuSelectedIndex];
                if (executarArquivoDucky(path)) {
                    tone(PIN_BUZZER, 600, 50);
                    tone(PIN_BUZZER, 400, 80);
                    triggerHaptic(path.indexOf("combo_") != -1 ? 3 : 1);
                } else {
                    triggerHaptic(4);
                }
            }
            isMenuMode = false;
        } else if (!isComboModeActive && !isKnobBeingLongPressed) {
            static unsigned long lastM = 0;
            if (millis() - lastM > 400) {
                ConsumerControl.press(CONSUMER_CONTROL_MUTE);
                ConsumerControl.release();
                lastM = millis();
                triggerHaptic(2);
            }
        }
        knobStateHistory = false;
        isComboModeActive = false;
    }
    knobStateHistory = knob;

    if (Serial.available() > 0) {
        String msg = Serial.readStringUntil('\n');
        msg.trim();
        if (msg.startsWith("V")) {
            volumeAtual = msg.substring(1).toInt();
            isMuteActive = (volumeAtual == -1);
            if (!pythonBridgeActive) {
                pythonBridgeActive = true;
                pixels.setPixelColor(0, 0, 255, 0);
                pixels.show();
                pendingHaSync = true;
            }
            if (!isMenuMode) atualizaDisplay();
        }
    }

    if (isMenuMode && (millis() - menuTimeoutTimer > 5000)) {
        isMenuMode = false;
    }

    // Periodic or event-driven telemetry push to Home Assistant
    if (wifiConnected && (pendingHaSync || (millis() - lastHaSyncMs > HA_SYNC_INTERVAL_MS))) {
        pendingHaSync = false;
        lastHaSyncMs = millis();
        syncHomeAssistantStatus();
    }

    static unsigned long lT = 0;
    if (millis() - lT > 60) {
        frameCounter++;
        if (vuBarsLevel > 0) vuBarsLevel--;
        if (millis() < timer_ledFlash) pixels.setPixelColor(0, 150, 255, 150);
        else if (pythonBridgeActive) {
            if (isMuteActive) pixels.setPixelColor(0, 255, 0, 0);
            else pixels.setPixelColor(0, 0, 180, 255);
        } else {
            pixels.setPixelColor(0, 50, 50, 0);
        }
        pixels.show();

        if (isMenuMode) drawMenu();
        else atualizaDisplay();
        lT = millis();
    }
}
