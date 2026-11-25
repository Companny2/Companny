/*
  ============================================================================
   RADIO HÍBRIDA ESP32 + LoRa (REYAX RYLR998)
  ============================================================================
   Cada estación = 1 ESP32 + 1 RYLR998. Tu celular se conecta al WiFi que
   crea el ESP32 y usa un chat en http://192.168.4.1

   - CERCA  (ESP-NOW, ~100-200 m): texto, notas de voz y fotos.
   - LEJOS  (LoRa, varios km):     texto y SOS.
   - El cambio entre CERCA y LEJOS es automático.
   - Al abrir la página aparece un MENÚ:
       · Modo SIN internet: solo radio entre las estaciones.
       · Modo CON internet: la radio se conecta a una red WiFi (elegida desde
         el celular) y publica todo en www.panarepara.com/radio mediante MQTT.
     El modo y la red quedan guardados; se puede cambiar sin perder datos.

   Arduino IDE: placa "ESP32 Dev Module", núcleo ESP32 3.x
   Librerías: "PubSubClient" y "DHT sensor library" (Adafruit)
  ============================================================================
*/

// ---------------------------------------------------------------------------
// 1) CONFIGURACIÓN
// ---------------------------------------------------------------------------
#define EQUIPO   1          // 1 en una placa, 2 en la otra

// WiFi que crea la placa para tu celular
const char* CLAVE_AP = "radio1234";        // mínimo 8 caracteres
#define CANAL_WIFI 1                       // canal inicial; luego se ajusta solo
#define PREFERIR_LORA 1                    // 1 = texto, SOS y sensores SIEMPRE por LoRa (ESP-NOW solo fotos/voz y respaldo)

// Modo CON internet (la red WiFi se elige desde el menú del celular)
const char* MQTT_HOST    = "broker.emqx.io";   // servidor MQTT público (funciona también en Brave)
const int   MQTT_PORT    = 1883;
const char* TOPIC_PREFIX = "panarepara-radio-acbdcce2";  // prefijo MQTT único (igual en la web)

// LoRa (RYLR998): debe ser igual en las dos placas
#define LORA_RX_PIN  16                    // RX2 del ESP32 <- TXD del RYLR998
#define LORA_TX_PIN  17                    // TX2 del ESP32 -> RXD del RYLR998
#define LORA_BAUD    115200
#define LORA_RED     18                    // NETWORKID (0-15 o 18)
#define LORA_BANDA   "915000000"           // Hz (verifica la banda permitida en tu país)
#define LORA_PARAM   "9,7,1,12"            // SF9, 125 kHz, CR 4/5, preámbulo 12
#define LORA_CLAVE   "4F7A21C9"            // cifrado: 8 dígitos hex, igual en ambas

#define LED_PIN   2                        // LED azul de la placa
#define BOTON_SOS 0                         // botón BOOT: pulsación de 2 s = SOS

// Sensor de temperatura y humedad DHT11
#define USAR_DHT       1                    // 1 = esta estación tiene DHT11, 0 = no
#define DHT_PIN        4                    // DAT del DHT11 -> D4 (GPIO4)
#define DHT_TIPO       DHT11
#define CLIMA_CADA_MS  30000                // enviar clima cada 30 s

// Sensor de gas MQ (salida analógica AO con divisor de voltaje 5 V -> 3.3 V)
#define USAR_GAS       1                    // 1 = esta estación tiene sensor MQ, 0 = no
#define GAS_PIN        34                   // punto medio del divisor -> D34 (GPIO34)
#define GAS_ALARMA     60                   // % a partir del cual se envía SOS automático
#define GAS_CALENTAR_MS 60000               // el sensor necesita ~1 min para calentarse
#define HIST_N         120                  // historial para la gráfica: 120 puntos = 1 hora

// Distancia aproximada a partir de la señal (RSSI), modelo log-distancia:
//   distancia = 10 ^ ((RSSI_a_1m - RSSI) / (10 * N))
#define RSSI_1M_LORA   -20                  // RSSI LoRa medido a 1 m (se puede calibrar)
#define RSSI_1M_ESP    -40                  // RSSI ESP-NOW medido a 1 m (se puede calibrar)
#define N_TRAYECTO     2.7                  // 2.0 campo abierto · 2.7 mixto · 3.5 con edificios

// ---------------------------------------------------------------------------
// 2) LIBRERÍAS
// ---------------------------------------------------------------------------
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <vector>
#include "pagina.h"                        // la página del chat (HTML)
#if USAR_DHT
#include <DHT.h>                            // librería "DHT sensor library" (Adafruit)
#endif
#include <PubSubClient.h>                   // MQTT (modo con internet)
#include <Preferences.h>                    // memoria permanente (modo y red WiFi)

// ---------------------------------------------------------------------------
// 3) CONSTANTES Y TIPOS
// ---------------------------------------------------------------------------
const uint8_t OTRO = (EQUIPO == 1) ? 2 : 1;   // dirección LoRa de la otra placa
const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define MAX_MSGS        40       // mensajes guardados en memoria
#define MAX_TEXTO       180      // bytes por mensaje de texto
#define MAX_ARCHIVO     400000   // tamaño máximo de foto / nota de voz
#define CHUNK           236      // bytes de archivo por paquete ESP-NOW
#define CERCA_MS        8000     // sin noticias ESP-NOW en 8 s = lejos
#define LORA_VIVO_MS    75000    // sin noticias LoRa en 75 s = sin enlace

enum Tipo   : uint8_t { TEXTO = 0, SOS = 1, AUDIO = 2, FOTO = 3 };
enum Estado : uint8_t { ENVIANDO = 0, ENTREGADO = 1, FALLIDO = 2, RECIBIDO = 3 };

// Clima (DHT11): lectura propia y la recibida de la otra estación
struct Clima { bool okTH = false, okGas = false; float t = 0, h = 0, g = 0; uint32_t cuando = 0; };
// Historial de lecturas para la gráfica (memoria circular)
struct Punto { float t, h, g; bool okTH, okGas; uint32_t ms; };
struct Hist { Punto p[HIST_N]; uint16_t n = 0, ini = 0; };

struct Msg {
  uint16_t id;        // número del mensaje (lo pone quien lo envía)
  uint8_t  de;        // equipo que lo escribió (0 = página web)
  bool     mio;       // true si salió de esta placa
  uint8_t  tipo;      // TEXTO, SOS, AUDIO, FOTO
  uint8_t  estado;    // ENVIANDO, ENTREGADO...
  char     via;       // 'E' ESP-NOW, 'L' LoRa, '-' aún no
  int16_t  rssi;      // señal con la que llegó
  uint8_t  progreso;  // % de envío de archivos
  String   texto;     // texto, o ruta del archivo
  uint32_t ts;        // hora real (epoch) si la base tiene internet; 0 si no se conoce
};
std::vector<Msg> msgs;
uint32_t version = 1;          // cambia cada vez que algo cambia (para la web)

