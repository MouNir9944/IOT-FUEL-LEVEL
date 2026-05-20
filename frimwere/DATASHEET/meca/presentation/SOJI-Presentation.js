"use strict";
const pptxgen = require("pptxgenjs");

// ─── Design tokens ────────────────────────────────────────────────────────────
const C = {
  navy:   "1A2332",
  blue:   "0A5C8A",
  amber:  "F59E0B",
  light:  "F8FAFC",
  card:   "FFFFFF",
  text:   "1E293B",
  muted:  "64748B",
  white:  "FFFFFF",
  steel:  "334155",
  blueLt: "E0F2FE",
};

const makeShadow = () => ({ type: "outer", blur: 8, offset: 2, angle: 135, color: "000000", opacity: 0.10 });

// ─── Helpers ───────────────────────────────────────────────────────────────────
function amberBar(slide, x, y, h) {
  slide.addShape("rect", { x, y, w: 0.08, h, fill: { color: C.amber }, line: { color: C.amber } });
}

function cardRect(slide, x, y, w, h) {
  slide.addShape("rect", { x, y, w, h, fill: { color: C.card }, line: { color: "E2E8F0", width: 0.5 }, shadow: makeShadow() });
}

function iconCircle(slide, x, y, r, bgColor, letter) {
  slide.addShape("ellipse", { x, y, w: r, h: r, fill: { color: bgColor }, line: { color: bgColor } });
  slide.addText(letter, { x, y, w: r, h: r, fontSize: 14, bold: true, color: C.white, align: "center", valign: "middle", margin: 0 });
}

function slideHeader(slide, title, bg) {
  const isDark = bg === C.navy || bg === C.blue;
  const titleColor = isDark ? C.white : C.text;
  slide.background = { color: bg };
  // Top amber stripe
  slide.addShape("rect", { x: 0, y: 0, w: 10, h: 0.08, fill: { color: C.amber }, line: { color: C.amber } });
  slide.addText(title, {
    x: 0.5, y: 0.1, w: 9, h: 0.72,
    fontSize: 24, bold: true, fontFace: "Arial Black",
    color: titleColor, align: "left", valign: "middle", margin: 0,
  });
  // Thin separator
  slide.addShape("rect", { x: 0.5, y: 0.85, w: 9, h: 0.025, fill: { color: isDark ? "FFFFFF" : C.amber }, line: { color: isDark ? "FFFFFF" : C.amber } });
}

