"use strict";
const pptxgen = require("pptxgenjs");

// ─── Color Palette ───────────────────────────────────────────────────────────
const C = {
  navy:    "021B2C",   // primary dark
  deep:    "065A82",   // secondary blue
  mint:    "02C39A",   // accent green
  cardBg:  "F0F9FF",   // light card bg
  white:   "FFFFFF",
  textDk:  "1E3A4C",   // dark text
  textMt:  "64748B",   // muted text
  lightBg: "EFF6FF",   // slide bg (light slides)
  teal:    "0891B2",   // supporting teal
  mintDk:  "019A78",   // darker mint for contrast
};

// ─── Factory helpers (NEVER reuse option objects) ────────────────────────────
const makeShadow = () => ({ type: "outer", color: "000000", blur: 8, offset: 3, angle: 135, opacity: 0.12 });
const makeCardShadow = () => ({ type: "outer", color: "000000", blur: 6, offset: 2, angle: 135, opacity: 0.10 });

// ─── Init ─────────────────────────────────────────────────────────────────────
let pres = new pptxgen();
pres.layout = "LAYOUT_16x9";
pres.author = "Smart-Gridix";
pres.title  = "Smart-Gridix Commercial Presentation";

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 1 — COVER
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.navy };

  // Full-width accent strip at top
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 10, h: 0.06, fill: { color: C.mint }, line: { color: C.mint } });

  // Left accent bar (thick)
  sl.addShape(pres.shapes.RECTANGLE, { x: 0.6, y: 1.2, w: 0.1, h: 3.2, fill: { color: C.mint }, line: { color: C.mint } });

  // Decorative circles — background
  sl.addShape(pres.shapes.OVAL, { x: 6.8, y: -0.3, w: 3.5, h: 3.5, fill: { color: C.deep, transparency: 60 }, line: { color: C.deep, transparency: 60 } });
  sl.addShape(pres.shapes.OVAL, { x: 7.8, y: 1.8, w: 2.2, h: 2.2, fill: { color: C.mint, transparency: 75 }, line: { color: C.mint, transparency: 75 } });

  // Company name
  sl.addText("SMART-GRIDIX", {
    x: 0.9, y: 1.3, w: 7.5, h: 1.1,
    fontSize: 52, fontFace: "Arial Black", bold: true,
    color: C.white, charSpacing: 6, margin: 0
  });

  // Subtitle
  sl.addText("Intelligent Fuel & Tank Management Platform", {
    x: 0.9, y: 2.5, w: 7.5, h: 0.7,
    fontSize: 20, fontFace: "Calibri", color: "A8D8EA", margin: 0
  });

  // Tagline with mint accent
  sl.addText("Monitor.  Control.  Optimize.", {
    x: 0.9, y: 3.3, w: 7, h: 0.6,
    fontSize: 16, fontFace: "Calibri", bold: true,
    color: C.mint, charSpacing: 2, margin: 0
  });

  // Bottom bar
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 5.25, w: 10, h: 0.375, fill: { color: C.deep }, line: { color: C.deep } });
  sl.addText("IoT  ·  Real-Time Monitoring  ·  Multi-Platform  ·  Smart Analytics", {
    x: 0, y: 5.25, w: 10, h: 0.375,
    fontSize: 11, fontFace: "Calibri", color: "A8D8EA",
    align: "center", valign: "middle"
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 2 — THE PROBLEM
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.lightBg };

  // Header stripe
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 10, h: 1.05, fill: { color: C.navy }, line: { color: C.navy } });
  sl.addText("THE CHALLENGE", {
    x: 0.5, y: 0, w: 9, h: 1.05,
    fontSize: 28, fontFace: "Arial Black", bold: true,
    color: C.white, valign: "middle", margin: 0
  });
  sl.addText("Industry pain points driving inefficiency and loss", {
    x: 0.5, y: 0, w: 9, h: 1.05,
    fontSize: 13, fontFace: "Calibri", color: C.mint,
    align: "right", valign: "middle", margin: 16
  });

  const problems = [
    { icon: "⛽", label: "Fuel Theft",         desc: "Undetected pilferage accounts for up to 15% of fuel losses across industrial sites." },
    { icon: "📋", label: "Manual Errors",       desc: "Paper-based monitoring creates data gaps, human errors, and delayed decision-making." },
    { icon: "📡", label: "No Real-Time View",   desc: "Lack of live data means issues are discovered hours or days after they occur." },
    { icon: "💸", label: "High Operational Cost", desc: "Inefficient dispatching and over-ordering inflate operating budgets unnecessarily." },
  ];

  const cols = [0.35, 5.1];
  const rows = [1.25, 3.3];

  problems.forEach((p, i) => {
    const x = cols[i % 2];
    const y = rows[Math.floor(i / 2)];
    const w = 4.5;
    const h = 1.75;

    // Card bg
    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w, h,
      fill: { color: C.white },
      line: { color: "DBEAFE", width: 1.2 },
      shadow: makeCardShadow()
    });
    // Left accent
    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w: 0.08, h,
      fill: { color: C.deep }, line: { color: C.deep }
    });
    // Icon circle
    sl.addShape(pres.shapes.OVAL, {
      x: x + 0.22, y: y + 0.45, w: 0.65, h: 0.65,
      fill: { color: "EFF6FF" }, line: { color: "BFDBFE", width: 1 }
    });
    sl.addText(p.icon, {
      x: x + 0.22, y: y + 0.45, w: 0.65, h: 0.65,
      fontSize: 18, align: "center", valign: "middle"
    });
    // Title
    sl.addText(p.label, {
      x: x + 1.0, y: y + 0.28, w: 3.3, h: 0.38,
      fontSize: 15, fontFace: "Arial Black", bold: true, color: C.textDk, margin: 0
    });
    // Description
    sl.addText(p.desc, {
      x: x + 1.0, y: y + 0.68, w: 3.3, h: 0.9,
      fontSize: 12, fontFace: "Calibri", color: C.textMt, margin: 0
    });
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 3 — OUR SOLUTION
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.white };

  // Left panel (dark)
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 5.2, h: 5.625, fill: { color: C.navy }, line: { color: C.navy } });

  // Accent top strip on left panel
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 5.2, h: 0.06, fill: { color: C.mint }, line: { color: C.mint } });

  sl.addText("OUR SOLUTION", {
    x: 0.4, y: 0.35, w: 4.5, h: 0.55,
    fontSize: 10, fontFace: "Calibri", bold: true,
    color: C.mint, charSpacing: 4, margin: 0
  });
  sl.addText("Smart-Gridix", {
    x: 0.4, y: 0.9, w: 4.5, h: 0.85,
    fontSize: 36, fontFace: "Arial Black", bold: true,
    color: C.white, margin: 0
  });
  sl.addText("A unified IoT platform that gives fuel & tank operators complete real-time visibility, automated alerts, and actionable analytics — across every site, from any device.", {
    x: 0.4, y: 1.85, w: 4.4, h: 1.5,
    fontSize: 14, fontFace: "Calibri", color: "A8D8EA",
    margin: 0
  });

  const bullets = [
    "Connects sensors to cloud in under 2 seconds",
    "Works on web, iOS, and Android simultaneously",
    "Reduces fuel loss through automated theft detection",
    "Scales from 1 site to enterprise-wide deployments",
  ];
  bullets.forEach((b, i) => {
    sl.addShape(pres.shapes.RECTANGLE, {
      x: 0.4, y: 3.5 + i * 0.38, w: 0.08, h: 0.22,
      fill: { color: C.mint }, line: { color: C.mint }
    });
    sl.addText(b, {
      x: 0.62, y: 3.48 + i * 0.38, w: 4.2, h: 0.3,
      fontSize: 12, fontFace: "Calibri", color: "D1EAF5", margin: 0
    });
  });

  // Right panel — stat callouts
  const stats = [
    { val: "2s",   lbl: "Live data latency",     sub: "MQTT real-time protocol" },
    { val: "40%",  lbl: "Reduction in fuel loss", sub: "Average client result" },
    { val: "99.9%",lbl: "Platform uptime",        sub: "SLA-backed reliability" },
    { val: "3",    lbl: "Platforms supported",    sub: "Web · iOS · Android" },
  ];

  const sxBase = 5.5;
  stats.forEach((s, i) => {
    const col = i % 2;
    const row = Math.floor(i / 2);
    const sx = sxBase + col * 2.15;
    const sy = 0.5 + row * 2.35;

    sl.addShape(pres.shapes.RECTANGLE, {
      x: sx, y: sy, w: 2.0, h: 2.1,
      fill: { color: C.cardBg },
      line: { color: "BFDBFE", width: 1 },
      shadow: makeCardShadow()
    });
    sl.addShape(pres.shapes.RECTANGLE, {
      x: sx, y: sy, w: 2.0, h: 0.06,
      fill: { color: C.mint }, line: { color: C.mint }
    });
    // Use smaller font for longer values to prevent wrapping
    const statFontSize = s.val.length >= 5 ? 34 : 44;
    sl.addText(s.val, {
      x: sx, y: sy + 0.2, w: 2.0, h: 0.85,
      fontSize: statFontSize, fontFace: "Arial Black", bold: true,
      color: C.deep, align: "center", margin: 0
    });
    sl.addText(s.lbl, {
      x: sx + 0.1, y: sy + 1.05, w: 1.8, h: 0.4,
      fontSize: 12, fontFace: "Calibri", bold: true,
      color: C.textDk, align: "center", margin: 0
    });
    sl.addText(s.sub, {
      x: sx + 0.1, y: sy + 1.48, w: 1.8, h: 0.35,
      fontSize: 10, fontFace: "Calibri", color: C.textMt,
      align: "center", italic: true, margin: 0
    });
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 4 — CORE FEATURES (2×3 grid)
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.lightBg };

  // Top band
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 10, h: 1.0, fill: { color: C.navy }, line: { color: C.navy } });
  sl.addText("CORE FEATURES", {
    x: 0.5, y: 0, w: 9, h: 1.0,
    fontSize: 28, fontFace: "Arial Black", bold: true,
    color: C.white, valign: "middle", margin: 0
  });

  const features = [
    { icon: "📊", color: "0284C7", title: "Real-Time Monitoring",   desc: "Live tank levels and sensor data streamed via MQTT protocol." },
    { icon: "🔔", color: "059669", title: "Smart Alerts",           desc: "Automated notifications for low levels, leaks, and anomalies." },
    { icon: "🗺",  color: "7C3AED", title: "Interactive Maps",      desc: "Leaflet-powered maps with site markers and status overlays." },
    { icon: "📈", color: "DC2626", title: "Analytics & Reports",    desc: "Historical charts, trends, and one-click PDF report export." },
    { icon: "📱", color: "D97706", title: "Multi-Platform",         desc: "Next.js web dashboard plus Flutter apps for iOS & Android." },
    { icon: "💳", color: C.deep,   title: "Payment Integration",    desc: "Stripe-powered billing with subscription tier management." },
  ];

  const cardW = 2.9;
  const cardH = 1.8;
  const colStarts = [0.3, 3.55, 6.8];
  const rowStarts = [1.15, 3.2];

  features.forEach((f, i) => {
    const col = i % 3;
    const row = Math.floor(i / 3);
    const x = colStarts[col];
    const y = rowStarts[row];

    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w: cardW, h: cardH,
      fill: { color: C.white },
      line: { color: "DBEAFE", width: 1 },
      shadow: makeCardShadow()
    });
    // Top accent
    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w: cardW, h: 0.07,
      fill: { color: f.color }, line: { color: f.color }
    });
    // Icon circle
    sl.addShape(pres.shapes.OVAL, {
      x: x + 0.18, y: y + 0.22, w: 0.58, h: 0.58,
      fill: { color: f.color, transparency: 15 }, line: { color: f.color, transparency: 40 }
    });
    sl.addText(f.icon, {
      x: x + 0.18, y: y + 0.22, w: 0.58, h: 0.58,
      fontSize: 18, align: "center", valign: "middle"
    });
    sl.addText(f.title, {
      x: x + 0.88, y: y + 0.22, w: 1.9, h: 0.55,
      fontSize: 11, fontFace: "Arial Black", bold: true,
      color: C.textDk, margin: 0
    });
    sl.addText(f.desc, {
      x: x + 0.12, y: y + 0.95, w: cardW - 0.24, h: 0.73,
      fontSize: 11, fontFace: "Calibri", color: C.textMt, margin: 0
    });
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 5 — PLATFORM ACCESS (3 columns, dark bg)
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.navy };

  // Top accent
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 10, h: 0.06, fill: { color: C.mint }, line: { color: C.mint } });

  sl.addText("PLATFORM ACCESS", {
    x: 0.5, y: 0.22, w: 9, h: 0.7,
    fontSize: 30, fontFace: "Arial Black", bold: true,
    color: C.white, align: "center", margin: 0
  });
  sl.addText("One ecosystem. Three access points.", {
    x: 0.5, y: 0.88, w: 9, h: 0.4,
    fontSize: 14, fontFace: "Calibri", color: C.mint,
    align: "center", italic: true, margin: 0
  });

  const platforms = [
    {
      icon: "🖥",
      title: "Web Dashboard",
      tech: "Next.js 14 + React 18",
      points: ["Full admin control panel", "Real-time charts (Recharts)", "Interactive Leaflet maps", "PDF report generation", "User & role management"],
      color: C.teal
    },
    {
      icon: "📱",
      title: "Mobile App",
      tech: "Flutter (iOS & Android)",
      points: ["Full & Lite versions", "Push notifications", "FL Chart visualizations", "Offline-capable lite mode", "On-the-go monitoring"],
      color: C.mint
    },
    {
      icon: "🔌",
      title: "API Integration",
      tech: "Express.js REST + MQTT",
      points: ["JWT-secured endpoints", "MQTT IoT sensor bridge", "MongoDB data store", "Stripe webhook events", "Third-party connectors"],
      color: "7C3AED"
    },
  ];

  platforms.forEach((p, i) => {
    const x = 0.4 + i * 3.1;
    const y = 1.45;
    const w = 2.9;
    const h = 3.85;

    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w, h,
      fill: { color: C.deep, transparency: 55 },
      line: { color: p.color, width: 1.5 }
    });
    // Top accent stripe
    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w, h: 0.07,
      fill: { color: p.color }, line: { color: p.color }
    });
    // Icon circle
    sl.addShape(pres.shapes.OVAL, {
      x: x + (w / 2) - 0.45, y: y + 0.22, w: 0.9, h: 0.9,
      fill: { color: p.color, transparency: 25 }, line: { color: p.color }
    });
    sl.addText(p.icon, {
      x: x + (w / 2) - 0.45, y: y + 0.22, w: 0.9, h: 0.9,
      fontSize: 26, align: "center", valign: "middle"
    });
    sl.addText(p.title, {
      x: x + 0.12, y: y + 1.26, w: w - 0.24, h: 0.45,
      fontSize: 15, fontFace: "Arial Black", bold: true,
      color: C.white, align: "center", margin: 0
    });
    sl.addText(p.tech, {
      x: x + 0.12, y: y + 1.72, w: w - 0.24, h: 0.3,
      fontSize: 10, fontFace: "Calibri", color: p.color,
      align: "center", italic: true, margin: 0
    });
    // Feature bullets
    p.points.forEach((pt, j) => {
      sl.addShape(pres.shapes.OVAL, {
        x: x + 0.22, y: y + 2.2 + j * 0.3, w: 0.1, h: 0.1,
        fill: { color: p.color }, line: { color: p.color }
      });
      sl.addText(pt, {
        x: x + 0.4, y: y + 2.15 + j * 0.3, w: w - 0.55, h: 0.28,
        fontSize: 11, fontFace: "Calibri", color: "D1EAF5", margin: 0
      });
    });
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 6 — REAL-TIME DASHBOARD
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.white };

  // Left header area
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 4.5, h: 5.625, fill: { color: C.navy }, line: { color: C.navy } });
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 4.5, h: 0.06, fill: { color: C.mint }, line: { color: C.mint } });

  sl.addText("REAL-TIME\nDASHBOARD", {
    x: 0.4, y: 0.5, w: 3.8, h: 1.3,
    fontSize: 30, fontFace: "Arial Black", bold: true,
    color: C.white, margin: 0
  });
  sl.addText("Monitor every tank and site from a single unified view — on any device, anywhere.", {
    x: 0.4, y: 1.95, w: 3.8, h: 1.0,
    fontSize: 14, fontFace: "Calibri", color: "A8D8EA", margin: 0
  });

  const features = [
    { icon: "🔴", txt: "Live tank level gauges" },
    { icon: "🟢", txt: "Site operational status" },
    { icon: "🔔", txt: "Active alert feed" },
    { icon: "📍", txt: "GPS-mapped locations" },
    { icon: "📊", txt: "Consumption trends" },
  ];
  features.forEach((f, i) => {
    sl.addShape(pres.shapes.RECTANGLE, {
      x: 0.4, y: 3.1 + i * 0.42, w: 0.08, h: 0.25,
      fill: { color: C.mint }, line: { color: C.mint }
    });
    sl.addText(`${f.icon}  ${f.txt}`, {
      x: 0.6, y: 3.08 + i * 0.42, w: 3.6, h: 0.32,
      fontSize: 12, fontFace: "Calibri", color: "D1EAF5", margin: 0
    });
  });

  // Right panel — metric cards
  const metrics = [
    { label: "Active Sites",      val: "24",    unit: "online",       color: C.teal },
    { label: "Tank Avg Level",    val: "73%",   unit: "capacity",     color: C.mint },
    { label: "Active Alerts",     val: "3",     unit: "requires attention", color: "EF4444" },
    { label: "Data Refresh",      val: "<2s",   unit: "live latency", color: "7C3AED" },
  ];

  metrics.forEach((m, i) => {
    const col = i % 2;
    const row = Math.floor(i / 2);
    const x = 4.8 + col * 2.55;
    const y = 0.45 + row * 2.45;

    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w: 2.35, h: 2.2,
      fill: { color: C.cardBg },
      line: { color: "BFDBFE", width: 1 },
      shadow: makeCardShadow()
    });
    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w: 2.35, h: 0.07,
      fill: { color: m.color }, line: { color: m.color }
    });
    sl.addText(m.val, {
      x, y: y + 0.3, w: 2.35, h: 0.9,
      fontSize: 46, fontFace: "Arial Black", bold: true,
      color: m.color, align: "center", margin: 0
    });
    sl.addText(m.label, {
      x: x + 0.1, y: y + 1.22, w: 2.15, h: 0.38,
      fontSize: 12, fontFace: "Calibri", bold: true,
      color: C.textDk, align: "center", margin: 0
    });
    sl.addText(m.unit, {
      x: x + 0.1, y: y + 1.6, w: 2.15, h: 0.35,
      fontSize: 10, fontFace: "Calibri", color: C.textMt,
      align: "center", italic: true, margin: 0
    });
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 7 — ANALYTICS & REPORTING
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.lightBg };

  // Header
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 10, h: 1.0, fill: { color: C.navy }, line: { color: C.navy } });
  sl.addText("ANALYTICS & REPORTING", {
    x: 0.5, y: 0, w: 9, h: 1.0,
    fontSize: 26, fontFace: "Arial Black", bold: true,
    color: C.white, valign: "middle", margin: 0
  });
  sl.addText("Turn raw sensor data into strategic insight", {
    x: 0.5, y: 0, w: 9, h: 1.0,
    fontSize: 13, fontFace: "Calibri", color: C.mint,
    align: "right", valign: "middle", margin: 14
  });

  // Bar chart — monthly fuel consumption
  sl.addChart(pres.charts.BAR, [{
    name: "Fuel Consumed (L)",
    labels: ["Jan", "Feb", "Mar", "Apr", "May", "Jun"],
    values: [12400, 11800, 13200, 12900, 14100, 13500]
  }], {
    x: 0.35, y: 1.1, w: 6.0, h: 3.95,
    barDir: "col",
    chartColors: [C.deep],
    chartArea: { fill: { color: C.white }, roundedCorners: true },
    catAxisLabelColor: C.textMt,
    valAxisLabelColor: C.textMt,
    valGridLine: { color: "E2E8F0", size: 0.5 },
    catGridLine: { style: "none" },
    showValue: true,
    dataLabelColor: C.textDk,
    dataLabelFontSize: 10,
    showLegend: false,
    showTitle: true,
    title: "Monthly Fuel Consumption (Litres)",
    titleColor: C.textDk,
    titleFontSize: 12,
  });

  // Right: feature callouts
  const callouts = [
    { icon: "📄", title: "PDF Export",       desc: "One-click export of consumption reports, alerts, and site summaries." },
    { icon: "📅", title: "Historical Data",  desc: "Query and compare data across any date range with trend overlays." },
    { icon: "⚡", title: "Live Charts",      desc: "Recharts (web) and FL Chart (mobile) render data in real time." },
    { icon: "🎯", title: "KPI Dashboards",   desc: "Custom KPI panels for efficiency, loss rates, and site comparisons." },
  ];

  callouts.forEach((c, i) => {
    const x = 6.7;
    const y = 1.1 + i * 1.1;
    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w: 3.0, h: 0.9,
      fill: { color: C.white },
      line: { color: "DBEAFE", width: 1 },
      shadow: makeCardShadow()
    });
    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w: 0.07, h: 0.9,
      fill: { color: C.mint }, line: { color: C.mint }
    });
    sl.addText(c.icon, {
      x: x + 0.18, y: y + 0.18, w: 0.5, h: 0.5,
      fontSize: 18, align: "center", valign: "middle"
    });
    sl.addText(c.title, {
      x: x + 0.78, y: y + 0.1, w: 2.1, h: 0.3,
      fontSize: 12, fontFace: "Arial Black", bold: true, color: C.textDk, margin: 0
    });
    sl.addText(c.desc, {
      x: x + 0.78, y: y + 0.42, w: 2.1, h: 0.42,
      fontSize: 10, fontFace: "Calibri", color: C.textMt, margin: 0
    });
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 8 — SECURITY & SCALABILITY
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.white };

  // Top bar
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 10, h: 1.0, fill: { color: C.deep }, line: { color: C.deep } });
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 10, h: 0.06, fill: { color: C.mint }, line: { color: C.mint } });
  sl.addText("SECURITY & SCALABILITY", {
    x: 0.5, y: 0, w: 9, h: 1.0,
    fontSize: 26, fontFace: "Arial Black", bold: true,
    color: C.white, valign: "middle", margin: 0
  });

  const pillars = [
    {
      icon: "🔑", color: "0284C7", title: "JWT Authentication",
      points: ["Stateless token-based auth", "Token expiry & refresh flows", "Secure HTTPS transport", "Session invalidation support"]
    },
    {
      icon: "👥", color: "059669", title: "Role-Based Access",
      points: ["Admin / SuperAdmin tiers", "Granular permission scopes", "Site-level data isolation", "Audit-ready access logs"]
    },
    {
      icon: "🛢", color: "7C3AED", title: "MongoDB Scalability",
      points: ["Horizontal sharding support", "Indexed queries for speed", "Geospatial data indexing", "Automated cloud backups"]
    },
    {
      icon: "📡", color: "D97706", title: "MQTT IoT Protocol",
      points: ["Lightweight pub/sub protocol", "Handles thousands of sensors", "Quality-of-service levels", "Encrypted broker connections"]
    },
  ];

  pillars.forEach((p, i) => {
    const x = 0.35 + i * 2.35;
    const y = 1.2;
    const w = 2.15;
    const h = 4.1;

    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w, h,
      fill: { color: C.cardBg },
      line: { color: "DBEAFE", width: 1.2 },
      shadow: makeCardShadow()
    });
    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w, h: 0.08,
      fill: { color: p.color }, line: { color: p.color }
    });
    // Icon circle
    sl.addShape(pres.shapes.OVAL, {
      x: x + (w / 2) - 0.42, y: y + 0.22, w: 0.84, h: 0.84,
      fill: { color: p.color, transparency: 20 }, line: { color: p.color }
    });
    sl.addText(p.icon, {
      x: x + (w / 2) - 0.42, y: y + 0.22, w: 0.84, h: 0.84,
      fontSize: 22, align: "center", valign: "middle"
    });
    sl.addText(p.title, {
      x: x + 0.1, y: y + 1.2, w: w - 0.2, h: 0.55,
      fontSize: 12, fontFace: "Arial Black", bold: true,
      color: C.textDk, align: "center", margin: 0
    });
    p.points.forEach((pt, j) => {
      sl.addShape(pres.shapes.OVAL, {
        x: x + 0.2, y: y + 1.95 + j * 0.46, w: 0.1, h: 0.1,
        fill: { color: p.color }, line: { color: p.color }
      });
      sl.addText(pt, {
        x: x + 0.38, y: y + 1.9 + j * 0.46, w: w - 0.5, h: 0.36,
        fontSize: 10, fontFace: "Calibri", color: C.textDk, margin: 0
      });
    });
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 9 — BUSINESS VALUE / ROI
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.navy };

  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 10, h: 0.06, fill: { color: C.mint }, line: { color: C.mint } });

  sl.addText("BUSINESS VALUE & ROI", {
    x: 0.5, y: 0.18, w: 9, h: 0.7,
    fontSize: 30, fontFace: "Arial Black", bold: true,
    color: C.white, align: "center", margin: 0
  });
  sl.addText("Measurable impact from day one", {
    x: 0.5, y: 0.85, w: 9, h: 0.35,
    fontSize: 14, fontFace: "Calibri", color: C.mint,
    align: "center", italic: true, margin: 0
  });

  const stats = [
    { val: "40%",    lbl: "Reduction in fuel losses",    sub: "Through real-time anomaly detection",  color: C.mint },
    { val: "99.9%",  lbl: "Platform uptime SLA",         sub: "Guaranteed availability commitment",   color: C.teal },
    { val: "<2s",    lbl: "Real-time data latency",       sub: "From sensor to dashboard",             color: "7C3AED" },
    { val: "3×",     lbl: "Faster incident response",     sub: "Vs. manual monitoring methods",        color: "F59E0B" },
    { val: "60%",    lbl: "Reduction in manual work",     sub: "Automated reports and alerts",         color: "EF4444" },
    { val: "∞",      lbl: "Sites supported",              sub: "Scales from 1 to enterprise-wide",     color: C.mint },
  ];

  const cols3 = [0.35, 3.55, 6.75];
  stats.forEach((s, i) => {
    const col = i % 3;
    const row = Math.floor(i / 3);
    const x = cols3[col];
    const y = 1.35 + row * 2.05;
    const w = 2.9;
    const h = 1.85;

    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w, h,
      fill: { color: C.deep, transparency: 50 },
      line: { color: s.color, width: 1.5 }
    });
    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w, h: 0.07,
      fill: { color: s.color }, line: { color: s.color }
    });
    sl.addText(s.val, {
      x, y: y + 0.1, w, h: 0.85,
      fontSize: 48, fontFace: "Arial Black", bold: true,
      color: s.color, align: "center", margin: 0
    });
    sl.addText(s.lbl, {
      x: x + 0.1, y: y + 0.96, w: w - 0.2, h: 0.38,
      fontSize: 12, fontFace: "Calibri", bold: true,
      color: C.white, align: "center", margin: 0
    });
    sl.addText(s.sub, {
      x: x + 0.1, y: y + 1.35, w: w - 0.2, h: 0.35,
      fontSize: 10, fontFace: "Calibri", color: "A8D8EA",
      align: "center", italic: true, margin: 0
    });
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 10 — PRICING PLANS
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.lightBg };

  // Header
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 10, h: 1.0, fill: { color: C.navy }, line: { color: C.navy } });
  sl.addText("PRICING PLANS", {
    x: 0.5, y: 0, w: 9, h: 1.0,
    fontSize: 28, fontFace: "Arial Black", bold: true,
    color: C.white, valign: "middle", margin: 0
  });
  sl.addText("Flexible tiers for every operation size", {
    x: 0.5, y: 0, w: 9, h: 1.0,
    fontSize: 13, fontFace: "Calibri", color: C.mint,
    align: "right", valign: "middle", margin: 14
  });

  const plans = [
    {
      name: "Starter",
      price: "$49",
      period: "/month",
      color: C.teal,
      highlight: false,
      features: ["Up to 5 sites", "10 sensor connections", "Web dashboard access", "Email alerts", "7-day data history", "Community support"],
    },
    {
      name: "Professional",
      price: "$149",
      period: "/month",
      color: C.mint,
      highlight: true,
      features: ["Up to 50 sites", "100 sensor connections", "Web + Mobile apps", "SMS & push alerts", "1-year data history", "PDF report export", "Priority support"],
    },
    {
      name: "Enterprise",
      price: "Custom",
      period: "pricing",
      color: "7C3AED",
      highlight: false,
      features: ["Unlimited sites", "Unlimited sensors", "All platforms + API", "Advanced role management", "Unlimited data history", "Custom integrations", "Dedicated SLA & support"],
    },
  ];

  plans.forEach((p, i) => {
    const x = 0.35 + i * 3.1;
    const y = p.highlight ? 1.1 : 1.35;
    const w = 2.9;
    const h = p.highlight ? 4.35 : 3.9;

    if (p.highlight) {
      // Outer glow for featured
      sl.addShape(pres.shapes.RECTANGLE, {
        x: x - 0.05, y: y - 0.05, w: w + 0.1, h: h + 0.1,
        fill: { color: C.mint, transparency: 75 },
        line: { color: C.mint, width: 2.5 }
      });
    }

    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w, h,
      fill: { color: C.white },
      line: { color: p.highlight ? C.mint : "DBEAFE", width: p.highlight ? 2 : 1 },
      shadow: makeShadow()
    });
    // Top accent
    sl.addShape(pres.shapes.RECTANGLE, {
      x, y, w, h: 0.07,
      fill: { color: p.color }, line: { color: p.color }
    });

    if (p.highlight) {
      // "Most Popular" badge
      sl.addShape(pres.shapes.RECTANGLE, {
        x: x + 0.55, y: y - 0.18, w: 1.8, h: 0.32,
        fill: { color: C.mint }, line: { color: C.mint }
      });
      sl.addText("★  MOST POPULAR", {
        x: x + 0.55, y: y - 0.18, w: 1.8, h: 0.32,
        fontSize: 9, fontFace: "Calibri", bold: true,
        color: C.navy, align: "center", valign: "middle"
      });
    }

    sl.addText(p.name, {
      x: x + 0.12, y: y + 0.22, w: w - 0.24, h: 0.45,
      fontSize: 16, fontFace: "Arial Black", bold: true,
      color: C.textDk, align: "center", margin: 0
    });
    sl.addText(p.price, {
      x: x + 0.12, y: y + 0.68, w: w - 0.24, h: 0.7,
      fontSize: 38, fontFace: "Arial Black", bold: true,
      color: p.color, align: "center", margin: 0
    });
    sl.addText(p.period, {
      x: x + 0.12, y: y + 1.35, w: w - 0.24, h: 0.3,
      fontSize: 11, fontFace: "Calibri", color: C.textMt,
      align: "center", italic: true, margin: 0
    });

    // Divider
    sl.addShape(pres.shapes.LINE, {
      x: x + 0.3, y: y + 1.72, w: w - 0.6, h: 0,
      line: { color: "DBEAFE", width: 1 }
    });

    // Feature list
    p.features.forEach((f, j) => {
      sl.addShape(pres.shapes.OVAL, {
        x: x + 0.25, y: y + 1.88 + j * 0.34, w: 0.14, h: 0.14,
        fill: { color: p.color }, line: { color: p.color }
      });
      sl.addText(f, {
        x: x + 0.48, y: y + 1.84 + j * 0.34, w: w - 0.6, h: 0.28,
        fontSize: 11, fontFace: "Calibri", color: C.textDk, margin: 0
      });
    });
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// SLIDE 11 — CALL TO ACTION / CONTACT
// ═══════════════════════════════════════════════════════════════════════════════
{
  let sl = pres.addSlide();
  sl.background = { color: C.navy };

  // Top/bottom accent bars
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: 10, h: 0.06, fill: { color: C.mint }, line: { color: C.mint } });
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 5.565, w: 10, h: 0.06, fill: { color: C.mint }, line: { color: C.mint } });

  // Background circle decoration
  sl.addShape(pres.shapes.OVAL, { x: -1.0, y: -0.8, w: 4.5, h: 4.5, fill: { color: C.deep, transparency: 60 }, line: { color: C.deep, transparency: 60 } });
  sl.addShape(pres.shapes.OVAL, { x: 7.5, y: 2.8, w: 3.5, h: 3.5, fill: { color: C.mint, transparency: 78 }, line: { color: C.mint, transparency: 78 } });

  sl.addText("Ready to Transform", {
    x: 0.8, y: 0.55, w: 8.5, h: 0.85,
    fontSize: 40, fontFace: "Arial Black", bold: true,
    color: C.white, align: "center", margin: 0
  });
  sl.addText("Your Operations?", {
    x: 0.8, y: 1.35, w: 8.5, h: 0.85,
    fontSize: 40, fontFace: "Arial Black", bold: true,
    color: C.mint, align: "center", margin: 0
  });

  sl.addText("Join leading fuel & tank operators already using Smart-Gridix to cut costs, prevent losses, and gain complete operational visibility.", {
    x: 1.5, y: 2.3, w: 7.0, h: 0.9,
    fontSize: 14, fontFace: "Calibri", color: "A8D8EA",
    align: "center", margin: 0
  });

  // CTA button (simulated)
  sl.addShape(pres.shapes.RECTANGLE, {
    x: 3.3, y: 3.35, w: 3.4, h: 0.62,
    fill: { color: C.mint }, line: { color: C.mint },
    shadow: makeShadow()
  });
  sl.addText("▶  REQUEST A FREE DEMO", {
    x: 3.3, y: 3.35, w: 3.4, h: 0.62,
    fontSize: 13, fontFace: "Arial Black", bold: true,
    color: C.navy, align: "center", valign: "middle"
  });

  // Contact info
  const contacts = [
    { icon: "🌐", txt: "www.smart-gridix.com" },
    { icon: "✉",  txt: "hello@smart-gridix.com" },
    { icon: "📞", txt: "+1 (800) GRIDIX-0" },
  ];
  contacts.forEach((c, i) => {
    sl.addText(`${c.icon}  ${c.txt}`, {
      x: 0.5 + i * 3.1, y: 4.55, w: 2.9, h: 0.45,
      fontSize: 12, fontFace: "Calibri", color: "A8D8EA",
      align: "center", margin: 0
    });
  });

  // Footer
  sl.addShape(pres.shapes.RECTANGLE, { x: 0, y: 5.2, w: 10, h: 0.37, fill: { color: C.deep }, line: { color: C.deep } });
  sl.addText("© 2025 Smart-Gridix  ·  Intelligent Fuel & Tank Management  ·  All Rights Reserved", {
    x: 0, y: 5.2, w: 10, h: 0.37,
    fontSize: 10, fontFace: "Calibri", color: "A8D8EA",
    align: "center", valign: "middle"
  });
}

// ─── Write file ────────────────────────────────────────────────────────────────
pres.writeFile({ fileName: "C:/Users/MSI/Desktop/Smart-Gridix-Commercial.pptx" })
  .then(() => console.log("✅  Saved: C:/Users/MSI/Desktop/Smart-Gridix-Commercial.pptx"))
  .catch(err => { console.error("❌  Error:", err); process.exit(1); });