// Mensajes de texto esperando confirmación
struct Pendiente {
  uint16_t id; uint8_t tipo; String texto;
  char via; uint8_t intentos; uint32_t t;
  bool otraVia;       // ya se probó la vía alternativa
};
std::vector<Pendiente> pendientes;

// ---------------------------------------------------------------------------
// 4) VARIABLES GLOBALES
// ---------------------------------------------------------------------------
WebServer server(80);
DNSServer dns;
HardwareSerial& lora = Serial2;

uint16_t siguienteId = 1;
uint32_t ultimoEspNow = 0, ultimoLora = 0;
int espRssi = 0, loraRssi = 0, loraSnr = 0;
bool loraOk = false;
uint8_t sosDe = 0;             // equipo que mandó SOS sin atender (0 = ninguno)
uint32_t sosHasta = 0;

// Cola de recepción ESP-NOW (el callback solo copia, el loop procesa)
struct RxEsp { uint8_t d[250]; int len; int rssi; };
QueueHandle_t colaEsp;

// LoRa: cola de envíos (el módulo atiende un comando a la vez)
std::vector<String> colaLora;
bool loraOcupado = false;
uint32_t loraDesde = 0;
String lineaLora;

WiFiClient net;
PubSubClient mqtt(net);
String T(const char* s) { return String(TOPIC_PREFIX) + "/" + s; }

// Modo de funcionamiento (se guarda en la memoria permanente)
Preferences prefs;
bool modoOnline = false;                       // false = sin internet, true = con internet
String redSsid, redClave;                      // red WiFi elegida en el menú
uint8_t canalActual = CANAL_WIFI;
bool online() { return modoOnline && WiFi.status() == WL_CONNECTED; }

bool cerca() { return ultimoEspNow && millis() - ultimoEspNow < CERCA_MS; }
bool loraVivo() { return ultimoLora && millis() - ultimoLora < LORA_VIVO_MS; }

Clima climaYo, climaOtro;
bool climaVigente(const Clima& c) { return (c.okTH || c.okGas) && millis() - c.cuando < 180000; }  // 3 min
Hist histYo, histOtro;
void agregarHist(Hist& hs, const Clima& c) {
  uint16_t i;
  if (hs.n < HIST_N) { i = (hs.ini + hs.n) % HIST_N; hs.n++; }
  else { i = hs.ini; hs.ini = (hs.ini + 1) % HIST_N; }      // lleno: reemplaza el más viejo
  hs.p[i] = {c.t, c.h, c.g, c.okTH, c.okGas, millis()};
}
// [[segundos atrás, temp, hum, gas], ...]
String histJson(const Hist& hs) {
  String j = "[";
  for (uint16_t k = 0; k < hs.n; k++) {
    const Punto& p = hs.p[(hs.ini + k) % HIST_N];
    if (k) j += ",";
    j += "[" + String((millis() - p.ms) / 1000) + "," + (p.okTH ? String(p.t, 1) : String("null")) + "," +
         (p.okTH ? String(p.h, 0) : String("null")) + "," + (p.okGas ? String(p.g, 0) : String("null")) + "]";
  }
  return j + "]";
}
// RSSI promediado (suaviza saltos de la señal)
float espProm = 0, loraProm = 0;
void promediar(float& prom, int nuevo) { prom = (prom == 0) ? nuevo : prom * 0.7f + nuevo * 0.3f; }
// Distancia aproximada en metros; -1 si no hay enlace. via: 'E' ESP-NOW, 'L' LoRa
float distanciaM(char& via) {
  float rssi, ref;
  if (cerca())         { via = 'E'; rssi = espProm;  ref = RSSI_1M_ESP; }
  else if (loraVivo()) { via = 'L'; rssi = loraProm; ref = RSSI_1M_LORA; }
  else { via = '-'; return -1; }
  float d = powf(10.0f, (ref - rssi) / (10.0f * N_TRAYECTO));
  return d < 1 ? 1 : d;
}
String distJson() {
  char via; float d = distanciaM(via);
  if (d < 0) return "null";
  return "{\"m\":" + String(d, 0) + ",\"via\":\"" + String(via) + "\"}";
}
String climaJson(const Clima& c) {
  if (!climaVigente(c)) return "null";
  String j = "{\"t\":" + (c.okTH ? String(c.t, 1) : String("null")) +
             ",\"h\":" + (c.okTH ? String(c.h, 0) : String("null")) +
             ",\"g\":" + (c.okGas ? String(c.g, 0) : String("null")) + "}";
  return j;
}
#if USAR_DHT
DHT dht(DHT_PIN, DHT_TIPO);
#endif

// ---------------------------------------------------------------------------
// 5) LISTA DE MENSAJES
// ---------------------------------------------------------------------------
Msg* buscar(uint8_t de, uint16_t id, bool mio) {
  for (auto& m : msgs)
    if (m.id == id && m.de == de && m.mio == mio) return &m;
  return nullptr;
}

Msg& agregar(const Msg& m) {
  if (msgs.size() >= MAX_MSGS) {                 // lleno: borrar el más viejo
    Msg& v = msgs.front();
    if ((v.tipo == AUDIO || v.tipo == FOTO) && v.texto.length())
      LittleFS.remove(v.texto);
    msgs.erase(msgs.begin());
  }
  msgs.push_back(m);
  time_t ahora = time(nullptr);
  msgs.back().ts = (ahora > 1700000000) ? (uint32_t)ahora : 0;   // hora de internet (solo la base)
  version++;
  return msgs.back();
}

void parpadear(int veces) {
  for (int i = 0; i < veces; i++) {
    digitalWrite(LED_PIN, HIGH); delay(40);
    digitalWrite(LED_PIN, LOW);  delay(40);
  }
}

String limpiar(String s) {                        // sin saltos de línea
  s.replace("\r", " "); s.replace("\n", " ");
  while (s.length() > MAX_TEXTO) s.remove(s.length() - 1);
  return s;
}

