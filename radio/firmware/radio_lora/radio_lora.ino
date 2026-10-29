/*
  ============================================================================
   RADIO HÍBRIDA ESP32 + LoRa (REYAX RYLR998)
  ============================================================================
   Cada estación = 1 ESP32 + 1 RYLR998. Tu celular se conecta al WiFi que
   crea el ESP32 y abre un chat en http://192.168.4.1

   - CERCA  (ESP-NOW, ~100-200 m): texto, notas de voz y fotos.
   - LEJOS  (LoRa, varios km):     texto y SOS.
   - El cambio entre CERCA y LEJOS es automático.
   - Opcional: la estación BASE se conecta al WiFi de tu casa y publica todo
     en una página web (GitHub Pages) mediante MQTT.

   Arduino IDE: placa "ESP32 Dev Module", núcleo ESP32 3.x
   Librería: "PubSubClient" (solo si ES_BASE = 1)
  ============================================================================
*/

// ---------------------------------------------------------------------------
// 1) CONFIGURACIÓN
// ---------------------------------------------------------------------------
#define EQUIPO   1          // 1 en una placa, 2 en la otra
#define ES_BASE  0          // 1 = se conecta al WiFi de casa y a la web (solo UNA placa)

// WiFi que crea la placa para tu celular
const char* CLAVE_AP = "radio1234";        // mínimo 8 caracteres
#define CANAL_WIFI 1                       // igual en las dos placas (ver guía)

// Solo para la BASE
const char* WIFI_SSID    = "TU_WIFI";
const char* WIFI_PASS    = "TU_CLAVE";
const char* MQTT_HOST    = "broker.hivemq.com";
const int   MQTT_PORT    = 1883;
const char* TOPIC_PREFIX = "panarepara-radio-acbdcce2";  // ¡cámbialo! igual en la web

// LoRa (RYLR998): debe ser igual en las dos placas
#define LORA_RX_PIN  16                    // RX2 del ESP32 <- TXD del RYLR998
#define LORA_TX_PIN  17                    // TX2 del ESP32 -> RXD del RYLR998
#define LORA_BAUD    115200
#define LORA_RED     18                    // NETWORKID (0-15 o 18)
#define LORA_BANDA   "915000000"           // Hz (verifica la banda permitida en tu país)
#define LORA_PARAM   "9,7,1,12"            // SF9, 125 kHz, CR 4/5, preámbulo 12
#define LORA_CLAVE   "4F7A21C9"            // cifrado: 8 dígitos hex, igual en ambas

#define LED_PIN   2                        // LED azul de la placa
#define BOTON_SOS 0                         // botón BOOT: mantener 2 s = SOS

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
#if ES_BASE
#include <PubSubClient.h>
#endif

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
};
std::vector<Msg> msgs;
uint32_t version = 1;          // cambia cada vez que algo cambia (para la web)

// Mensajes de texto esperando confirmación
struct Pendiente {
  uint16_t id; uint8_t tipo; String texto;
  char via; uint8_t intentos; uint32_t t;
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

#if ES_BASE
WiFiClient net;
PubSubClient mqtt(net);
String T(const char* s) { return String(TOPIC_PREFIX) + "/" + s; }
#endif

bool cerca() { return ultimoEspNow && millis() - ultimoEspNow < CERCA_MS; }
bool loraVivo() { return ultimoLora && millis() - ultimoLora < LORA_VIVO_MS; }

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

