// Página del chat que el ESP32 sirve al celular (http://192.168.4.1)
// Se guarda en la memoria de programa (PROGMEM) para no gastar RAM.
#pragma once
const char PAGINA[] PROGMEM = R"PAGINA(
<!doctype html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Radio</title>
<style>
:root{--bg:#0f1720;--card:#17212c;--line:#253241;--tx:#e8eef4;--mu:#8a9bab;--yo:#1e4a6e;--ok:#3fd17a;--bad:#ff5d5d;--lo:#ffb23b}
*{box-sizing:border-box}html,body{height:100%}
body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.4 system-ui,-apple-system,Roboto,sans-serif;display:flex;flex-direction:column}
header{padding:10px 12px;border-bottom:1px solid var(--line);display:flex;flex-direction:column;gap:8px}
.top{display:flex;align-items:center;justify-content:space-between}
h1{font-size:17px;margin:0}
.chips{display:flex;gap:6px;flex-wrap:wrap}
.chip{font-size:12px;padding:4px 9px;border-radius:99px;background:var(--line);color:var(--mu)}
.chip.on{background:rgba(63,209,122,.15);color:var(--ok)}
.chip.lo{background:rgba(255,178,59,.15);color:var(--lo)}
#sosBtn{background:var(--bad);color:#fff;border:0;border-radius:10px;padding:7px 12px;font-weight:700}
#alerta{display:none;background:var(--bad);color:#fff;padding:12px;font-weight:700;justify-content:space-between;align-items:center;animation:p 1s infinite}
#alerta button{background:#fff;color:var(--bad);border:0;border-radius:8px;padding:6px 12px;font-weight:700}
@keyframes p{50%{opacity:.75}}
#lista{flex:1;overflow-y:auto;padding:12px;display:flex;flex-direction:column;gap:8px}
.m{max-width:82%;padding:8px 11px;border-radius:14px;background:var(--card);word-wrap:break-word}
.m.yo{align-self:flex-end;background:var(--yo)}
.m.sos{background:#5c1a1a;border:1px solid var(--bad)}
.m small{display:block;font-size:11px;color:var(--mu);margin-top:3px}
.m img{max-width:100%;border-radius:8px;display:block}
.m audio{width:230px;max-width:100%}
.vacio{color:var(--mu);text-align:center;margin:auto;font-size:14px}
form{display:flex;gap:6px;padding:8px;border-top:1px solid var(--line)}
input[type=text]{flex:1;min-width:0;background:var(--card);border:1px solid var(--line);color:var(--tx);border-radius:12px;padding:11px;font:inherit}
.b{background:var(--card);border:1px solid var(--line);color:var(--tx);border-radius:12px;padding:0 12px;font-size:18px}
.b:disabled{opacity:.35}
#env{background:#3ba7ff;color:#04121f;border:0;font-weight:700;font-size:15px}
#graf{border-bottom:1px solid var(--line);padding:8px 12px}
#graf .tabs{display:flex;gap:6px;align-items:center}
#graf .tab{font-size:12px;padding:4px 10px;border-radius:99px;background:var(--line);color:var(--mu);border:0}
#graf .tab.sel{background:#3ba7ff;color:#04121f;font-weight:700}
#graf .ley{margin-left:auto;font-size:11px;color:var(--mu)}
#graf svg{width:100%;height:150px;display:block;margin-top:6px}
#graf.cerrado svg,#graf.cerrado .tab:not(#gTog){display:none}
#menuBtn{background:var(--card);border:1px solid var(--line);color:var(--tx);border-radius:10px;padding:6px 11px;font-size:16px;margin-right:8px}
#menu{position:fixed;inset:0;background:var(--bg);z-index:10;overflow-y:auto;padding:18px 16px 30px}
#menu.oculto{display:none}
#menu h2{margin:0;font-size:22px}
#menu .sub{color:var(--mu);font-size:14px;margin:4px 0 16px}
.op{display:block;width:100%;text-align:left;background:var(--card);border:1px solid var(--line);color:var(--tx);border-radius:14px;padding:16px;margin-bottom:12px;font:inherit}
.op b{font-size:17px;display:block;margin-bottom:4px}
.op span{color:var(--mu);font-size:14px}
.op.act{border-color:#3ba7ff;box-shadow:0 0 0 1px #3ba7ff}
.op .tag{float:right;font-size:11px;font-weight:700;color:#04121f;background:#3ba7ff;border-radius:99px;padding:2px 8px}
.nota{font-size:13px;color:var(--mu);margin-top:6px}
.btn{display:block;width:100%;border:0;border-radius:12px;padding:13px;font:inherit;font-weight:700;margin-top:10px;background:#3ba7ff;color:#04121f}
.btn.sec{background:var(--card);color:var(--tx);border:1px solid var(--line)}
.volver{background:none;border:0;color:#3ba7ff;font:inherit;padding:0;margin-bottom:12px}
.estado{background:var(--card);border-radius:12px;padding:12px;font-size:14px;margin-bottom:12px}
.estado.ok{border:1px solid var(--ok)} .estado.mal{border:1px solid var(--bad)}
.red{display:flex;justify-content:space-between;width:100%;background:var(--card);border:1px solid var(--line);color:var(--tx);border-radius:10px;padding:12px;margin-bottom:6px;font:inherit}
.red.sel{border-color:#3ba7ff}
#clave{width:100%;background:var(--card);border:1px solid var(--line);color:var(--tx);border-radius:12px;padding:12px;font:inherit;margin-top:6px}
#aviso{display:none;position:fixed;left:12px;right:12px;bottom:70px;background:#000c;padding:10px 12px;border-radius:10px;font-size:14px}
</style></head><body>
<header>
  <div class="top"><div style="display:flex;align-items:center"><button id="menuBtn" title="Menú">☰</button><h1 id="tit">Radio</h1></div><button id="sosBtn">SOS</button></div>
  <div class="chips"><span id="cCerca" class="chip">⚡ Cerca: —</span><span id="cLora" class="chip">📡 LoRa: —</span><span id="cWeb" class="chip" style="display:none">🌐 Web</span><span id="cClima" class="chip">🌡 Clima: —</span><span id="cDist" class="chip">📏 Distancia: —</span></div>
</header>
<div id="menu">
  <div id="vMain">
    <h2 id="mTit">Radio</h2>
    <div class="sub">Elige cómo quieres usar la radio</div>
    <button class="op" id="opOff"><b>📴 Sin internet</b><span>Chat, alertas y sensores entre las dos radios por ESP-NOW (cerca) y LoRa (lejos).</span></button>
    <button class="op" id="opOn"><b>🌐 Con internet</b><span id="opOnTxt">Conecta esta radio a una red WiFi para ver todo en www.panarepara.com/radio.</span></button>
    <button class="btn" id="seguir" style="display:none">Continuar al chat →</button>
    <div class="nota">Puedes volver a este menú con ☰ y cambiar de modo cuando quieras: los mensajes, la gráfica y los datos se conservan.</div>
  </div>
  <div id="vOnline" style="display:none">
    <button class="volver" id="volver">← Volver al menú</button>
    <h2>🌐 Con internet</h2>
    <div class="sub">Elige la red WiFi (2.4 GHz) a la que se conectará la radio</div>
    <div class="estado" id="estRed">Comprobando…</div>
    <button class="btn" id="irChat" style="display:none">Ir al chat →</button>
    <button class="btn sec" id="reconectar" style="display:none"></button>
    <div id="redes" style="margin-top:14px"></div>
    <div id="claveBox" style="display:none"><input id="clave" type="password" placeholder="Clave del WiFi" autocomplete="off"><button class="btn" id="conectar">Conectar</button></div>
    <button class="btn sec" id="buscar">🔄 Buscar redes de nuevo</button>
    <div class="nota">Al conectarse, la red Radio puede cambiar de canal: si el celular se desconecta, vuelve a conectarlo a la red Radio y abre 192.168.4.1.</div>
  </div>
</div>
<div id="graf"><div class="tabs"><button class="tab sel" id="gTog">📈 Gráfica</button><button class="tab sel" data-k="1">Temp.</button><button class="tab" data-k="2">Humedad</button><button class="tab" data-k="3">Gas</button><span class="ley"><b style="color:#3ba7ff">━</b> Aquí <b style="color:#ffb23b">━</b> Otro</span></div><svg id="gSvg"></svg></div>
<div id="alerta"><span id="alertaTxt">🚨 SOS</span><button id="visto">Visto</button></div>
<div id="lista"><div class="vacio" id="vacio">Aún no hay mensajes.<br>Escribe abajo para empezar.</div></div>
<div id="aviso"></div>
<form id="f">
  <button type="button" class="b" id="bFoto" title="Foto">📷</button>
  <button type="button" class="b" id="bAudio" title="Nota de voz">🎤</button>
  <input type="text" id="txt" maxlength="160" placeholder="Mensaje…" autocomplete="off">
  <button class="b" id="env">Enviar</button>
</form>
<input type="file" id="iFoto" accept="image/*" capture="environment" hidden>
<input type="file" id="iAudio" accept="audio/*" capture hidden>
<script>
// --- utilidades ---
const $ = id => document.getElementById(id);
let estado = null, vistos = {}, nodos = {};

function aviso(t, ms = 3500) { const a = $("aviso"); a.textContent = t; a.style.display = "block"; clearTimeout(a._t); a._t = setTimeout(() => a.style.display = "none", ms); }
function nombre(n) { return n == 0 ? "Web" : "Radio " + n; }
function hora(k) { if (!vistos[k]) vistos[k] = new Date(); return vistos[k].toLocaleTimeString("es-PA", { hour: "2-digit", minute: "2-digit" }); }

// --- cómo se dibuja cada mensaje ---
function pie(m) {
  const via = m.via == "E" ? "⚡ cerca" : m.via == "L" ? "📡 LoRa" : "";
  let est = "";
  if (m.mio) est = m.e == 0 ? (m.t >= 2 ? "⏳ " + m.p + "%" : "⏳ enviando") : m.e == 1 ? "✓ entregado" : "✕ no entregado";
  else est = m.rssi ? m.rssi + " dBm" : "";
  return [m.mio ? "Tú" : nombre(m.de), hora(m.mio + "-" + m.de + "-" + m.id), via, est].filter(Boolean).join(" · ");
}

function crear(m) {
  const d = document.createElement("div");
  d.className = "m" + (m.mio ? " yo" : "") + (m.t == 1 ? " sos" : "");
  if (m.t == 3) { const i = document.createElement("img"); i.src = m.x; i.onload = () => { const l = $("lista"); l.scrollTop = l.scrollHeight; }; d.append(i); }
  else if (m.t == 2) { const a = document.createElement("audio"); a.controls = true; a.preload = "none"; a.src = m.x; d.append(a); }
  else { const s = document.createElement("div"); s.textContent = (m.t == 1 ? "🚨 " : "") + m.x; d.append(s); }
  const p = document.createElement("small"); d.append(p); d._pie = p;
  return d;
}

// --- dibujar el estado completo ---
function pintar(j) {
  $("tit").textContent = "Radio " + j.yo;
  const c = $("cCerca"); c.textContent = "⚡ Cercano (fotos/voz): " + (j.cerca.on ? j.cerca.rssi + " dBm" : "no disponible"); c.className = "chip" + (j.cerca.on ? " on" : "");
  const l = $("cLora");
  l.textContent = "📡 LoRa (principal): " + (!j.lora.ok ? "sin módulo" : j.lora.on ? j.lora.rssi + " dBm · SNR " + j.lora.snr : "esperando…");
  l.className = "chip" + (j.lora.on ? " lo" : "");
  if ("web" in j) { const w = $("cWeb"); w.style.display = ""; w.className = "chip" + (j.web ? " on" : ""); w.textContent = "🌐 Web: " + (j.web ? "conectada" : "sin conexión"); }
  const cl = j.clima || {}, fc = x => !x ? "sin datos" : [x.t != null ? x.t + "°C · " + x.h + "%" : "", x.g != null ? "gas " + x.g + "%" : ""].filter(Boolean).join(" · ");
  $("cClima").textContent = "🌡 Aquí: " + fc(cl.yo) + " | Radio " + j.otro + ": " + fc(cl.otro);
  $("cClima").className = "chip" + (cl.yo || cl.otro ? " on" : "");
  const fDist = m => m < 100 ? "~" + Math.round(m) + " m" : m < 1000 ? "~" + Math.round(m / 10) * 10 + " m" : "~" + (m / 1000).toFixed(1) + " km";
  $("cDist").textContent = "📏 Distancia aprox.: " + (j.dist ? fDist(j.dist.m) + (j.dist.via == "E" ? " (⚡)" : " (📡)") : "sin enlace");
  $("cDist").className = "chip" + (j.dist ? " on" : "");
  const medios = j.cerca.on && !j.ocupado;
  $("bFoto").disabled = $("bAudio").disabled = !medios;

  const al = $("alerta");
  if (j.sos) { $("alertaTxt").textContent = "🚨 SOS de " + nombre(j.sos); al.style.display = "flex"; if (navigator.vibrate) navigator.vibrate([300, 150, 300]); }
  else al.style.display = "none";

  const lista = $("lista"), vivos = {};
  let abajo = lista.scrollHeight - lista.scrollTop - lista.clientHeight < 60;
  for (const m of j.msgs) {
    const k = m.mio + "-" + m.de + "-" + m.id; vivos[k] = 1;
    if (!nodos[k]) { nodos[k] = crear(m); lista.append(nodos[k]); abajo = true; }
    nodos[k]._pie.textContent = pie(m);
  }
  for (const k in nodos) if (!vivos[k]) { nodos[k].remove(); delete nodos[k]; }
  $("vacio").style.display = j.msgs.length ? "none" : "";
  if (abajo) lista.scrollTop = lista.scrollHeight;
}

// --- preguntar a la placa cada segundo ---
async function actualizar() {
  try { const r = await fetch("/api"); estado = await r.json(); pintar(estado); }
  catch (e) { $("cCerca").textContent = "⚠ Sin conexión con la placa"; }
  setTimeout(actualizar, 1000);
}

// --- enviar texto y SOS ---
function post(url, datos) { return fetch(url, { method: "POST", body: new URLSearchParams(datos) }); }

$("f").onsubmit = e => {
  e.preventDefault();
  const t = $("txt").value.trim(); if (!t) return;
  post("/enviar", { t }); $("txt").value = "";
};
$("sosBtn").onclick = () => { if (confirm("¿Enviar SOS a la otra estación?")) post("/enviar", { sos: 1, t: "" }); };
$("visto").onclick = () => post("/sosvisto", {});

// --- fotos y notas de voz ---
function subir(blob, tipo, ext) {
  if (blob.size > 400000) { aviso("Muy grande (" + Math.round(blob.size / 1024) + " KB). Máximo 400 KB: graba menos tiempo."); return; }
  const fd = new FormData(); fd.append("archivo", blob, "a." + ext);
  const x = new XMLHttpRequest();
  x.open("POST", "/subir?tipo=" + tipo + "&ext=" + ext);
  x.upload.onprogress = e => aviso("Cargando… " + Math.round(e.loaded * 100 / e.total) + "%", 2000);
  x.onload = () => aviso(x.status == 200 ? "Enviando por radio…" : x.responseText);
  x.onerror = () => aviso("Error al cargar el archivo");
  x.send(fd);
}

$("bFoto").onclick = () => $("iFoto").click();
$("bAudio").onclick = () => $("iAudio").click();
$("iFoto").onchange = e => {
  const f = e.target.files[0]; e.target.value = ""; if (!f) return;
  const img = new Image();
  img.onload = () => {   // reducir la foto a 800 px y comprimir en JPEG
    const k = Math.min(1, 800 / Math.max(img.width, img.height));
    const c = document.createElement("canvas"); c.width = img.width * k; c.height = img.height * k;
    c.getContext("2d").drawImage(img, 0, 0, c.width, c.height);
    c.toBlob(b => subir(b, "foto", "jpg"), "image/jpeg", 0.6);
    URL.revokeObjectURL(img.src);
  };
  img.src = URL.createObjectURL(f);
};
$("iAudio").onchange = e => {
  const f = e.target.files[0]; e.target.value = ""; if (!f) return;
  let ext = (f.name.split(".").pop() || "m4a").toLowerCase().slice(0, 5);
  subir(f, "audio", ext);
};
// --- gráfica (sin internet: se dibuja con SVG) ---
let gK = 1, gDatos = null;
const gUni = { 1: "°C", 2: "%", 3: "%" };
function graficar() {
  const svg = $("gSvg"), W = svg.clientWidth || 360, H = 150, L = 30, R = 8, T = 8, B = 18;
  svg.setAttribute("viewBox", "0 0 " + W + " " + H);
  if (!gDatos) { svg.innerHTML = ""; return; }
  const ser = [["yo", "#3ba7ff"], ["otro", "#ffb23b"]].map(([n, c]) => ({ c, p: (gDatos[n] || []).filter(q => q[gK] != null).map(q => [-q[0] / 60, q[gK]]) }));
  const ys = ser.flatMap(s => s.p.map(q => q[1]));
  let h = '<text x="' + W / 2 + '" y="' + H / 2 + '" fill="#8a9bab" font-size="12" text-anchor="middle">Sin datos todavía</text>';
  if (ys.length) {
    let lo = Math.min(...ys), hi = Math.max(...ys); if (hi - lo < 2) { lo -= 1; hi += 1; }
    const xs = v => L + (v + 60) / 60 * (W - L - R), yy = v => T + (hi - v) / (hi - lo) * (H - T - B);
    h = "";
    for (let i = 0; i <= 2; i++) { const v = lo + (hi - lo) * i / 2, y = yy(v).toFixed(1);
      h += '<line x1="' + L + '" x2="' + (W - R) + '" y1="' + y + '" y2="' + y + '" stroke="#253241"/><text x="' + (L - 4) + '" y="' + (+y + 4) + '" fill="#8a9bab" font-size="10" text-anchor="end">' + v.toFixed(gK == 1 ? 1 : 0) + '</text>'; }
    [-60, -45, -30, -15, 0].forEach(m => h += '<text x="' + xs(m) + '" y="' + (H - 4) + '" fill="#8a9bab" font-size="10" text-anchor="' + (m ? "middle" : "end") + '">' + (m ? m + " min" : "ahora") + '</text>');
    ser.forEach(s => { if (!s.p.length) return;
      h += '<polyline fill="none" stroke="' + s.c + '" stroke-width="2" points="' + s.p.map(q => xs(Math.max(q[0], -60)).toFixed(1) + "," + yy(q[1]).toFixed(1)).join(" ") + '"/>';
      const u = s.p[s.p.length - 1]; h += '<circle cx="' + xs(u[0]) + '" cy="' + yy(u[1]) + '" r="3" fill="' + s.c + '"/>'; });
    h += '<text x="' + (W - R) + '" y="' + (T + 10) + '" fill="#8a9bab" font-size="10" text-anchor="end">' + gUni[gK] + '</text>';
  }
  svg.innerHTML = h;
}
async function cargarHist() {
  if (!$("graf").classList.contains("cerrado")) {
    try { gDatos = await (await fetch("/hist")).json(); graficar(); } catch (e) {}
  }
  setTimeout(cargarHist, 30000);
}
document.querySelectorAll("#graf .tab[data-k]").forEach(b => b.onclick = () => {
  gK = +b.dataset.k; document.querySelectorAll("#graf .tab[data-k]").forEach(x => x.classList.toggle("sel", x == b)); graficar(); });
$("gTog").onclick = () => { const g = $("graf"); g.classList.toggle("cerrado"); $("gTog").classList.toggle("sel"); if (!g.classList.contains("cerrado")) fetch("/hist").then(r => r.json()).then(d => { gDatos = d; graficar(); }).catch(() => {}); };
// --- menú: modo sin internet / con internet ---
let red = null, redSel = "", tRed = null, tScan = null;
function verMenu(v) {                      // v: "main", "online" o null (chat)
  $("menu").classList.toggle("oculto", !v);
  $("vMain").style.display = v == "main" ? "" : "none";
  $("vOnline").style.display = v == "online" ? "" : "none";
  clearTimeout(tRed); clearTimeout(tScan);
  if (v) leerRed();
  if (v == "online") escanear(false);
}
async function leerRed() {
  try { red = await (await fetch("/red")).json(); pintarRed(); } catch (e) {}
  if (!$("menu").classList.contains("oculto")) tRed = setTimeout(leerRed, 2000);
}
function pintarRed() {
  if (!red) return;
  const on = red.modo == "online";
  $("mTit").textContent = estado ? "Radio " + estado.yo : "Radio";
  $("opOff").classList.toggle("act", !on); $("opOn").classList.toggle("act", on);
  $("opOff").querySelector(".tag")?.remove(); $("opOn").querySelector(".tag")?.remove();
  const tag = document.createElement("i"); tag.className = "tag"; tag.textContent = "ACTUAL"; (on ? $("opOn") : $("opOff")).prepend(tag);
  $("opOnTxt").textContent = on ? (red.estado == "conectado" ? "Conectada a “" + red.ssid + "” · publicando en www.panarepara.com/radio" : "Conectando a “" + red.ssid + "”…")
                                : red.ssid ? "Red guardada: “" + red.ssid + "”. Toca para conectarte o elegir otra." : "Conecta esta radio a una red WiFi para ver todo en www.panarepara.com/radio.";
  $("seguir").style.display = ""; $("seguir").textContent = "Continuar " + (on ? "con" : "sin") + " internet →";
  const e = $("estRed"), txt = { offline: "Modo sin internet" + (red.ssid ? " · red guardada: “" + red.ssid + "”" : ""),
    conectando: "⏳ Conectando a “" + red.ssid + "”…", clave: "✕ Clave incorrecta para “" + red.ssid + "”",
    noRed: "✕ No se encuentra la red “" + red.ssid + "”", conectado: "✓ Conectada a “" + red.ssid + "” · IP " + red.ip + " · canal " + red.canal + (red.web ? " · 🌐 enviando a la web" : " · conectando con la web…") };
  e.textContent = txt[red.estado] || red.estado;
  e.className = "estado" + (red.estado == "conectado" ? " ok" : red.estado == "clave" || red.estado == "noRed" ? " mal" : "");
  $("irChat").style.display = red.estado == "conectado" ? "" : "none";
  const rc = $("reconectar"); rc.style.display = !on && red.ssid ? "" : "none"; rc.textContent = "Reconectar a “" + red.ssid + "”";
}
async function escanear(nuevo) {
  clearTimeout(tScan);
  if (nuevo) $("redes").innerHTML = '<div class="nota">Buscando redes…</div>';
  try {
    const r = await (await fetch("/escanear" + (nuevo ? "?nuevo=1" : ""))).json();
    if (!r.listo) { if (!$("redes").innerHTML) $("redes").innerHTML = '<div class="nota">Buscando redes…</div>'; tScan = setTimeout(() => escanear(false), 1500); return; }
    const box = $("redes"); box.innerHTML = "";
    if (!r.redes.length) box.innerHTML = '<div class="nota">No se encontraron redes de 2.4 GHz.</div>';
    r.redes.sort((a, b) => b.r - a.r).forEach(n => {
      const b = document.createElement("button"); b.className = "red" + (n.s == redSel ? " sel" : "");
      const izq = document.createElement("span"); izq.textContent = (n.seg ? "🔒 " : "") + n.s;
      const der = document.createElement("span"); der.textContent = n.r > -60 ? "▂▄▆█" : n.r > -72 ? "▂▄▆" : n.r > -82 ? "▂▄" : "▂"; der.style.color = "#8a9bab";
      b.append(izq, der);
      b.onclick = () => { redSel = n.s; box.querySelectorAll(".red").forEach(x => x.classList.toggle("sel", x == b));
        $("claveBox").style.display = ""; $("clave").value = ""; $("clave").style.display = n.seg ? "" : "none"; if (n.seg) $("clave").focus(); };
      box.append(b);
    });
  } catch (e) { tScan = setTimeout(() => escanear(false), 2000); }
}
$("menuBtn").onclick = () => verMenu("main");
$("volver").onclick = () => verMenu("main");
$("seguir").onclick = () => verMenu(null);
$("irChat").onclick = () => verMenu(null);
$("opOff").onclick = async () => { await post("/modo", { m: "offline" }).catch(() => {}); aviso("Modo sin internet"); verMenu(null); };
$("opOn").onclick = () => verMenu("online");
$("buscar").onclick = () => escanear(true);
$("reconectar").onclick = () => { post("/modo", { m: "online" }); aviso("Conectando…"); };
$("conectar").onclick = () => {
  if (!redSel) return;
  post("/conectar", { ssid: redSel, clave: $("clave").value }).catch(() => {});
  $("claveBox").style.display = "none"; aviso("Conectando a “" + redSel + "”…", 5000);
};
verMenu("main");
actualizar();
cargarHist();
</script></body></html>
)PAGINA";