// ---------------------------------------------------------------------------
// 6) LoRa (RYLR998 con comandos AT)
// ---------------------------------------------------------------------------
// Envía un comando y espera "+OK" (solo se usa al arrancar)
bool loraComando(const String& cmd, uint32_t espera = 1500) {
  while (lora.available()) lora.read();
  lora.print(cmd + "\r\n");
  uint32_t t0 = millis();
  String l;
  while (millis() - t0 < espera) {
    while (lora.available()) {
      char c = lora.read();
      if (c == '\n') {
        l.trim();
        if (l.startsWith("+OK")) return true;
        if (l.startsWith("+ERR")) { Serial.printf("LoRa %s -> %s\n", cmd.c_str(), l.c_str()); return false; }
        l = "";
      } else l += c;
    }
  }
  return false;
}

void loraIniciar() {
  lora.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(200);
  loraOk = loraComando("AT");
  if (!loraOk) {
    Serial.println("!! El RYLR998 no responde: revisa TXD->16, RXD->17, 3V3 y GND");
    return;
  }
  loraComando("AT+ADDRESS=" + String(EQUIPO));
  loraComando("AT+NETWORKID=" + String(LORA_RED));
  loraComando("AT+BAND=" + String(LORA_BANDA));
  loraComando("AT+PARAMETER=" + String(LORA_PARAM));
  loraComando("AT+CPIN=" + String(LORA_CLAVE));
  loraComando("AT+CRFOP=22");                     // potencia máxima (22 dBm)
  Serial.println("LoRa listo");
}

// Encola un envío LoRa a la otra placa
void loraEnviar(const String& datos) {
  if (!loraOk) return;
  colaLora.push_back("AT+SEND=" + String(OTRO) + "," + String(datos.length()) + "," + datos);
}

void procesarTramaTexto(const String& s, char via, int rssi);

// Atiende el módulo: manda lo pendiente y lee lo que llega
void loraLoop() {
  if (!loraOk) return;
  if (!loraOcupado && !colaLora.empty()) {
    lora.print(colaLora.front() + "\r\n");
    colaLora.erase(colaLora.begin());
    loraOcupado = true;
    loraDesde = millis();
  }
  if (loraOcupado && millis() - loraDesde > 5000) loraOcupado = false;  // por si acaso

  while (lora.available()) {
    char c = lora.read();
    if (c != '\n') { if (lineaLora.length() < 300) lineaLora += c; continue; }
    lineaLora.trim();
    if (lineaLora.startsWith("+OK") || lineaLora.startsWith("+ERR")) {
      loraOcupado = false;
    } else if (lineaLora.startsWith("+RCV=")) {
      // Formato: +RCV=<dirección>,<largo>,<datos>,<RSSI>,<SNR>
      int p1 = lineaLora.indexOf(',');
      int p2 = lineaLora.indexOf(',', p1 + 1);
      int largo = lineaLora.substring(p1 + 1, p2).toInt();
      int ini = p2 + 1;
      while (ini < (int)lineaLora.length() && lineaLora[ini] == ' ') ini++;  // por si trae espacios
      String datos = lineaLora.substring(ini, ini + largo);
      String resto = lineaLora.substring(ini + largo + 1);
      int p3 = resto.indexOf(',');
      loraRssi = resto.substring(0, p3).toInt();
      promediar(loraProm, loraRssi);
      loraSnr  = resto.substring(p3 + 1).toInt();
      ultimoLora = millis();
      version++;
      procesarTramaTexto(datos, 'L', loraRssi);
    }
    lineaLora = "";
  }
}

// ---------------------------------------------------------------------------
// 7) ESP-NOW (radio directa entre ESP32)
// ---------------------------------------------------------------------------
void onEspNow(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len <= 0 || len > 250) return;
  RxEsp r;
  memcpy(r.d, data, len);
  r.len = len;
  r.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  xQueueSend(colaEsp, &r, 0);                    // el loop lo procesa
}

void espEnviar(const uint8_t* d, size_t n) { esp_now_send(BCAST, d, n); }
void espEnviarTexto(const String& s) { espEnviar((const uint8_t*)s.c_str(), s.length()); }

// ---------------------------------------------------------------------------
// 8) PROTOCOLO DE TEXTO (igual por ESP-NOW y por LoRa)
//    M,<de>,<id>,<tipo>,<texto>   mensaje
//    K,<de>,<id>                  confirmación ("lo recibí")
//    P,<de>                       "estoy aquí" (latido)
// ---------------------------------------------------------------------------
void enviarPorVia(char via, const String& trama) {
  if (via == 'E') espEnviarTexto(trama);
  else loraEnviar(trama);
}

void publicarMsg(const Msg& m);   // (MQTT, más abajo)

void procesarTramaTexto(const String& s, char via, int rssi) {
  if (s.length() < 3 || s[1] != ',') return;
  char tipoTrama = s[0];
  int c1 = s.indexOf(',', 2);
  uint8_t de = s.substring(2, c1 < 0 ? s.length() : c1).toInt();
  if (de == EQUIPO) return;                       // eco de mí mismo

  if (tipoTrama == 'P') {                         // latido: P,<de>,<canal>,<con internet 0/1>
    int c2 = s.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > 0) {
      bool otroOnline = s.substring(c2 + 1) == "1";
      // Se sigue el canal de la radio con internet; si ninguna tiene, la Radio 2 sigue a la Radio 1
      if (otroOnline || de < EQUIPO) seguirCanal(s.substring(c1 + 1, c2).toInt(), de);
      if (via == 'L' && !cerca()) pedirLatidoLora();       // responder rápido para reenganchar ESP-NOW
    }
    return;
  }

  if (tipoTrama == 'C') {                         // clima: C,<de>,<temp>,<hum>,<gas>  ("x" = sin dato)
    int c2 = s.indexOf(',', c1 + 1);
    int c3 = s.indexOf(',', c2 + 1);
    if (c1 < 0 || c2 < 0 || c3 < 0) return;
    String ts = s.substring(c1 + 1, c2), hs = s.substring(c2 + 1, c3), gs = s.substring(c3 + 1);
    climaOtro.okTH = (ts != "x" && hs != "x");
    climaOtro.t = ts.toFloat();
    climaOtro.h = hs.toFloat();
    climaOtro.okGas = (gs != "x");
    climaOtro.g = gs.toFloat();
    climaOtro.cuando = millis();
    agregarHist(histOtro, climaOtro);
    version++;
    Serial.printf("[CLIMA EQUIPO %d] %s C  %s %%  gas %s %%  (%s)\n", de, ts.c_str(), hs.c_str(), gs.c_str(),
                  via == 'E' ? "cerca" : "LoRa");
    return;
  }

  int c2 = s.indexOf(',', c1 + 1);
  uint16_t id = s.substring(c1 + 1, c2 < 0 ? s.length() : c2).toInt();

  if (tipoTrama == 'K') {                         // confirmación de algo mío
    for (size_t i = 0; i < pendientes.size(); i++)
      if (pendientes[i].id == id) { pendientes.erase(pendientes.begin() + i); break; }
    Msg* m = buscar(EQUIPO, id, true);
    if (!m) m = buscar(0, id, true);
    if (m && m->estado != ENTREGADO) { m->estado = ENTREGADO; m->via = via; version++; }
    return;
  }

  if (tipoTrama == 'M') {
    int c3 = s.indexOf(',', c2 + 1);
    if (c3 < 0) return;
    uint8_t tipo = s.substring(c2 + 1, c3).toInt();
    String texto = s.substring(c3 + 1);
    // Confirmar SIEMPRE por la misma vía (aunque sea repetido)
    enviarPorVia(via, "K," + String(EQUIPO) + "," + String(id));
    if (buscar(de, id, false)) return;            // repetido: ya lo tengo
    Msg m{id, de, false, tipo, RECIBIDO, via, (int16_t)rssi, 100, texto};
    agregar(m);
    publicarMsg(m);
    Serial.printf("[%s %s] %s\n", via == 'E' ? "CERCA" : "LoRa",
                  tipo == SOS ? "SOS" : "MSG", texto.c_str());
    if (tipo == SOS) { sosDe = de; sosHasta = millis() + 120000; }
    parpadear(3);
  }
}