// ─── Create presentation ──────────────────────────────────────────────────────
const pres = new pptxgen();
pres.layout = "LAYOUT_16x9";
pres.author = "SOJI IoT";
pres.title  = "SOJI – Capteur de Niveau de Carburant";

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 1 – Page de Titre
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  s.background = { color: C.navy };

  // Amber left stripe
  s.addShape("rect", { x: 0, y: 0, w: 0.35, h: 5.625, fill: { color: C.amber }, line: { color: C.amber } });

  // Top-right decorative circle
  s.addShape("ellipse", { x: 7.5, y: -1.5, w: 4, h: 4, fill: { color: C.blue }, line: { color: C.blue } });
  s.addShape("ellipse", { x: 8.2, y: -0.8, w: 2.8, h: 2.8, fill: { color: "0D6FA0" }, line: { color: "0D6FA0" } });

  // Bottom decorative bar
  s.addShape("rect", { x: 0.35, y: 5.325, w: 9.65, h: 0.3, fill: { color: C.blue }, line: { color: C.blue } });

  // SOJI tag
  s.addText("CAPTEUR SOJI", {
    x: 0.65, y: 1.4, w: 8, h: 0.75,
    fontSize: 48, bold: true, fontFace: "Arial Black",
    color: C.white, align: "left", valign: "middle",
  });

  // Amber divider
  s.addShape("rect", { x: 0.65, y: 2.22, w: 3.2, h: 0.06, fill: { color: C.amber }, line: { color: C.amber } });

  s.addText("Système de Surveillance du Niveau de Carburant", {
    x: 0.65, y: 2.35, w: 8.5, h: 0.55,
    fontSize: 18, bold: false, fontFace: "Calibri",
    color: "B0C4DE", align: "left",
  });

  s.addText("Présentation Technique", {
    x: 0.65, y: 2.95, w: 6, h: 0.45,
    fontSize: 14, fontFace: "Calibri",
    color: C.amber, align: "left", bold: true,
  });

  // Tech badges
  const badges = ["ESP32", "RS-485", "4G LTE", "MQTT", "FreeRTOS"];
  badges.forEach((b, i) => {
    const bx = 0.65 + i * 1.72;
    s.addShape("rect", { x: bx, y: 3.7, w: 1.55, h: 0.4, fill: { color: "243552" }, line: { color: C.blue, width: 0.75 } });
    s.addText(b, { x: bx, y: 3.7, w: 1.55, h: 0.4, fontSize: 10, bold: true, color: C.amber, align: "center", valign: "middle", margin: 0 });
  });

  s.addText("© 2026 SOJI IoT — Tous droits réservés", {
    x: 0.65, y: 5.1, w: 8, h: 0.25,
    fontSize: 9, color: "6B7F9E", align: "left", fontFace: "Calibri",
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 2 – Vue d'Ensemble Architecture
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  slideHeader(s, "Vue d'Ensemble — Architecture Système", C.blue);

  // Flow nodes — 5 nodes evenly distributed across 9.4" with 0.3" margins
  // Each node 1.6" wide, gap 0.15" between
  const nw = 1.68, nh = 1.4, ny = 1.9, gap2 = 0.17;
  const nodes = [
    { label: "Capteur\nLIGO T54", sub: "RS-485",    bg: C.amber,  fg: C.navy  },
    { label: "MCU\nESP32",        sub: "FreeRTOS",  bg: "1565C0", fg: C.white },
    { label: "Modem\nSIM7600",    sub: "4G LTE",    bg: "0F4C75", fg: C.white },
    { label: "Cloud\nMQTT",       sub: "Broker",    bg: "155E75", fg: C.white },
    { label: "Tableau\nde Bord",  sub: "Dashboard", bg: "065F46", fg: C.white },
  ];
  const totalW = 5 * nw + 4 * gap2;
  const startX2 = (10 - totalW) / 2;

  // Draw protocol labels centered between each pair of nodes (above their midpoint)
  const protocols = ["RS-485\n9600 baud", "AT Cmds", "TCP/MQTT", "MQTT JSON"];
  for (let i = 0; i < 4; i++) {
    // Midpoint between node i and node i+1
    const n1x = startX2 + i * (nw + gap2);
    const n2x = startX2 + (i + 1) * (nw + gap2);
    const midX = (n1x + nw + n2x) / 2; // center between end of node i and start of node i+1
    // Place a wide label centered on the midpoint
    const lw = 1.0;
    s.addText(protocols[i], {
      x: midX - lw / 2, y: ny - 0.52, w: lw, h: 0.48,
      fontSize: 9, color: "CCE5F5", align: "center", fontFace: "Calibri",
    });
  }

  nodes.forEach((n, i) => {
    const nx = startX2 + i * (nw + gap2);
    s.addShape("rect", { x: nx, y: ny, w: nw, h: nh, fill: { color: n.bg }, line: { color: n.bg }, shadow: makeShadow() });
    s.addText(n.label, { x: nx, y: ny + 0.1, w: nw, h: 0.9, fontSize: 13, bold: true, color: n.fg, align: "center", valign: "middle", fontFace: "Calibri" });
    const footerBg = n.bg === C.amber ? "C07800" : "0A1929";
    const footerFg = n.bg === C.amber ? C.navy : "8BB8D0";
    s.addShape("rect", { x: nx, y: ny + nh - 0.35, w: nw, h: 0.35, fill: { color: footerBg }, line: { color: footerBg } });
    s.addText(n.sub, { x: nx, y: ny + nh - 0.35, w: nw, h: 0.35, fontSize: 10, color: footerFg, align: "center", valign: "middle", margin: 0, fontFace: "Calibri" });

    // Arrow to next node
    if (i < nodes.length - 1) {
      const arX = nx + nw;
      const arY = ny + nh / 2 - 0.025;
      s.addShape("rect", { x: arX, y: arY, w: gap2, h: 0.05, fill: { color: C.amber }, line: { color: C.amber } });
      // Arrowhead triangle approximation using a narrow rect
      s.addShape("rect", { x: arX + gap2 - 0.1, y: arY - 0.08, w: 0.1, h: 0.21, fill: { color: C.amber }, line: { color: C.amber } });
    }
  });

  // Bottom legend bullets
  s.addText([
    { text: "Acquisition RS-485 toutes les 30 s", options: { bullet: true, breakLine: true } },
    { text: "Calcul volume selon géométrie du réservoir", options: { bullet: true, breakLine: true } },
    { text: "Publication JSON via MQTT 4G LTE", options: { bullet: true, breakLine: true } },
    { text: "Sauvegarde SD si déconnexion — retry automatique", options: { bullet: true } },
  ], {
    x: 0.5, y: 3.65, w: 9, h: 1.6,
    fontSize: 12, color: C.white, fontFace: "Calibri", valign: "top",
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 3 – Composants Matériels
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  slideHeader(s, "Composants Matériels", C.light);

  const cards = [
    {
      icon: "E", iconBg: "1565C0", title: "ESP32", sub: "Microcontrôleur Principal",
      items: ["Dual-core Xtensa LX6 240 MHz", "WiFi + Bluetooth intégré", "ESP-IDF 5.x / FreeRTOS", "40 GPIO, ADC, SPI, UART", "Partitions OTA + NVS + SPIFFS"],
    },
    {
      icon: "S", iconBg: C.amber, title: "LIGO T54", sub: "Capteur RS-485",
      items: ["Niveau: 0–65535 (digital)", "Température: -40°C à +100°C", "Fréquence: 0–10000 Hz", "Protocol: 5 bytes cmd / 9 bytes rsp", "Timeout réponse: 2000 ms"],
    },
    {
      icon: "M", iconBg: "0F4C75", title: "SIM7600", sub: "Modem 4G LTE + GPS",
      items: ["4G LTE / 3G / 2G fallback", "GPS / GLONASS intégré", "MQTT, HTTP, TCP/IP", "Commandes AT standard", "Synchronisation NTP"],
    },
    {
      icon: "D", iconBg: "065F46", title: "microSD (SPI)", sub: "Stockage Local",
      items: ["Bus SPI à 4 MHz", "MOSI=GPIO15, MISO=GPIO2", "CLK=GPIO14, CS=GPIO13", "Sauvegarde trames MQTT", "Retry automatique au reconnect"],
    },
  ];

  const cw = 4.4, ch = 3.85, gap = 0.15;
  const startX = [0.3, 4.85, 0.3, 4.85];
  const startY = [0.95, 0.95, 0.95, 0.95];
  const rowY   = [0.95, 0.95, 0.95, 0.95];
  // 2x2 grid
  const cols = [0.3, 5.1];
  const rows = [0.95, 0.95];

  cards.forEach((c, i) => {
    const cx = cols[i % 2];
    const cy = i < 2 ? 0.95 : 0.95; // single row — we'll stack in y
    // Actually 2 rows
    const cyFinal = i < 2 ? 0.95 : 0.95;
    const cyReal  = i < 2 ? 0.95 : 2.82; // row 0 and row 1

    const x = cols[i % 2];
    const y = i < 2 ? 0.95 : 2.82;
    const w = 4.5;
    const h = 1.7;

    cardRect(s, x, y, w, h);
    amberBar(s, x, y, h);

    iconCircle(s, x + 0.18, y + 0.15, 0.52, c.iconBg, c.icon);

    s.addText(c.title, { x: x + 0.82, y: y + 0.1, w: 3.4, h: 0.35, fontSize: 15, bold: true, color: C.text, fontFace: "Calibri", valign: "middle" });
    s.addText(c.sub,   { x: x + 0.82, y: y + 0.42, w: 3.4, h: 0.22, fontSize: 9, color: C.muted, fontFace: "Calibri" });

    // Items as 2-column mini list
    const half = Math.ceil(c.items.length / 2);
    c.items.slice(0, half).forEach((item, j) => {
      s.addText("▸ " + item, { x: x + 0.18, y: y + 0.75 + j * 0.24, w: 2.0, h: 0.24, fontSize: 9, color: C.text, fontFace: "Calibri" });
    });
    c.items.slice(half).forEach((item, j) => {
      s.addText("▸ " + item, { x: x + 2.28, y: y + 0.75 + j * 0.24, w: 2.1, h: 0.24, fontSize: 9, color: C.text, fontFace: "Calibri" });
    });
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 4 – Capteur LIGO T54
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  slideHeader(s, "Capteur LIGO T54 — Protocole RS-485", C.light);

  // Left column: protocol details
  // Command frame card
  cardRect(s, 0.3, 1.0, 4.5, 1.55);
  amberBar(s, 0.3, 1.0, 1.55);
  s.addText("Trame Commande (5 octets)", { x: 0.5, y: 1.05, w: 4.1, h: 0.3, fontSize: 12, bold: true, color: C.blue, fontFace: "Calibri" });
  const cmdBytes = ["0x31", "ADDR", "0x06", "0x00", "CRC8"];
  const cmdLabels = ["START", "Adresse", "Cmd", "Data", "CRC"];
  cmdBytes.forEach((b, i) => {
    const bx = 0.5 + i * 0.82;
    s.addShape("rect", { x: bx, y: 1.42, w: 0.78, h: 0.42, fill: { color: i === 4 ? "FEF3C7" : C.blueLt }, line: { color: C.blue, width: 0.5 } });
    s.addText(b, { x: bx, y: 1.42, w: 0.78, h: 0.24, fontSize: 9, bold: true, color: C.blue, align: "center", valign: "middle", margin: 0, fontFace: "Calibri" });
    s.addText(cmdLabels[i], { x: bx, y: 1.66, w: 0.78, h: 0.18, fontSize: 7, color: C.muted, align: "center", margin: 0, fontFace: "Calibri" });
  });
  s.addShape("rect", { x: 0.5, y: 1.88, w: 4.08, h: 0.1, fill: { color: "FEF3C7" }, line: { color: C.amber } });
  s.addText("Polynomial CRC-8 (0x31 + ADDR + 0x06 + 0x00)", { x: 0.5, y: 1.9, w: 4.1, h: 0.2, fontSize: 8, color: C.muted, fontFace: "Calibri" });

  // Response frame card
  cardRect(s, 0.3, 2.7, 4.5, 1.8);
  amberBar(s, 0.3, 2.7, 1.8);
  s.addText("Trame Réponse (9 octets)", { x: 0.5, y: 2.75, w: 4.1, h: 0.3, fontSize: 12, bold: true, color: C.blue, fontFace: "Calibri" });
  const rspBytes  = ["0x3E", "ADDR", "0x06", "TEMP", "LVL_L", "LVL_H", "FRQ_L", "FRQ_H", "CRC8"];
  const rspGroups = ["Header", "", "", "Temp", "Niveau", "", "Fréq.", "", "CRC"];
  rspBytes.forEach((b, i) => {
    const bx = 0.38 + i * 0.475;
    const colMap = { 3: "FEE2E2", 4: C.blueLt, 5: C.blueLt, 6: "DCFCE7", 7: "DCFCE7", 8: "FEF3C7" };
    const bg = colMap[i] || C.blueLt;
    s.addShape("rect", { x: bx, y: 3.12, w: 0.45, h: 0.42, fill: { color: bg }, line: { color: "CBD5E1", width: 0.5 } });
    s.addText(b,          { x: bx, y: 3.12, w: 0.45, h: 0.24, fontSize: 7.5, bold: true, color: C.navy, align: "center", valign: "middle", margin: 0, fontFace: "Calibri" });
    s.addText(rspGroups[i],{ x: bx, y: 3.36, w: 0.45, h: 0.18, fontSize: 6.5, color: C.muted, align: "center", margin: 0, fontFace: "Calibri" });
  });
  s.addText("Timeout réponse: 2000 ms  |  Half-duplex (DE/RE GPIO32 control)", {
    x: 0.5, y: 3.62, w: 4.1, h: 0.2, fontSize: 8.5, color: C.muted, fontFace: "Calibri",
  });

  // Paramètres
  cardRect(s, 0.3, 4.55, 4.5, 0.85);
  amberBar(s, 0.3, 4.55, 0.85);
  s.addText("Paramètres mesurés", { x: 0.5, y: 4.6, w: 4.1, h: 0.25, fontSize: 11, bold: true, color: C.text, fontFace: "Calibri" });
  s.addText("Niveau: 0–65535 dig.  |  Temp: -40°C à +100°C  |  Fréquence: 0–10000 Hz", {
    x: 0.5, y: 4.86, w: 4.1, h: 0.22, fontSize: 9, color: C.text, fontFace: "Calibri",
  });

  // Right column: GPIO & config
  cardRect(s, 5.0, 1.0, 4.7, 1.5);
  amberBar(s, 5.0, 1.0, 1.5);
  s.addText("Interfaçage GPIO — ESP32", { x: 5.2, y: 1.05, w: 4.2, h: 0.3, fontSize: 12, bold: true, color: C.blue, fontFace: "Calibri" });
  const gpios = [
    ["RS-485 TX", "GPIO 21"],
    ["RS-485 RX", "GPIO 22"],
    ["DE / RE",   "GPIO 32"],
    ["Baud Rate", "9600"],
    ["Format",    "8N1, Half-duplex"],
  ];
  gpios.forEach(([k, v], i) => {
    const gy = 1.42 + i * 0.2;
    s.addText(k + ":", { x: 5.2, y: gy, w: 1.6, h: 0.2, fontSize: 9, bold: true, color: C.text, fontFace: "Calibri" });
    s.addText(v, { x: 6.85, y: gy, w: 2.6, h: 0.2, fontSize: 9, color: C.blue, fontFace: "Calibri" });
  });

  // Timing diagram card
  cardRect(s, 5.0, 2.65, 4.7, 1.75);
  amberBar(s, 5.0, 2.65, 1.75);
  s.addText("Chronogramme Communication", { x: 5.2, y: 2.7, w: 4.2, h: 0.3, fontSize: 12, bold: true, color: C.blue, fontFace: "Calibri" });

  // Simple timing diagram shapes
  const tdY = 3.1;
  s.addText("MCU TX:", { x: 5.1, y: tdY, w: 0.9, h: 0.25, fontSize: 8, color: C.muted, fontFace: "Calibri", valign: "middle" });
  s.addShape("rect", { x: 6.05, y: tdY, w: 1.1, h: 0.22, fill: { color: C.blue }, line: { color: C.blue } });
  s.addText("5 bytes", { x: 6.05, y: tdY, w: 1.1, h: 0.22, fontSize: 7, color: C.white, align: "center", valign: "middle", margin: 0 });
  s.addShape("rect", { x: 7.15, y: tdY + 0.09, w: 2.3, h: 0.04, fill: { color: "CBD5E1" }, line: { color: "CBD5E1" } });

  s.addText("DE/RE:", { x: 5.1, y: tdY + 0.35, w: 0.9, h: 0.25, fontSize: 8, color: C.muted, fontFace: "Calibri", valign: "middle" });
  s.addShape("rect", { x: 6.05, y: tdY + 0.35, w: 1.1, h: 0.22, fill: { color: C.amber }, line: { color: C.amber } });
  s.addText("TX MODE", { x: 6.05, y: tdY + 0.35, w: 1.1, h: 0.22, fontSize: 7, color: C.navy, align: "center", valign: "middle", margin: 0 });
  s.addShape("rect", { x: 7.15, y: tdY + 0.35, w: 2.3, h: 0.22, fill: { color: "DCFCE7" }, line: { color: "86EFAC" } });
  s.addText("RX MODE", { x: 7.15, y: tdY + 0.35, w: 2.3, h: 0.22, fontSize: 7, color: "065F46", align: "center", valign: "middle", margin: 0 });

  s.addText("Capteur RX:", { x: 5.1, y: tdY + 0.7, w: 0.9, h: 0.25, fontSize: 8, color: C.muted, fontFace: "Calibri", valign: "middle" });
  s.addShape("rect", { x: 6.05, y: tdY + 0.7, w: 1.1, h: 0.04, fill: { color: "CBD5E1" }, line: { color: "CBD5E1" } });
  s.addShape("rect", { x: 7.3, y: tdY + 0.7, w: 1.5, h: 0.22, fill: { color: "F3E8FF" }, line: { color: "C084FC" } });
  s.addText("9 bytes réponse", { x: 7.3, y: tdY + 0.7, w: 1.5, h: 0.22, fontSize: 7, color: "7C3AED", align: "center", valign: "middle", margin: 0 });

  s.addText("← 5.2 ms →                 ← Timeout 2000 ms →", {
    x: 5.95, y: tdY + 1.0, w: 3.6, h: 0.2, fontSize: 7.5, color: C.muted, fontFace: "Calibri",
  });

  // Battery ADC
  cardRect(s, 5.0, 4.55, 4.7, 0.85);
  amberBar(s, 5.0, 4.55, 0.85);
  s.addText("ADC Batterie", { x: 5.2, y: 4.6, w: 4.2, h: 0.25, fontSize: 11, bold: true, color: C.text, fontFace: "Calibri" });
  s.addText("GPIO39 — Canal 7  |  Plage: 2500–4300 mV  |  Moyenne sur 8 échantillons", {
    x: 5.2, y: 4.86, w: 4.2, h: 0.22, fontSize: 9, color: C.text, fontFace: "Calibri",
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 5 – Modem SIM7600
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  slideHeader(s, "Modem SIM7600 — 4G LTE & GPS", C.light);

  // 3 feature cards top row
  const topCards = [
    { icon: "4G", bg: "1565C0", title: "Connectivité LTE", lines: ["4G LTE Cat-4 (principal)", "3G WCDMA (fallback)", "2G EDGE (fallback)", "Bande mondiale"] },
    { icon: "G",  bg: "065F46", title: "GPS / GNSS",       lines: ["GPS + GLONASS", "Latitude + Longitude", "Altitude + Vitesse", "Précision ≤ 2.5 m CEP"] },
    { icon: "T",  bg: "7C3AED", title: "Synchronisation",  lines: ["NTP via modem", "UTC timestamp", "Calibration horloge", "RTC interne ESP32"] },
  ];
  topCards.forEach((c, i) => {
    const cx = 0.3 + i * 3.22;
    cardRect(s, cx, 0.95, 3.05, 2.0);
    amberBar(s, cx, 0.95, 2.0);
    iconCircle(s, cx + 0.18, 0.95 + 0.18, 0.5, c.bg, c.icon);
    s.addText(c.title, { x: cx + 0.82, y: 0.95 + 0.12, w: 2.0, h: 0.42, fontSize: 12, bold: true, color: C.text, fontFace: "Calibri", valign: "middle" });
    c.lines.forEach((l, j) => {
      s.addText("▸ " + l, { x: cx + 0.18, y: 1.62 + j * 0.27, w: 2.7, h: 0.27, fontSize: 10, color: C.text, fontFace: "Calibri" });
    });
  });

  // AT Commands section
  cardRect(s, 0.3, 3.1, 9.4, 1.25);
  amberBar(s, 0.3, 3.1, 1.25);
  s.addText("Commandes AT Principales", { x: 0.5, y: 3.15, w: 4, h: 0.3, fontSize: 12, bold: true, color: C.blue, fontFace: "Calibri" });

  const atCmds = [
    ["AT+CMQTTSTART",    "Initialiser client MQTT"],
    ["AT+CMQTTACCQ",     "Acquérir un client MQTT"],
    ["AT+CMQTTCONNECT",  "Connecter au broker"],
    ["AT+CMQTTPAYLOAD",  "Préparer le payload JSON"],
    ["AT+CMQTTPUB",      "Publier un message"],
    ["AT+CGPSINFO",      "Obtenir coordonnées GPS"],
    ["AT+CSNTPSTART",    "Démarrer NTP sync"],
    ["AT+CCLK?",         "Lire l'heure système"],
  ];
  const half = 4;
  atCmds.slice(0, half).forEach(([cmd, desc], i) => {
    s.addShape("rect", { x: 0.5, y: 3.52 + i * 0.2, w: 1.8, h: 0.2, fill: { color: "EFF6FF" }, line: { color: "BFDBFE", width: 0.5 } });
    s.addText(cmd,  { x: 0.5,  y: 3.52 + i * 0.2, w: 1.8, h: 0.2, fontSize: 8.5, bold: true, color: C.blue, fontFace: "Calibri", valign: "middle", margin: [0, 2, 0, 2] });
    s.addText(desc, { x: 2.35, y: 3.52 + i * 0.2, w: 2.2, h: 0.2, fontSize: 8.5, color: C.text, fontFace: "Calibri", valign: "middle" });
  });
  atCmds.slice(half).forEach(([cmd, desc], i) => {
    s.addShape("rect", { x: 5.0, y: 3.52 + i * 0.2, w: 1.8, h: 0.2, fill: { color: "EFF6FF" }, line: { color: "BFDBFE", width: 0.5 } });
    s.addText(cmd,  { x: 5.0,  y: 3.52 + i * 0.2, w: 1.8, h: 0.2, fontSize: 8.5, bold: true, color: C.blue, fontFace: "Calibri", valign: "middle", margin: [0, 2, 0, 2] });
    s.addText(desc, { x: 6.85, y: 3.52 + i * 0.2, w: 2.8, h: 0.2, fontSize: 8.5, color: C.text, fontFace: "Calibri", valign: "middle" });
  });

  // Capabilities footer
  cardRect(s, 0.3, 4.5, 9.4, 0.9);
  amberBar(s, 0.3, 4.5, 0.9);
  s.addText("Protocoles supportés", { x: 0.5, y: 4.55, w: 2, h: 0.28, fontSize: 11, bold: true, color: C.text, fontFace: "Calibri" });
  const protos = ["MQTT v3.1.1", "HTTP/HTTPS", "TCP/UDP", "FTP", "SMS", "SSL/TLS"];
  protos.forEach((p, i) => {
    const px = 2.65 + i * 1.15;
    s.addShape("rect", { x: px, y: 4.58, w: 1.05, h: 0.3, fill: { color: C.blueLt }, line: { color: C.blue, width: 0.5 } });
    s.addText(p, { x: px, y: 4.58, w: 1.05, h: 0.3, fontSize: 9, bold: true, color: C.blue, align: "center", valign: "middle", margin: 0, fontFace: "Calibri" });
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 6 – Flux de Données
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  slideHeader(s, "Flux de Données — Pipeline de Traitement", C.blue);

  const steps = [
    { num: "1", title: "Acquisition",   desc: "RS-485 cmd → LIGO T54\nRéponse 9 octets\nTimeout 2000 ms", color: C.amber,  fc: C.navy },
    { num: "2", title: "Validation",    desc: "Vérif. CRC-8\nPlausibilité valeurs\nExtraction champs", color: "1565C0", fc: C.white },
    { num: "3", title: "Calcul Volume", desc: "8 géométries réservoir\nNiveau → Volume (L)\nCorriger f", color: "0F4C75", fc: C.white },
    { num: "4", title: "JSON Payload",  desc: "Assemblage télémétrie\nTimestamp NTP/UTC\nGPS + batterie", color: "155E75", fc: C.white },
    { num: "5", title: "MQTT / SD",     desc: "Publication MQTT 4G\nSi échec → SD card\nRetry auto dès reconnect", color: "065F46", fc: C.white },
  ];

  const sw = 1.68, sh = 3.0, sy = 1.3, gap2 = 0.24;
  steps.forEach((st, i) => {
    const sx = 0.25 + i * (sw + gap2);
    s.addShape("rect", { x: sx, y: sy, w: sw, h: sh, fill: { color: st.color }, line: { color: st.color }, shadow: makeShadow() });

    // Number circle
    s.addShape("ellipse", { x: sx + sw / 2 - 0.28, y: sy + 0.18, w: 0.56, h: 0.56, fill: { color: "FFFFFF", transparency: 80 }, line: { color: "FFFFFF", transparency: 70 } });
    s.addText(st.num, { x: sx + sw / 2 - 0.28, y: sy + 0.18, w: 0.56, h: 0.56, fontSize: 16, bold: true, color: C.white, align: "center", valign: "middle", margin: 0 });

    s.addText(st.title, { x: sx, y: sy + 0.85, w: sw, h: 0.38, fontSize: 11, bold: true, color: st.fc, align: "center", fontFace: "Calibri", valign: "middle" });

    // Desc lines
    st.desc.split("\n").forEach((line, j) => {
      s.addText(line, { x: sx + 0.08, y: sy + 1.35 + j * 0.32, w: sw - 0.16, h: 0.3, fontSize: 9.5, color: i === 0 ? C.navy : "D0E8F5", align: "center", fontFace: "Calibri" });
    });

    // Arrow to next
    if (i < steps.length - 1) {
      const ax = sx + sw + 0.04;
      const arY = sy + sh / 2;
      s.addShape("rect", { x: ax, y: arY - 0.025, w: gap2 - 0.08, h: 0.05, fill: { color: C.amber }, line: { color: C.amber } });
    }
  });

  // Bottom note
  s.addShape("rect", { x: 0.25, y: 4.5, w: 9.5, h: 0.75, fill: { color: "1E3A5F" }, line: { color: "2E5E8E" } });
  s.addText([
    { text: "Fiabilité: ", options: { bold: true, color: C.amber } },
    { text: "En cas de perte 4G, chaque trame est sauvegardée sur carte SD. Au retour de la connectivité, le système rejoue automatiquement toutes les trames en attente dans l'ordre chronologique.", options: { color: C.white } },
  ], { x: 0.45, y: 4.55, w: 9.1, h: 0.65, fontSize: 10, fontFace: "Calibri", valign: "middle" });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 7 – Calcul de Volume
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  slideHeader(s, "Calcul de Volume — 8 Types de Réservoirs", C.light);

  const tanks = [
    { num: "1", name: "Rectangulaire",      formula: "V = L × W × H × f",               icon: "R" },
    { num: "2", name: "Cylindre Vertical",  formula: "V = π × r² × H × f",              icon: "CV" },
    { num: "3", name: "Cylindre Horizontal",formula: "V = Segment circ. × L",            icon: "CH" },
    { num: "4", name: "Cône Vertical",      formula: "V = ⅓ × π × r² × H × f³",        icon: "CO" },
    { num: "5", name: "Ellipse Verticale",  formula: "V = π × a × b × H × f",           icon: "EL" },
    { num: "6", name: "Sphère",             formula: "V = π × h²(3r-h) / 3",            icon: "SP" },
    { num: "7", name: "Capsule",            formula: "V = Calotte + Cylindre",           icon: "CA" },
    { num: "8", name: "Multi-sections",     formula: "V = Σ sections (max 8)",           icon: "MS" },
  ];

  const cols2 = [0.3, 2.6, 4.9, 7.2];
  const rows2 = [0.95, 2.38, 3.81];
  // 2 rows × 4 cols = 8 tanks
  const tw = 2.1, th = 1.2;

  tanks.forEach((t, i) => {
    const col = i % 4;
    const row = Math.floor(i / 4);
    const tx = cols2[col];
    const ty = row === 0 ? 0.95 : 2.38;

    const colors = ["1565C0","0F4C75","065F46","7C3AED","0F766E","B45309","9F1239","334155"];
    const bg = colors[i];

    cardRect(s, tx, ty, tw, th);
    amberBar(s, tx, ty, th);

    // Icon circle
    s.addShape("ellipse", { x: tx + 0.14, y: ty + 0.14, w: 0.44, h: 0.44, fill: { color: bg }, line: { color: bg } });
    s.addText(t.icon, { x: tx + 0.14, y: ty + 0.14, w: 0.44, h: 0.44, fontSize: 8, bold: true, color: C.white, align: "center", valign: "middle", margin: 0 });

    // Number badge
    s.addShape("rect", { x: tx + tw - 0.3, y: ty, w: 0.3, h: 0.3, fill: { color: C.amber }, line: { color: C.amber } });
    s.addText(t.num, { x: tx + tw - 0.3, y: ty, w: 0.3, h: 0.3, fontSize: 9, bold: true, color: C.navy, align: "center", valign: "middle", margin: 0 });

    s.addText(t.name, { x: tx + 0.65, y: ty + 0.12, w: 1.3, h: 0.38, fontSize: 10, bold: true, color: C.text, fontFace: "Calibri", valign: "middle" });
    s.addShape("rect", { x: tx + 0.14, y: ty + 0.64, w: tw - 0.28, h: 0.42, fill: { color: "F1F5F9" }, line: { color: "E2E8F0" } });
    s.addText(t.formula, { x: tx + 0.14, y: ty + 0.64, w: tw - 0.28, h: 0.42, fontSize: 9, color: C.blue, align: "center", valign: "middle", fontFace: "Calibri", bold: true });
  });

  // Note card
  cardRect(s, 0.3, 3.66, 9.4, 1.72);
  amberBar(s, 0.3, 3.66, 1.72);
  s.addText("Notes d'implémentation", { x: 0.5, y: 3.72, w: 4, h: 0.3, fontSize: 12, bold: true, color: C.blue, fontFace: "Calibri" });
  const notes = [
    "f = facteur de remplissage (niveau_raw / 65535), valeur normalisée 0.0 → 1.0",
    "Cylindre horizontal: intégration numérique de segment circulaire, précision ≤ 0.1%",
    "Mode multi-sections: jusqu'à 8 sections de hauteurs et géométries différentes — somme des volumes partiels",
    "Toutes les formules utilisent des doubles IEEE 754 64-bit pour la précision maximale",
  ];
  notes.forEach((n, i) => {
    s.addText("▸ " + n, { x: 0.5, y: 4.08 + i * 0.3, w: 9.0, h: 0.3, fontSize: 10, color: C.text, fontFace: "Calibri" });
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 8 – Communication MQTT
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  slideHeader(s, "Communication MQTT — Protocole & Payload", C.light);

  // Left: Topic structure + QoS
  cardRect(s, 0.3, 0.98, 4.5, 1.68);
  amberBar(s, 0.3, 0.98, 1.68);
  s.addText("Structure des Topics", { x: 0.5, y: 1.03, w: 4.1, h: 0.3, fontSize: 12, bold: true, color: C.blue, fontFace: "Calibri" });

  const topics = [
    { label: "Publication:",  topic: "device/{ORG}/{MAC}/telemetry", color: "DCFCE7", tc: "065F46" },
    { label: "Abonnement:",   topic: "device/{ORG}/{MAC}/config",    color: C.blueLt,  tc: "1565C0" },
    { label: "LWT:",          topic: "device/{ORG}/{MAC}/status",    color: "FEF3C7",  tc: "B45309" },
  ];
  topics.forEach((t, i) => {
    const ty = 1.4 + i * 0.4;
    s.addText(t.label, { x: 0.5, y: ty, w: 0.95, h: 0.32, fontSize: 9, bold: true, color: C.text, fontFace: "Calibri", valign: "middle" });
    s.addShape("rect", { x: 1.5, y: ty + 0.02, w: 3.08, h: 0.28, fill: { color: t.color }, line: { color: t.tc, width: 0.5 } });
    s.addText(t.topic, { x: 1.5, y: ty + 0.02, w: 3.08, h: 0.28, fontSize: 8.5, color: t.tc, bold: true, fontFace: "Calibri", valign: "middle", margin: [0, 3, 0, 3] });
  });

  // QoS + LWT details
  cardRect(s, 0.3, 2.78, 4.5, 1.1);
  amberBar(s, 0.3, 2.78, 1.1);
  s.addText("Paramètres MQTT", { x: 0.5, y: 2.83, w: 4.1, h: 0.28, fontSize: 12, bold: true, color: C.blue, fontFace: "Calibri" });
  const mqttParams = [
    ["QoS:", "1 (at least once)"],
    ["Keep-alive:", "60 secondes"],
    ["LWT:", "payload: {\"status\":\"offline\"}"],
    ["Client ID:", "MAC address du dispositif"],
  ];
  mqttParams.forEach(([k, v], i) => {
    s.addText(k, { x: 0.5, y: 3.18 + i * 0.19, w: 1.0, h: 0.19, fontSize: 9, bold: true, color: C.text, fontFace: "Calibri" });
    s.addText(v, { x: 1.55, y: 3.18 + i * 0.19, w: 3.0, h: 0.19, fontSize: 9, color: C.muted, fontFace: "Calibri" });
  });

  // Config distante
  cardRect(s, 0.3, 4.0, 4.5, 1.35);
  amberBar(s, 0.3, 4.0, 1.35);
  s.addText("Configuration Distante", { x: 0.5, y: 4.05, w: 4.1, h: 0.28, fontSize: 12, bold: true, color: C.blue, fontFace: "Calibri" });
  const configs = ["Intervalle d'acquisition", "Paramètres réservoir", "Seuils d'alerte", "Activation GPS / SD", "Redémarrage OTA"];
  configs.forEach((c, i) => {
    s.addText("▸ " + c, { x: 0.5, y: 4.38 + i * 0.195, w: 4.1, h: 0.2, fontSize: 9.5, color: C.text, fontFace: "Calibri" });
  });

  // Right: JSON payload
  cardRect(s, 5.0, 0.98, 4.7, 4.37);
  amberBar(s, 5.0, 0.98, 4.37);
  s.addText("Payload JSON — Télémétrie", { x: 5.2, y: 1.03, w: 4.2, h: 0.3, fontSize: 12, bold: true, color: C.blue, fontFace: "Calibri" });

  s.addShape("rect", { x: 5.18, y: 1.4, w: 4.35, h: 3.75, fill: { color: "0F172A" }, line: { color: "1E293B" } });

  const jsonLines = [
    { t: "{",                                           c: C.white },
    { t: '  "device_id":   "AA:BB:CC:DD:EE:FF",',     c: "93C5FD" },
    { t: '  "firmware":    "v1.0.0",',                 c: "93C5FD" },
    { t: '  "timestamp":   "2026-03-31T14:22:00Z",',  c: "86EFAC" },
    { t: '  "level_raw":   42500,',                    c: "FCA5A5" },
    { t: '  "level_pct":   64.8,',                     c: "FCA5A5" },
    { t: '  "volume_l":    1280.5,',                   c: "FCA5A5" },
    { t: '  "temperature": 22.4,',                     c: "FDBA74" },
    { t: '  "frequency":   4800.0,',                   c: "FDBA74" },
    { t: '  "battery_mv":  3850,',                     c: "C4B5FD" },
    { t: '  "rssi":        -72,',                      c: "C4B5FD" },
    { t: '  "gps": {',                                 c: C.white  },
    { t: '    "lat": 36.7525,',                        c: "86EFAC" },
    { t: '    "lon": 3.0422',                          c: "86EFAC" },
    { t: '  }',                                        c: C.white  },
    { t: "}",                                          c: C.white  },
  ];
  jsonLines.forEach((jl, i) => {
    s.addText(jl.t, { x: 5.28, y: 1.46 + i * 0.218, w: 4.15, h: 0.22, fontSize: 8.5, color: jl.c, fontFace: "Consolas", valign: "middle" });
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 9 – Stockage & Fiabilité
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  slideHeader(s, "Stockage & Fiabilité du Système", C.light);

  // 4 feature cards
  const fCards = [
    { icon: "SD", bg: "065F46", title: "Sauvegarde SD Card",
      items: ["Chaque trame MQTT échouée → microSD", "Format: fichier JSON line-delimited", "Persistance à travers les redémarrages"] },
    { icon: "RT", bg: "1565C0", title: "Retry Automatique",
      items: ["Détection reconnexion 4G", "Replay des trames en attente", "Ordre chronologique garanti"] },
    { icon: "NV", bg: "7C3AED", title: "NVS Flash",
      items: ["24 KB partition dédiée", "Configuration persistante", "Préservée lors des OTA updates"] },
    { icon: "OT", bg: C.amber,  title: "OTA Firmware Update",
      items: ["Deux slots OTA (1.25 MB chacun)", "Mise à jour sans interruption", "Rollback automatique si erreur"] },
  ];

  const fcw = 4.5, fch = 2.25;
  fCards.forEach((fc, i) => {
    const fx = i % 2 === 0 ? 0.3 : 5.1;
    const fy = i < 2 ? 0.98 : 3.35;
    cardRect(s, fx, fy, fcw, fch);
    amberBar(s, fx, fy, fch);
    iconCircle(s, fx + 0.18, fy + 0.18, 0.52, fc.bg, fc.icon);
    s.addText(fc.title, { x: fx + 0.82, y: fy + 0.14, w: 3.4, h: 0.38, fontSize: 13, bold: true, color: C.text, fontFace: "Calibri", valign: "middle" });
    fc.items.forEach((item, j) => {
      s.addText("▸ " + item, { x: fx + 0.2, y: fy + 0.72 + j * 0.46, w: fcw - 0.38, h: 0.45, fontSize: 10, color: C.text, fontFace: "Calibri" });
    });
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 10 – Partitionnement Flash
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  slideHeader(s, "Partitionnement Flash ESP32", C.light);

  // Partition table visual
  const partitions = [
    { name: "nvs",     label: "NVS",          size: "24 KB",   desc: "Config persistante",      color: "7C3AED", w: 0.6 },
    { name: "phy",     label: "PHY Init",     size: "4 KB",    desc: "Calibration RF",          color: "9F1239", w: 0.3 },
    { name: "factory", label: "Factory App",  size: "1 MB",    desc: "Firmware initial",        color: "065F46", w: 1.4 },
    { name: "ota_0",   label: "OTA Slot 0",   size: "1.25 MB", desc: "Image firmware A",        color: "1565C0", w: 1.8 },
    { name: "ota_1",   label: "OTA Slot 1",   size: "1.25 MB", desc: "Image firmware B",        color: "0F4C75", w: 1.8 },
    { name: "spiffs",  label: "SPIFFS",       size: "1.375 MB",desc: "Fichiers / logs",         color: "B45309", w: 2.0 },
    { name: "otadata", label: "OTA Data",     size: "8 KB",    desc: "Sélection slot actif",   color: C.muted, w: 0.6 },
  ];

  const barY = 1.05, barH = 1.2;
  const totalW = 9.4;
  const startX2 = 0.3;
  let cx = startX2;
  const scale = totalW / partitions.reduce((a, p) => a + p.w, 0);

  partitions.forEach((p) => {
    const pw = p.w * scale;
    s.addShape("rect", { x: cx, y: barY, w: pw, h: barH, fill: { color: p.color }, line: { color: "FFFFFF", width: 1 } });
    if (pw > 0.8) {
      s.addText(p.label, { x: cx, y: barY + 0.25, w: pw, h: 0.38, fontSize: pw > 1.5 ? 11 : 9, bold: true, color: C.white, align: "center", valign: "middle", margin: 0, fontFace: "Calibri" });
      s.addText(p.size,  { x: cx, y: barY + 0.65, w: pw, h: 0.3, fontSize: pw > 1.5 ? 10 : 8, color: "DDDDDD", align: "center", margin: 0, fontFace: "Calibri" });
    }
    cx += pw;
  });

  // Address labels below bar
  s.addText("0x0000", { x: startX2, y: barY + barH + 0.08, w: 0.6, h: 0.2, fontSize: 7.5, color: C.muted, fontFace: "Calibri" });
  s.addText("0x10000", { x: startX2 + 0.6 * scale * 3.5, y: barY + barH + 0.08, w: 0.8, h: 0.2, fontSize: 7.5, color: C.muted, fontFace: "Calibri" });
  s.addText("Flash totale: 4 MB (ESP32 standard)", { x: startX2, y: barY + barH + 0.08, w: 9.4, h: 0.2, fontSize: 8.5, color: C.muted, fontFace: "Calibri", align: "right" });

  // Detail cards
  const detailCards = [
    { title: "NVS — 24 KB",     color: "7C3AED", items: ["Stockage clé-valeur", "Config WiFi, MQTT, réservoir", "API esp_nvs_flash_init()"] },
    { title: "OTA Slots — 1.25 MB × 2", color: "1565C0", items: ["Mise à jour à distance", "Swap après validation", "esp_ota_ops.h API"] },
    { title: "SPIFFS — 1.375 MB", color: "B45309", items: ["Système de fichiers embarqué", "Logs, config JSON, SD buffer", "esp_spiffs_conf API"] },
    { title: "OTA Process",     color: "065F46", items: ["1. Download MQTT/HTTP", "2. Écriture slot inactif", "3. Validation + reboot"] },
  ];
  const dw = 2.2, dh = 1.68;
  const dx = [0.3, 2.62, 4.94, 7.26];
  detailCards.forEach((dc, i) => {
    cardRect(s, dx[i], 2.65, dw, dh);
    s.addShape("rect", { x: dx[i], y: 2.65, w: dw, h: 0.3, fill: { color: dc.color }, line: { color: dc.color } });
    s.addText(dc.title, { x: dx[i], y: 2.65, w: dw, h: 0.3, fontSize: 9, bold: true, color: C.white, align: "center", valign: "middle", margin: 0, fontFace: "Calibri" });
    dc.items.forEach((it, j) => {
      s.addText("▸ " + it, { x: dx[i] + 0.1, y: 3.02 + j * 0.38, w: dw - 0.2, h: 0.36, fontSize: 9, color: C.text, fontFace: "Calibri" });
    });
  });

  // OTA flow arrow diagram
  cardRect(s, 0.3, 4.45, 9.4, 0.9);
  amberBar(s, 0.3, 4.45, 0.9);
  s.addText("Flux OTA", { x: 0.5, y: 4.5, w: 1.2, h: 0.28, fontSize: 11, bold: true, color: C.blue, fontFace: "Calibri" });
  const otaSteps = ["Réception MQTT", "Download chunk", "Écriture slot OTA", "Validation SHA256", "Set boot partition", "Reboot → Nouveau FW"];
  const otaW = 1.28, otaGap = 0.12;
  const otaTotalW = otaSteps.length * otaW + (otaSteps.length - 1) * otaGap;
  const otaStartX = 1.75;
  otaSteps.forEach((step, i) => {
    const ox = otaStartX + i * (otaW + otaGap);
    s.addShape("rect", { x: ox, y: 4.55, w: otaW, h: 0.4, fill: { color: "EFF6FF" }, line: { color: C.blue, width: 0.5 } });
    s.addText(step, { x: ox, y: 4.55, w: otaW, h: 0.4, fontSize: 8, color: C.blue, align: "center", valign: "middle", margin: 0, fontFace: "Calibri" });
    if (i < otaSteps.length - 1) {
      s.addShape("rect", { x: ox + otaW, y: 4.72, w: otaGap, h: 0.05, fill: { color: C.amber }, line: { color: C.amber } });
    }
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 11 – Outil de Test (Python GUI)
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  slideHeader(s, "Outil de Test — Simulateur RS-485 Python", C.light);

  // Left: description
  cardRect(s, 0.3, 0.98, 4.5, 4.35);
  amberBar(s, 0.3, 0.98, 4.35);
  s.addText("Simulateur GUI Python", { x: 0.5, y: 1.03, w: 4.1, h: 0.32, fontSize: 14, bold: true, color: C.blue, fontFace: "Calibri" });

  iconCircle(s, 0.5, 1.42, 0.48, "7C3AED", "P");
  s.addText("Interface Tkinter", { x: 1.1, y: 1.48, w: 3.5, h: 0.28, fontSize: 12, bold: true, color: C.text, fontFace: "Calibri" });
  s.addText("Bibliothèque GUI standard Python", { x: 1.1, y: 1.76, w: 3.5, h: 0.22, fontSize: 9.5, color: C.muted, fontFace: "Calibri" });

  const features = [
    { icon: "▸", text: "Simulation du protocole RS-485 LIGO T54" },
    { icon: "▸", text: "Mode Esclave: répond aux commandes ESP32" },
    { icon: "▸", text: "Mode Maître: envoie des commandes de test" },
    { icon: "▸", text: "Calcul CRC-8 automatique (polynomial 0x31)" },
    { icon: "▸", text: "Affichage hex/ASCII des trames en temps réel" },
    { icon: "▸", text: "Injection de valeurs personnalisées" },
    { icon: "▸", text: "Log des échanges avec horodatage" },
    { icon: "▸", text: "Simulation de timeouts et d'erreurs CRC" },
  ];
  features.forEach((f, i) => {
    s.addText(f.icon + " " + f.text, { x: 0.5, y: 2.12 + i * 0.325, w: 4.1, h: 0.32, fontSize: 10, color: C.text, fontFace: "Calibri" });
  });

  // Right: Mock GUI terminal
  cardRect(s, 5.0, 0.98, 4.7, 4.35);
  s.addShape("rect", { x: 5.0, y: 0.98, w: 4.7, h: 0.3, fill: { color: "1E293B" }, line: { color: "1E293B" } });
  // Window dots
  ["FF5F57", "FFBD2E", "28C840"].forEach((c, i) => {
    s.addShape("ellipse", { x: 5.12 + i * 0.22, y: 1.04, w: 0.14, h: 0.14, fill: { color: c }, line: { color: c } });
  });
  s.addText("Simulateur RS-485 SOJI — v1.0", { x: 5.4, y: 1.0, w: 3.8, h: 0.26, fontSize: 9, color: "94A3B8", align: "center", fontFace: "Calibri", valign: "middle" });

  s.addShape("rect", { x: 5.0, y: 1.28, w: 4.7, h: 3.98, fill: { color: "0F172A" }, line: { color: "1E293B" } });

  const terminal = [
    { t: "$ Mode: ESCLAVE | Port: COM3 | 9600,8N1", c: "64748B" },
    { t: "",                                         c: C.white },
    { t: "[14:22:01] RX ← 31 01 06 00 2F",          c: "86EFAC" },
    { t: "  Commande valide | ADDR=0x01 | CRC OK",   c: "64748B" },
    { t: "[14:22:01] TX → 3E 01 06 16 A6 B1 C0 12 4A", c: C.amber },
    { t: "  Niveau: 45494 | Temp: 22.4°C | Fréq: 4928 Hz", c: "64748B" },
    { t: "",                                         c: C.white },
    { t: "[14:22:31] RX ← 31 01 06 00 2F",          c: "86EFAC" },
    { t: "  Commande valide | ADDR=0x01 | CRC OK",   c: "64748B" },
    { t: "[14:22:31] TX → 3E 01 06 16 A7 B1 C0 12 51", c: C.amber },
    { t: "  Niveau: 45495 | Temp: 22.4°C | Fréq: 4929 Hz", c: "64748B" },
    { t: "",                                         c: C.white },
    { t: "[14:23:01] RX ← 31 01 06 00 2F",          c: "86EFAC" },
    { t: "  CRC ERROR injected (test mode)",         c: "FCA5A5" },
    { t: "  No response sent — timeout test",        c: "FCA5A5" },
    { t: "",                                         c: C.white },
    { t: "Total: 42 échanges | Erreurs: 1 | OK: 97.6%", c: "94A3B8" },
  ];
  terminal.forEach((line, i) => {
    s.addText(line.t, { x: 5.1, y: 1.35 + i * 0.215, w: 4.5, h: 0.21, fontSize: 8, color: line.c, fontFace: "Consolas", valign: "middle" });
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SLIDE 12 – Conclusion
// ══════════════════════════════════════════════════════════════════════════════
{
  const s = pres.addSlide();
  s.background = { color: C.navy };

  // Amber left stripe
  s.addShape("rect", { x: 0, y: 0, w: 0.35, h: 5.625, fill: { color: C.amber }, line: { color: C.amber } });

  // Decorative circles
  s.addShape("ellipse", { x: 7.2, y: 2.5, w: 3.5, h: 3.5, fill: { color: C.blue }, line: { color: C.blue } });
  s.addShape("ellipse", { x: 8.0, y: 3.2, w: 2.5, h: 2.5, fill: { color: "0D6FA0" }, line: { color: "0D6FA0" } });

  s.addText("SOJI", { x: 0.65, y: 0.7, w: 6, h: 0.7, fontSize: 52, bold: true, fontFace: "Arial Black", color: C.amber });
  s.addText("Récapitulatif Technique", { x: 0.65, y: 1.42, w: 6.5, h: 0.42, fontSize: 18, color: "B0C4DE", fontFace: "Calibri" });
  s.addShape("rect", { x: 0.65, y: 1.9, w: 3.0, h: 0.05, fill: { color: C.amber }, line: { color: C.amber } });

  const bullets = [
    "ESP32 + FreeRTOS — architecture temps réel robuste",
    "Capteur LIGO T54 RS-485 — protocole trame CRC-8",
    "Modem SIM7600 — 4G LTE + GPS + NTP intégré",
    "8 géométries de réservoirs — calcul volumétrique précis",
    "MQTT JSON — télémétrie cloud en temps réel",
    "SD Card backup + retry — fiabilité garantie",
    "OTA — mises à jour firmware à distance",
  ];
  bullets.forEach((b, i) => {
    s.addShape("rect", { x: 0.65, y: 2.1 + i * 0.4, w: 0.06, h: 0.26, fill: { color: C.amber }, line: { color: C.amber } });
    s.addText(b, { x: 0.85, y: 2.1 + i * 0.4, w: 6.1, h: 0.3, fontSize: 11.5, color: C.white, fontFace: "Calibri", valign: "middle" });
  });

  // Bottom bar
  s.addShape("rect", { x: 0.35, y: 5.25, w: 9.65, h: 0.375, fill: { color: C.blue }, line: { color: C.blue } });
  s.addText("SOJI IoT — Système de Surveillance du Niveau de Carburant  |  ESP32 · RS-485 · 4G LTE · MQTT  |  © 2026", {
    x: 0.5, y: 5.26, w: 9.3, h: 0.35, fontSize: 9, color: "B0C4DE", align: "center", fontFace: "Calibri", valign: "middle",
  });
}

// ─── Write file ───────────────────────────────────────────────────────────────
pres.writeFile({ fileName: "C:/Users/MSI/Desktop/SOJI-Technique-FR.pptx" })
  .then(() => console.log("✅  SOJI-Technique-FR.pptx written successfully!"))
  .catch((err) => { console.error("❌ Error:", err); process.exit(1); });