  if (tipoTrama == 'P') return;                   // latido: ya se anotó la señal

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
  char via = cerca() ? 'E' : 'L';
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
      if (p.intentos < 2 && cerca()) p.intentos++;             // otra vez cerca
      else if (loraOk) { p.via = 'L'; p.intentos = 1; }        // saltar a LoRa
      else reintentar = false;
    } else {
      if (p.intentos < 3) p.intentos++;
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
#if ES_BASE
  j += ",\"web\":" + String(mqtt.connected() ? "true" : "false");
#endif
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
  server.on("/enviar", HTTP_POST, apiEnviar);
  server.on("/sosvisto", HTTP_POST, apiSosVisto);
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
// 11) MQTT (solo BASE): puente con la página de GitHub
// ---------------------------------------------------------------------------
#if ES_BASE
void onMqtt(char* topic, byte* payload, unsigned int len) {
  String s; for (unsigned i = 0; i < len; i++) s += (char)payload[i];
  bool sos = String(topic).endsWith("/web/sos");
  if (!s.length()) s = "SOS";
  enviarMensaje(sos ? SOS : TEXTO, "🌐 " + s, 0);   // 🌐 = escrito en la web
}

void mqttLoop() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!mqtt.connected()) {
    static uint32_t intento = 0;
    if (millis() - intento < 5000) return;
    intento = millis();
    String id = "radio-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (!mqtt.connect(id.c_str(), T("estado").c_str(), 0, true, "{\"base\":false}")) return;
    mqtt.subscribe(T("web/msg").c_str());
    mqtt.subscribe(T("web/sos").c_str());
    Serial.println("MQTT conectado");
  }
  mqtt.loop();
  static uint32_t ultimo = 0;
  if (millis() - ultimo > 10000) {
    ultimo = millis();
    String j = "{\"base\":true,\"cerca\":{\"on\":" + String(cerca() ? "true" : "false") + ",\"rssi\":" + String(espRssi) +
               "},\"lora\":{\"on\":" + String(loraVivo() ? "true" : "false") + ",\"rssi\":" + String(loraRssi) +
               ",\"snr\":" + String(loraSnr) + "}}";
    mqtt.publish(T("estado").c_str(), j.c_str(), true);
  }
}
#endif

void publicarMsg(const Msg& m) {
#if ES_BASE
  if (!mqtt.connected()) return;
  String x = (m.tipo == AUDIO) ? "🎤 Nota de voz" : (m.tipo == FOTO) ? "📷 Foto" : m.texto;
  String j = "{\"de\":" + String(m.de) + ",\"id\":" + String(m.id) + ",\"t\":" + String(m.tipo) +
             ",\"via\":\"" + String(m.via) + "\",\"x\":\"" + jsonEsc(x) + "\"}";
  mqtt.publish(T("radio/msg").c_str(), j.c_str());
#endif
}

// ---------------------------------------------------------------------------
// 12) SETUP
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== RADIO EQUIPO %d %s===\n", EQUIPO, ES_BASE ? "(BASE) " : "");
  pinMode(LED_PIN, OUTPUT);
  pinMode(BOTON_SOS, INPUT_PULLUP);
  siguienteId = esp_random() % 50000 + 1;         // al azar: evita repetidos tras reiniciar

  // Memoria para fotos y audios (se borra al encender)
  if (!LittleFS.begin(true)) Serial.println("!! Error de memoria LittleFS");
  File dir = LittleFS.open("/m");
  if (dir) {
    std::vector<String> borrar;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) borrar.push_back(String("/m/") + f.name());
    for (auto& r : borrar) LittleFS.remove(r);
  } else LittleFS.mkdir("/m");

  loraIniciar();

  // WiFi: punto de acceso para el celular (+ conexión a casa si es BASE)
  WiFi.mode(WIFI_AP_STA);
  uint8_t canal = CANAL_WIFI;
#if ES_BASE
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando al WiFi de casa");
  for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) { delay(250); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    canal = WiFi.channel();
    Serial.printf("WiFi OK. >>> CANAL %d: ponlo en CANAL_WIFI de la otra placa\n", canal);
  } else Serial.println("Sin WiFi de casa: la radio funciona igual, sin la web");
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqtt);
  mqtt.setBufferSize(512);
#endif
  String nombre = "Radio-" + String(EQUIPO);
  WiFi.softAP(nombre.c_str(), CLAVE_AP, canal);
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
    if (!antes) version++;
    char c = r.d[0];
    if (c == 'f' || c == 'd' || c == 'a') procesarArchivo(r.d, r.len, r.rssi);
    else procesarTramaTexto(String((const char*)r.d, r.len), 'E', r.rssi);
  }

  loraLoop();
  revisarPendientes();
  txLoop();
  rxLoop();

  // Latidos: "estoy aquí"
  static uint32_t latidoEsp = 0, latidoLora = 0;
  if (millis() - latidoEsp > 2000) { latidoEsp = millis(); espEnviarTexto("P," + String(EQUIPO)); }
  if (millis() - latidoLora > 30000 + EQUIPO * 1500) { latidoLora = millis(); loraEnviar("P," + String(EQUIPO)); }

  // ¿Se perdió el enlace cercano? avisar a la web
  static bool cercaAntes = false;
  if (cerca() != cercaAntes) { cercaAntes = cerca(); version++; }

  // Botón BOOT: mantener 2 s = SOS
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

#if ES_BASE
  mqttLoop();
#endif
}