// Envía un mensaje de texto o SOS, eligiendo la mejor vía
void enviarMensaje(uint8_t tipo, String texto, uint8_t de = EQUIPO) {
  texto = limpiar(texto);
  if (!texto.length()) return;
  uint16_t id = siguienteId++;
  char via = (PREFERIR_LORA && loraOk) ? 'L' : (cerca() ? 'E' : 'L');   // LoRa es la vía principal
  Msg m{id, de, true, tipo, ENVIANDO, via, 0, 0, texto};
  agregar(m);
  publicarMsg(m);
  String trama = "M," + String(EQUIPO) + "," + String(id) + "," + String(tipo) + "," + texto;
  enviarPorVia(via, trama);
  pendientes.push_back({id, tipo, trama, via, 1, millis()});
  Serial.printf("[ENVIADO por %s] %s\n", via == 'E' ? "CERCA" : "LoRa", texto.c_str());
}

// Reintentos: por ESP-NOW espera 1.5 s (2 intentos); si no hay respuesta,
// pasa a LoRa y espera 9 s (3 intentos). Si nada funciona: "no entregado".
void revisarPendientes() {
  for (size_t i = 0; i < pendientes.size(); i++) {
    Pendiente& p = pendientes[i];
    uint32_t espera = (p.via == 'E') ? 1500 : 9000;
    if (millis() - p.t < espera) continue;

    bool reintentar = true;
    if (p.via == 'E') {
      if (p.intentos < 2 && cerca()) p.intentos++;                               // otra vez cerca
      else if (loraOk && !p.otraVia) { p.via = 'L'; p.intentos = 1; p.otraVia = true; }  // pasar a LoRa
      else reintentar = false;
    } else {
      if (p.intentos < 3) p.intentos++;                                          // otro intento LoRa
      else if (cerca() && !p.otraVia) { p.via = 'E'; p.intentos = 1; p.otraVia = true; } // respaldo cercano
      else reintentar = false;
    }

    Msg* m = buscar(EQUIPO, p.id, true);
    if (!m) m = buscar(0, p.id, true);
    if (!reintentar) {
      if (m) { m->estado = FALLIDO; version++; }
      pendientes.erase(pendientes.begin() + i);
      i--;
      continue;
    }
    enviarPorVia(p.via, p.texto);
    p.t = millis();
    if (m && m->via != p.via) { m->via = p.via; version++; }
  }
}

// ---------------------------------------------------------------------------
// 9) ARCHIVOS (fotos y notas de voz) — solo por ESP-NOW, en trozos
//    f <id:2> <tipo:1> <tamaño:4> <ext>   inicio
//    d <id:2> <n:2> <datos>               trozo número n
//    a <id:2> <siguiente:2>               "ya tengo hasta n-1, mándame n"
// ---------------------------------------------------------------------------
struct Envio {
  bool activo = false; uint16_t id; uint8_t tipo; File f; uint32_t tam;
  uint16_t total, n; uint32_t t; uint16_t fallos; String ext;
} tx;
struct Recibo {
  bool activo = false; uint16_t id; uint8_t de; uint8_t tipo; File f;
  uint32_t tam, escritos; uint16_t siguiente; String ruta; uint32_t t; int rssi;
} rx;

void txMandarTrozo() {
  uint8_t b[250];
  if (tx.n == 0xFFFF) {                            // todavía no aceptó el inicio
    b[0] = 'f'; memcpy(b + 1, &tx.id, 2); b[3] = tx.tipo; memcpy(b + 4, &tx.tam, 4);
    size_t e = tx.ext.length();
    memcpy(b + 8, tx.ext.c_str(), e);
    espEnviar(b, 8 + e);
  } else {
    b[0] = 'd'; memcpy(b + 1, &tx.id, 2); memcpy(b + 3, &tx.n, 2);
    tx.f.seek((uint32_t)tx.n * CHUNK);
    size_t r = tx.f.read(b + 5, CHUNK);
    espEnviar(b, 5 + r);
  }
  tx.t = millis();
}

bool empezarEnvioArchivo(const String& ruta, uint8_t tipo, const String& ext) {
  if (tx.activo || !cerca()) return false;
  tx.f = LittleFS.open(ruta, "r");
  if (!tx.f) return false;
  tx.activo = true; tx.id = siguienteId++; tx.tipo = tipo; tx.tam = tx.f.size();
  tx.total = (tx.tam + CHUNK - 1) / CHUNK; tx.n = 0xFFFF; tx.fallos = 0; tx.ext = ext;
  Msg m{tx.id, EQUIPO, true, tipo, ENVIANDO, 'E', 0, 0, ruta};
  agregar(m);
  publicarMsg(m);
  txMandarTrozo();
  return true;
}

void txTerminar(bool ok) {
  tx.f.close();
  tx.activo = false;
  Msg* m = buscar(EQUIPO, tx.id, true);
  if (m) { m->estado = ok ? ENTREGADO : FALLIDO; m->progreso = ok ? 100 : m->progreso; version++; }
}

