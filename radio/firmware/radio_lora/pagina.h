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
#aviso{display:none;position:fixed;left:12px;right:12px;bottom:70px;background:#000c;padding:10px 12px;border-radius:10px;font-size:14px}
</style></head><body>
<header>
  <div class="top"><h1 id="tit">Radio</h1><button id="sosBtn">SOS</button></div>
  <div class="chips"><span id="cCerca" class="chip">⚡ Cerca: —</span><span id="cLora" class="chip">📡 LoRa: —</span><span id="cWeb" class="chip" style="display:none">🌐 Web</span><span id="cClima" class="chip">🌡 Clima: —</span></div>
</header>
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
function nombre(n) { return n == 0 ? "Web" : "Equipo " + n; }
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
  $("tit").textContent = "Radio · Equipo " + j.yo;
  const c = $("cCerca"); c.textContent = "⚡ Cerca: " + (j.cerca.on ? j.cerca.rssi + " dBm" : "fuera de alcance"); c.className = "chip" + (j.cerca.on ? " on" : "");
  const l = $("cLora");
  l.textContent = "📡 LoRa: " + (!j.lora.ok ? "sin módulo" : j.lora.on ? j.lora.rssi + " dBm · SNR " + j.lora.snr : "esperando…");
  l.className = "chip" + (j.lora.on ? " lo" : "");
  if ("web" in j) { const w = $("cWeb"); w.style.display = ""; w.className = "chip" + (j.web ? " on" : ""); w.textContent = "🌐 Web: " + (j.web ? "conectada" : "sin conexión"); }
  const cl = j.clima || {}, fc = x => !x ? "sin datos" : [x.t != null ? x.t + "°C · " + x.h + "%" : "", x.g != null ? "gas " + x.g + "%" : ""].filter(Boolean).join(" · ");
  $("cClima").textContent = "🌡 Aquí: " + fc(cl.yo) + " | Equipo " + j.otro + ": " + fc(cl.otro);
  $("cClima").className = "chip" + (cl.yo || cl.otro ? " on" : "");
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
actualizar();
cargarHist();
</script></body></html>
)PAGINA";