void txLoop() {
  if (!tx.activo) return;
  if (millis() - tx.t > 80) {                      // sin respuesta: repetir
    if (++tx.fallos > 60) { txTerminar(false); return; }
    txMandarTrozo();
  }
}

void procesarArchivo(const uint8_t* d, int len, int rssi) {
  if (len < 3) return;
  uint16_t id; memcpy(&id, d + 1, 2);
  if (d[0] == 'a' && tx.activo && id == tx.id && len >= 5) {   // soy el que envía
    uint16_t sig; memcpy(&sig, d + 3, 2);
    tx.fallos = 0;
    if (sig >= tx.total) { txTerminar(true); return; }
    if (tx.n == 0xFFFF || sig > tx.n) {
      tx.n = sig;
      Msg* m = buscar(EQUIPO, tx.id, true);
      uint8_t p = (uint32_t)sig * 100 / tx.total;
      if (m && m->progreso != p) { m->progreso = p; version++; }
    }
    txMandarTrozo();
    return;
  }
  uint8_t r[5] = {'a'};
  if (d[0] == 'f' && len >= 8) {                   // soy el que recibe: inicio
    if (!(rx.activo && rx.id == id)) {
      if (rx.activo) { rx.f.close(); LittleFS.remove(rx.ruta); }
      uint32_t tam; memcpy(&tam, d + 4, 4);
      String ext((const char*)(d + 8), len - 8);
      rx.activo = true; rx.id = id; rx.tipo = d[3]; rx.tam = tam; rx.escritos = 0;
      rx.siguiente = 0; rx.rssi = rssi;
      rx.ruta = "/m/r" + String(id) + "_" + String(millis() % 100000) + "." + ext;
      rx.f = LittleFS.open(rx.ruta, "w");
    }
    rx.t = millis();
    memcpy(r + 1, &id, 2); memcpy(r + 3, &rx.siguiente, 2);
    espEnviar(r, 5);
    return;
  }
  // (si ya terminó y el emisor repite el último trozo, solo se vuelve a confirmar)
  if (d[0] == 'd' && len >= 5 && id == rx.id && (rx.activo || rx.siguiente > 0)) {
    uint16_t n; memcpy(&n, d + 3, 2);
    if (rx.activo && n == rx.siguiente) {
      rx.f.write(d + 5, len - 5);
      rx.escritos += len - 5;
      rx.siguiente++;
    }
    rx.t = millis();
    memcpy(r + 1, &id, 2); memcpy(r + 3, &rx.siguiente, 2);
    espEnviar(r, 5);
    if (rx.activo && rx.escritos >= rx.tam) {      // ¡completo!
      rx.f.close();
      rx.activo = false;
      if (!buscar(OTRO, id, false)) {
        Msg m{id, OTRO, false, rx.tipo, RECIBIDO, 'E', (int16_t)rx.rssi, 100, rx.ruta};
        agregar(m);
        publicarMsg(m);
        parpadear(3);
        Serial.printf("[CERCA] %s recibida (%u bytes)\n", rx.tipo == FOTO ? "Foto" : "Nota de voz", rx.tam);
      }
    }
  }
}

void rxLoop() {                                    // recepción abandonada
  if (rx.activo && millis() - rx.t > 8000) {
    rx.f.close(); LittleFS.remove(rx.ruta); rx.activo = false;
  }
}

// ---------------------------------------------------------------------------
// 10) PÁGINA WEB DEL CELULAR
// ---------------------------------------------------------------------------
String jsonEsc(const String& s) {
  String o; o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  return o;
}

void apiEstado() {
  String j; j.reserve(4096);
  j = "{\"v\":" + String(version) + ",\"yo\":" + String(EQUIPO) + ",\"otro\":" + String(OTRO);
  j += ",\"cerca\":{\"on\":" + String(cerca() ? "true" : "false") + ",\"rssi\":" + String(espRssi) + "}";
  j += ",\"lora\":{\"ok\":" + String(loraOk ? "true" : "false") + ",\"on\":" + String(loraVivo() ? "true" : "false") +
       ",\"rssi\":" + String(loraRssi) + ",\"snr\":" + String(loraSnr) + "}";
  j += ",\"ocupado\":" + String(tx.activo ? "true" : "false");
  j += ",\"sos\":" + String(sosDe);
  j += ",\"clima\":{\"yo\":" + climaJson(climaYo) + ",\"otro\":" + climaJson(climaOtro) + "}";
  j += ",\"dist\":" + distJson();
  if (modoOnline) j += ",\"web\":" + String(mqtt.connected() ? "true" : "false");
  j += ",\"msgs\":[";
  for (size_t i = 0; i < msgs.size(); i++) {
    const Msg& m = msgs[i];
    if (i) j += ",";
    j += "{\"id\":" + String(m.id) + ",\"de\":" + String(m.de) + ",\"mio\":" + (m.mio ? "true" : "false") +
         ",\"t\":" + String(m.tipo) + ",\"e\":" + String(m.estado) + ",\"via\":\"" + String(m.via) +
         "\",\"rssi\":" + String(m.rssi) + ",\"p\":" + String(m.progreso) +
         ",\"x\":\"" + jsonEsc((m.tipo == AUDIO || m.tipo == FOTO) ? String("/f") + m.texto : m.texto) + "\"}";
  }
  j += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void apiEnviar() {
  String t = server.arg("t");
  uint8_t tipo = server.arg("sos") == "1" ? SOS : TEXTO;
  if (tipo == SOS && !t.length()) t = "SOS: necesito ayuda";
  enviarMensaje(tipo, t);
  server.send(200, "text/plain", "ok");
}

void apiSosVisto() { sosDe = 0; version++; server.send(200, "text/plain", "ok"); }

// --- Menú: modo sin internet / con internet ---
// Estado de la red: {"modo":"online|offline","ssid":"...","estado":"...","ip":"...","canal":6,"web":true}
void apiRed() {
  String est = !modoOnline ? "offline" : WiFi.status() == WL_CONNECTED ? "conectado" :
               WiFi.status() == WL_CONNECT_FAILED ? "clave" : WiFi.status() == WL_NO_SSID_AVAIL ? "noRed" : "conectando";
  String j = "{\"modo\":\"" + String(modoOnline ? "online" : "offline") + "\",\"ssid\":\"" + jsonEsc(redSsid) +
             "\",\"estado\":\"" + est + "\",\"ip\":\"" + (online() ? WiFi.localIP().toString() : String("")) +
             "\",\"canal\":" + String(canalActual) + ",\"web\":" + String(mqtt.connected() ? "true" : "false") + "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}
// Busca redes WiFi (en segundo plano): {"listo":false} o {"listo":true,"redes":[...]}
void apiEscanear() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED || (n >= 0 && server.arg("nuevo") == "1")) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    n = WIFI_SCAN_RUNNING;
  }
  if (n < 0) { server.send(200, "application/json", "{\"listo\":false}"); return; }
  String j = "{\"listo\":true,\"redes\":[";
  int k = 0;
  for (int i = 0; i < n && k < 20; i++) {
    if (WiFi.SSID(i).isEmpty()) continue;
    if (k++) j += ",";
    j += "{\"s\":\"" + jsonEsc(WiFi.SSID(i)) + "\",\"r\":" + String(WiFi.RSSI(i)) +
         ",\"seg\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  server.send(200, "application/json", j + "]}");
}
void apiModo() {
  if (server.arg("m") == "offline") ponerOffline();
  else if (redSsid.length()) ponerOnline(redSsid, redClave);   // reconectar a la red guardada
  else { server.send(400, "text/plain", "Primero elige una red"); return; }
  server.send(200, "text/plain", "ok");
}
void apiConectar() {
  String ssid = server.arg("ssid");
  if (!ssid.length()) { server.send(400, "text/plain", "Falta la red"); return; }
  server.send(200, "text/plain", "ok");           // responder antes: el celular puede desconectarse un momento
  delay(100);
  ponerOnline(ssid, server.arg("clave"));
}

// Subida de foto / audio desde el celular
File subida; String subidaRuta; bool subidaMal = false;
void apiSubidaDatos() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    String ext = server.arg("ext");
    if (ext.length() < 2 || ext.length() > 5) ext = "bin";
    subidaRuta = "/m/t" + String(millis() % 100000) + "." + ext;
    subida = LittleFS.open(subidaRuta, "w");
    subidaMal = !subida;
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (!subidaMal) {
      if (subida.size() + u.currentSize > MAX_ARCHIVO) subidaMal = true;
      else subida.write(u.buf, u.currentSize);
    }
  } else if (u.status == UPLOAD_FILE_END || u.status == UPLOAD_FILE_ABORTED) {
    if (subida) subida.close();
    if (u.status == UPLOAD_FILE_ABORTED) subidaMal = true;
  }
}
void apiSubidaFin() {
  uint8_t tipo = server.arg("tipo") == "foto" ? FOTO : AUDIO;
  String ext = server.arg("ext");
  if (subidaMal) { LittleFS.remove(subidaRuta); server.send(413, "text/plain", "Archivo muy grande (máx. 400 KB)"); return; }
  if (!empezarEnvioArchivo(subidaRuta, tipo, ext)) {
    LittleFS.remove(subidaRuta);
    server.send(409, "text/plain", "La otra estación está lejos u ocupada: fotos y audios solo viajan CERCA");
    return;
  }
  server.send(200, "text/plain", "ok");
}

void apiArchivo() {
  String ruta = server.uri().substring(2);         // quita "/f"
  if (!ruta.startsWith("/m/") || !LittleFS.exists(ruta)) { server.send(404, "text/plain", "no"); return; }
  String ct = "application/octet-stream";
  if (ruta.endsWith(".jpg")) ct = "image/jpeg";
  else if (ruta.endsWith(".m4a") || ruta.endsWith(".mp4")) ct = "audio/mp4";
  else if (ruta.endsWith(".aac")) ct = "audio/aac";
  else if (ruta.endsWith(".amr")) ct = "audio/amr";
  else if (ruta.endsWith(".3gp")) ct = "audio/3gpp";
  else if (ruta.endsWith(".ogg") || ruta.endsWith(".opus")) ct = "audio/ogg";
  else if (ruta.endsWith(".webm")) ct = "audio/webm";
  else if (ruta.endsWith(".mp3")) ct = "audio/mpeg";
  else if (ruta.endsWith(".wav")) ct = "audio/wav";
  File f = LittleFS.open(ruta, "r");
  server.streamFile(f, ct);
  f.close();
}

void iniciarWeb() {
  server.on("/", []() { server.send_P(200, "text/html", PAGINA); });
  server.on("/api", apiEstado);
  server.on("/hist", []() {                       // historial para la gráfica
    String j = "{\"yo\":" + histJson(histYo) + ",\"otro\":" + histJson(histOtro) + "}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", j);
  });
  server.on("/enviar", HTTP_POST, apiEnviar);
  server.on("/sosvisto", HTTP_POST, apiSosVisto);
  server.on("/red", apiRed);
  server.on("/escanear", apiEscanear);
  server.on("/modo", HTTP_POST, apiModo);
  server.on("/conectar", HTTP_POST, apiConectar);
  server.on("/subir", HTTP_POST, apiSubidaFin, apiSubidaDatos);
  // Respuestas para que el celular no crea que "no hay internet" y se desconecte
  server.on("/generate_204", []() { server.send(204); });
  server.on("/hotspot-detect.html", []() { server.send(200, "text/html", "Success"); });
  server.onNotFound([]() {
    if (server.uri().startsWith("/f/")) { apiArchivo(); return; }
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302);
  });
  server.begin();
  dns.start(53, "*", WiFi.softAPIP());             // cualquier dirección -> esta placa
}

// ---------------------------------------------------------------------------
// 11) MODO CON INTERNET: red WiFi + MQTT (puente con www.panarepara.com/radio)
// ---------------------------------------------------------------------------
void onMqtt(char* topic, byte* payload, unsigned int len) {
  String s; for (unsigned i = 0; i < len; i++) s += (char)payload[i];
  bool sos = String(topic).endsWith("/web/sos");
  if (!s.length()) s = "SOS";
  enviarMensaje(sos ? SOS : TEXTO, "🌐 " + s, 0);   // 🌐 = escrito en la web
}

void mqttLoop() {
  static uint32_t firmaAnt = 0, ultimoHist = 0;   // para publicar el historial solo si cambió
  if (!online()) return;
  if (!mqtt.connected()) {
    static uint32_t intento = 0;
    if (millis() - intento < 5000) return;
    intento = millis();
    String id = "radio-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (!mqtt.connect(id.c_str(), T("estado").c_str(), 0, true, "{\"base\":false}")) return;
    mqtt.subscribe(T("web/msg").c_str());
    mqtt.subscribe(T("web/sos").c_str());
    Serial.println("MQTT conectado");
    firmaAnt = 0;                                  // recién conectado: publicar el historial
  }
  mqtt.loop();
  // Historial del chat (últimos 20 mensajes), retenido: quien abre la web ve la conversación
  if (millis() - ultimoHist > 2000) {
    ultimoHist = millis();
    uint32_t firma = msgs.size();
    for (auto& m : msgs) firma = firma * 31 + m.id * 7 + m.estado * 3 + m.progreso;
    if (firma != firmaAnt) {
      firmaAnt = firma;
      String j = "[";
      size_t desde = msgs.size() > 20 ? msgs.size() - 20 : 0;
      for (size_t i = desde; i < msgs.size(); i++) { if (i > desde) j += ","; j += msgJson(msgs[i]); }
      j += "]";
      mqtt.publish(T("radio/historial").c_str(), j.c_str(), true);
    }
  }
  static uint32_t ultimo = 0;
  if (millis() - ultimo > 10000) {
    ultimo = millis();
    String j = "{\"base\":true,\"equipo\":" + String(EQUIPO) + ",\"otro\":" + String(OTRO) + ",\"cerca\":{\"on\":" + String(cerca() ? "true" : "false") + ",\"rssi\":" + String(espRssi) +
               "},\"lora\":{\"on\":" + String(loraVivo() ? "true" : "false") + ",\"rssi\":" + String(loraRssi) +
               ",\"snr\":" + String(loraSnr) + "},\"dist\":" + distJson() + ",\"clima\":{\"" + String(EQUIPO) + "\":" + climaJson(climaYo) +
               ",\"" + String(OTRO) + "\":" + climaJson(climaOtro) + "}}";
    mqtt.publish(T("estado").c_str(), j.c_str(), true);
  }
}

// Latido: P,<equipo>,<canal>,<con internet>. La otra radio usa el canal para seguirnos.
String tramaLatido() { return "P," + String(EQUIPO) + "," + String(canalActual) + "," + (online() ? "1" : "0"); }
uint32_t latidoLoraForzado = 0;                // para avisar enseguida un cambio de canal
void pedirLatidoLora() { latidoLoraForzado = 1; }

// Cambia el canal WiFi para coincidir con la otra radio cuando ella tiene internet
void seguirCanal(int canal, uint8_t de) {
  if (online() || canal < 1 || canal > 13 || canal == canalActual) return;
  canalActual = canal;
  prefs.putUChar("canal", canal);
  String nombre = "Radio-" + String(EQUIPO);
  WiFi.softAP(nombre.c_str(), CLAVE_AP, canal);
  Serial.printf("[WIFI] Canal cambiado a %d para seguir a Radio %d\n", canal, de);
  version++;
}

// Pasa a modo SIN internet (no borra mensajes ni historial)
void ponerOffline() {
  modoOnline = false;
  prefs.putBool("online", false);
  mqtt.disconnect();
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);                // desconecta del router, mantiene la red Radio-N
  Serial.println("[MODO] Sin internet");
  version++;
}

// Pasa a modo CON internet con la red indicada
void ponerOnline(const String& ssid, const String& clave) {
  redSsid = ssid; redClave = clave; modoOnline = true;
  prefs.putString("ssid", ssid); prefs.putString("clave", clave); prefs.putBool("online", true);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(false, false);
  WiFi.begin(ssid.c_str(), clave.c_str());
  Serial.printf("[MODO] Con internet: conectando a \"%s\"...\n", ssid.c_str());
  version++;
}

// Detecta cuándo se conecta o desconecta del router
void revisarRed() {
  static bool antes = false;
  bool ahora = online();
  if (ahora == antes) return;
  antes = ahora;
  if (ahora) {
    canalActual = WiFi.channel();
    prefs.putUChar("canal", canalActual);
    configTime(0, 0, "pool.ntp.org", "time.google.com");   // hora real para los mensajes
    Serial.printf("[WIFI] Conectado a \"%s\"  IP=%s  canal %d\n", redSsid.c_str(),
                  WiFi.localIP().toString().c_str(), canalActual);
    latidoLoraForzado = 1;                       // avisar el canal a la otra radio
  } else if (modoOnline) {
    Serial.println("[WIFI] Sin conexión con el router (reintentando...)");
  }
  version++;
}

// Un mensaje en JSON para la página web
String msgJson(const Msg& m) {
  String x = (m.tipo == AUDIO) ? "🎤 Nota de voz" : (m.tipo == FOTO) ? "📷 Foto" : m.texto;
  return "{\"de\":" + String(m.de) + ",\"id\":" + String(m.id) + ",\"t\":" + String(m.tipo) +
         ",\"e\":" + String(m.estado) + ",\"via\":\"" + String(m.via) + "\",\"ts\":" + String(m.ts) +
         ",\"x\":\"" + jsonEsc(x) + "\"}";
}

void publicarMsg(const Msg& m) {
  if (!online() || !mqtt.connected()) return;
  mqtt.publish(T("radio/msg").c_str(), msgJson(m).c_str());
}

// Lee el DHT11 cada 30 s y lo envía a la otra estación (cerca: ESP-NOW, lejos: LoRa)
// Lee el sensor de gas: promedio de 16 lecturas, en % de la escala (0-100)
float leerGas() {
  uint32_t suma = 0;
  for (int i = 0; i < 16; i++) { suma += analogRead(GAS_PIN); delay(2); }
  return (suma / 16.0f) * 100.0f / 4095.0f;
}

// Lee los sensores cada 30 s y envía los datos a la otra estación
// (cerca: ESP-NOW, lejos: LoRa). Si el gas supera GAS_ALARMA, envía SOS.
void manejarClima() {
#if USAR_DHT || USAR_GAS
  static uint32_t proximo = 3000;                 // primera lectura a los 3 s
  if ((int32_t)(millis() - proximo) < 0) return;
  proximo = millis() + CLIMA_CADA_MS;
  String ts = "x", hs = "x", gs = "x";
#if USAR_DHT
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    Serial.println("!! DHT11 sin lectura: revisa VCC->3V3, DAT->D4 (GPIO4) y GND");
    climaYo.okTH = false;
  } else {
    climaYo.okTH = true; climaYo.t = t; climaYo.h = h;
    ts = String(t, 1); hs = String(h, 0);
  }
#endif
#if USAR_GAS
  float g = leerGas();
  climaYo.okGas = true; climaYo.g = g;
  gs = String(g, 0);
  static bool alarmaGas = false;
  if (millis() < GAS_CALENTAR_MS) {
    Serial.println("[GAS] calentando el sensor...");
  } else if (!alarmaGas && g >= GAS_ALARMA) {
    alarmaGas = true;
    enviarMensaje(SOS, "SOS: gas detectado (" + gs + " %)");
  } else if (alarmaGas && g < GAS_ALARMA - 10) {
    alarmaGas = false;                            // se rearma al bajar del umbral
  }
#endif
  if (!climaYo.okTH && !climaYo.okGas) { version++; return; }
  climaYo.cuando = millis();
  agregarHist(histYo, climaYo);
  version++;
  Serial.printf("[CLIMA] %s C  %s %%  gas %s %%\n", ts.c_str(), hs.c_str(), gs.c_str());
  String trama = "C," + String(EQUIPO) + "," + ts + "," + hs + "," + gs;
  if ((PREFERIR_LORA && loraOk) || !cerca()) loraEnviar(trama);   // sensores: por LoRa
  else espEnviarTexto(trama);
#endif
}

// ---------------------------------------------------------------------------
// 12) SETUP
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== RADIO EQUIPO %d ===\n", EQUIPO);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BOTON_SOS, INPUT_PULLUP);
  siguienteId = esp_random() % 50000 + 1;         // al azar: evita repetidos tras reiniciar
#if USAR_DHT
  dht.begin();
#endif
#if USAR_GAS
  analogSetPinAttenuation(GAS_PIN, ADC_11db);     // rango completo 0-3.3 V
#endif

  // Memoria para fotos y audios (se borra al encender)
  if (!LittleFS.begin(true)) Serial.println("!! Error de memoria LittleFS");
  File dir = LittleFS.open("/m");
  if (dir) {
    std::vector<String> borrar;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) borrar.push_back(String("/m/") + f.name());
    for (auto& r : borrar) LittleFS.remove(r);
  } else LittleFS.mkdir("/m");

  loraIniciar();

  // Modo guardado (sin internet / con internet) y red WiFi elegida en el menú
  prefs.begin("radio", false);
  modoOnline  = prefs.getBool("online", false);
  redSsid     = prefs.getString("ssid", "");
  redClave    = prefs.getString("clave", "");
  canalActual = prefs.getUChar("canal", CANAL_WIFI);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqtt);
  mqtt.setBufferSize(8192);                       // espacio para el historial del chat

  // WiFi: red propia para el celular (+ conexión al router en modo con internet)
  WiFi.mode(WIFI_AP_STA);
  WiFi.persistent(false);                         // la red la guardamos nosotros (Preferences)
  String nombre = "Radio-" + String(EQUIPO);
  WiFi.softAP(nombre.c_str(), CLAVE_AP, canalActual);
  if (modoOnline && redSsid.length()) {
    WiFi.setAutoReconnect(true);
    WiFi.begin(redSsid.c_str(), redClave.c_str()); // no bloquea: la conexión sigue en segundo plano
    Serial.printf("[MODO] Con internet: conectando a \"%s\"...\n", redSsid.c_str());
  } else {
    modoOnline = false;
    Serial.println("[MODO] Sin internet");
  }
  esp_wifi_set_ps(WIFI_PS_NONE);
  Serial.printf("WiFi para el celular: %s / %s -> abre http://192.168.4.1\n", nombre.c_str(), CLAVE_AP);

  // ESP-NOW
  colaEsp = xQueueCreate(24, sizeof(RxEsp));
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onEspNow);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_AP;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  } else Serial.println("!! Error iniciando ESP-NOW");

  iniciarWeb();
  latidoLoraForzado = 1;                         // anunciarse por LoRa al encender
  Serial.println("Listo.");
}

// ---------------------------------------------------------------------------
// 13) LOOP
// ---------------------------------------------------------------------------
void loop() {
  dns.processNextRequest();
  server.handleClient();

  // Lo que llegó por ESP-NOW
  RxEsp r;
  while (xQueueReceive(colaEsp, &r, 0) == pdTRUE) {
    bool antes = cerca();
    ultimoEspNow = millis();
    espRssi = r.rssi;
    promediar(espProm, r.rssi);
    if (!antes) version++;
    char c = r.d[0];
    if (c == 'f' || c == 'd' || c == 'a') procesarArchivo(r.d, r.len, r.rssi);
    else procesarTramaTexto(String((const char*)r.d, r.len), 'E', r.rssi);
  }

  loraLoop();
  manejarClima();
  revisarPendientes();
  txLoop();
  rxLoop();

  // Latidos: "estoy aquí"
  // LoRa: cada 30 s; cada 10 s si no hay enlace cercano (para reenganchar el canal);
  // un latido pedido se envía como máximo cada 8 s (evita rebotes entre las radios)
  static uint32_t latidoEsp = 0, latidoLora = 0;
  if (millis() - latidoEsp > 2000) { latidoEsp = millis(); espEnviarTexto(tramaLatido()); }
  uint32_t periodo = (cerca() ? 30000 : 10000) + EQUIPO * 1500;
  if ((latidoLoraForzado && millis() - latidoLora > 8000) || millis() - latidoLora > periodo) {
    latidoLoraForzado = 0; latidoLora = millis(); loraEnviar(tramaLatido());
  }

  // Estado de los enlaces en el Monitor serie cada 30 s
  static uint32_t ultimoInforme = 0;
  if (millis() - ultimoInforme > 30000) {
    ultimoInforme = millis();
    Serial.printf("[ENLACE] canal %d · LoRa: %s (%d dBm) · cercano: %s · internet: %s\n", canalActual,
                  loraVivo() ? "OK" : "sin señal", loraRssi, cerca() ? "OK" : "no", online() ? "sí" : "no");
  }

  // ¿Se perdió el enlace cercano? avisar a la web
  static bool cercaAntes = false;
  if (cerca() != cercaAntes) { cercaAntes = cerca(); version++; }

  // Botón BOOT: pulsación de 2 s = SOS
  static uint32_t pulsado = 0;
  if (digitalRead(BOTON_SOS) == LOW) {
    if (!pulsado) pulsado = millis();
    else if (pulsado != 1 && millis() - pulsado > 2000) {
      enviarMensaje(SOS, "SOS: botón de emergencia");
      pulsado = 1;                                 // no repetir hasta soltar
    }
  } else pulsado = 0;

  // LED: parpadeo rápido si hay un SOS sin atender
  if (sosDe && millis() < sosHasta) digitalWrite(LED_PIN, (millis() / 120) % 2);
  else if (sosDe) { sosDe = 0; digitalWrite(LED_PIN, LOW); }

  // Mensajes escritos en el Monitor Serie
  static String linea;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { if (linea.length()) enviarMensaje(TEXTO, linea); linea = ""; }
    else linea += c;
  }

  revisarRed();
  mqttLoop();
}
