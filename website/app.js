// PushFPS website. No dependencies, no tracking. The only network request is GitHub's public
// release API, to link the newest download directly.
(() => {
  "use strict";

  const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  const $ = (s, el = document) => el.querySelector(s);
  const $$ = (s, el = document) => Array.from(el.querySelectorAll(s));
  const theme = () => document.documentElement.getAttribute("data-theme") || "sunset";

  // ---------------------------------------------------------------- looks
  // Sunset City (default), Dark (the original graphite look) and Light. The choice is kept in
  // localStorage when the browser allows it; the <head> script applies it before the first paint.
  const THEME_COLORS = { sunset: "#150a28", classic: "#0e1013", light: "#f3f4f6" };
  function startThemes() {
    const buttons = $$("[data-theme-choice]");
    const meta = document.querySelector('meta[name="theme-color"]');
    const show = () => {
      const t = theme();
      buttons.forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.themeChoice === t)));
      if (meta) meta.setAttribute("content", THEME_COLORS[t] || THEME_COLORS.classic);
    };
    buttons.forEach((b) => b.addEventListener("click", () => {
      const t = b.dataset.themeChoice;
      if (t === theme()) return;
      document.documentElement.setAttribute("data-theme", t);
      try { localStorage.setItem("pushfps-theme", t); } catch (_) { /* private mode: this visit only */ }
      show();
      window.dispatchEvent(new CustomEvent("pushfps-theme", { detail: t }));
    }));
    show();
  }

  // ---------------------------------------------------------------- background flow field
  // Faint streaks drift rightward along a slowly changing flow field, like motion vectors in a
  // frame. About 30 updates per second, paused in background tabs, a single still frame when
  // the visitor prefers reduced motion.
  function startFlow() {
    const canvas = $("#flow");
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    let w = 0, h = 0, dpr = 1, streaks = [], last = 0, t = 0, raf = 0;

    const field = (x, y, time) => {
      // Mostly rightward, with gentle curls. Angle in radians, kept within about +/-35 degrees.
      const a = Math.sin(x * 0.0016 + time * 0.00011) * 0.38 +
                Math.cos(y * 0.0021 - time * 0.00008) * 0.26 +
                Math.sin((x + y) * 0.0009 + time * 0.00005) * 0.18;
      return a;
    };

    const spawn = (s, anywhere) => {
      s.x = anywhere ? Math.random() * w : -40 - Math.random() * 200;
      s.y = Math.random() * h;
      s.speed = 0.35 + Math.random() * 0.9;            // px per ms * 1/16
      s.len = 14 + Math.random() * 46;
      s.green = Math.random() < 0.34;
      s.alpha = (s.green ? 0.06 : 0.035) + Math.random() * 0.07;
      s.alphaLight = (s.green ? 0.16 : 0.07) + Math.random() * 0.1;
      s.width = Math.random() < 0.2 ? 1.6 : 1;
      s.trail = [];
      return s;
    };

    const resize = () => {
      dpr = Math.min(window.devicePixelRatio || 1, 2);
      w = window.innerWidth; h = window.innerHeight;
      canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr);
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      const count = Math.round(Math.min(150, Math.max(50, (w * h) / 11000)));
      streaks = Array.from({ length: count }, () => spawn({}, true));
    };

    let light = theme() === "light";
    const step = (dt) => {
      ctx.clearRect(0, 0, w, h);
      ctx.lineCap = "round";
      for (const s of streaks) {
        const a = field(s.x, s.y, t);
        const v = s.speed * dt * 0.06;
        s.x += Math.cos(a) * v;
        s.y += Math.sin(a) * v;
        s.trail.push(s.x, s.y);
        const maxPts = Math.max(4, Math.round(s.len / Math.max(0.5, v)) * 2);
        while (s.trail.length > Math.min(maxPts, 60)) s.trail.splice(0, 2);
        if (s.x > w + 60 || s.y < -60 || s.y > h + 60) spawn(s, false);
        if (s.trail.length < 4) continue;
        const tr = s.trail;
        const grad = ctx.createLinearGradient(tr[0], tr[1], tr[tr.length - 2], tr[tr.length - 1]);
        const col = light ? (s.green ? "86,170,10" : "60,70,84") : (s.green ? "141,242,55" : "220,228,236");
        grad.addColorStop(0, `rgba(${col},0)`);
        grad.addColorStop(1, `rgba(${col},${light ? s.alphaLight : s.alpha})`);
        ctx.strokeStyle = grad;
        ctx.lineWidth = s.width;
        ctx.beginPath();
        ctx.moveTo(tr[0], tr[1]);
        for (let i = 2; i < tr.length; i += 2) ctx.lineTo(tr[i], tr[i + 1]);
        ctx.stroke();
      }
    };

    const loop = (now) => {
      raf = requestAnimationFrame(loop);
      const dt = now - last;
      if (dt < 31) return;            // about 30 updates per second is plenty for drifting lines
      last = now;
      t += Math.min(dt, 60);
      step(Math.min(dt, 60));
    };

    const still = () => {
      // Advance the field invisibly so the still frame shows full-length streaks.
      for (let i = 0; i < 40; i++) { t += 33; step(33); }
    };

    const run = () => {
      cancelAnimationFrame(raf);
      light = theme() === "light";
      if (theme() === "sunset") { ctx.clearRect(0, 0, w, h); return; }   // the sky runs instead
      if (reduceMotion.matches) { still(); return; }
      last = performance.now();
      raf = requestAnimationFrame(loop);
    };

    resize();
    run();
    let rt = 0;
    window.addEventListener("resize", () => { clearTimeout(rt); rt = setTimeout(() => { resize(); run(); }, 150); });
    document.addEventListener("visibilitychange", () => {
      if (document.hidden) cancelAnimationFrame(raf); else run();
    });
    reduceMotion.addEventListener?.("change", run);
    window.addEventListener("pushfps-theme", run);
  }

  // ---------------------------------------------------------------- Sunset City sky
  // A neon dusk: stars, a striped sun setting behind the city, palms at the edges and a road grid
  // rolling towards the viewer. The still parts are drawn once per resize into an offscreen
  // canvas; each update (about 30 a second) only adds the grid and the twinkling stars. It dims
  // as the page scrolls so text stays easy to read, pauses in background tabs and holds one still
  // frame when the visitor prefers reduced motion.
  function startSky() {
    const canvas = $("#sky");
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    const load = (src) => { const im = new Image(); im.decoding = "async"; im.src = src; return im; };
    const city = load("assets/sunset-skyline.webp");
    const palm = load("assets/sunset-palm.webp");
    let w = 0, h = 0, dpr = 1, horizon = 0, still = null, stars = [], raf = 0, last = 0, t = 0;

    const paintStill = () => {
      still = document.createElement("canvas");
      still.width = canvas.width; still.height = canvas.height;
      const g = still.getContext("2d");
      g.setTransform(dpr, 0, 0, dpr, 0, 0);
      // Sky: night at the top, magenta and coral towards the horizon.
      const sky = g.createLinearGradient(0, 0, 0, horizon);
      sky.addColorStop(0, "#0c0520");
      sky.addColorStop(0.38, "#2a0b4d");
      sky.addColorStop(0.68, "#7a1670");
      sky.addColorStop(0.88, "#e23d82");
      sky.addColorStop(1, "#ff8a50");
      g.fillStyle = sky; g.fillRect(0, 0, w, horizon + 1);
      // Sun: gold to pink, cut by stripes that widen towards the horizon.
      const r = Math.min(w * 0.2, h * 0.24, 260);
      const cx = w * 0.5, cy = horizon - r * 0.32;
      const halo = g.createRadialGradient(cx, cy, r * 0.6, cx, cy, r * 2.4);
      halo.addColorStop(0, "rgba(255,110,150,0.42)"); halo.addColorStop(1, "rgba(255,110,150,0)");
      g.fillStyle = halo; g.fillRect(0, 0, w, horizon);
      const sun = document.createElement("canvas");
      sun.width = Math.ceil(r * 2 * dpr); sun.height = sun.width;
      const sg = sun.getContext("2d");
      sg.setTransform(dpr, 0, 0, dpr, 0, 0);
      const sf = sg.createLinearGradient(0, 0, 0, r * 2);
      sf.addColorStop(0, "#ffe27a"); sf.addColorStop(0.55, "#ff8a5c"); sf.addColorStop(1, "#ff2f8e");
      sg.fillStyle = sf; sg.beginPath(); sg.arc(r, r, r, 0, Math.PI * 2); sg.fill();
      sg.globalCompositeOperation = "destination-out";
      for (let i = 0; i < 7; i++) {
        const y = r * (1.0 + i * 0.15), band = 2 + i * 1.7;
        sg.fillRect(0, y, r * 2, band);
      }
      g.drawImage(sun, cx - r, cy - r, r * 2, r * 2);
      // City along the horizon, repeated when the window is wider than the art.
      if (city.complete && city.naturalWidth) {
        const ch = Math.min(h * 0.2, 250), cw = ch * city.naturalWidth / city.naturalHeight;
        for (let x = (w - cw) / 2 - Math.ceil((w - cw) / 2 / cw) * cw; x < w; x += cw) g.drawImage(city, x, horizon - ch + 1, cw, ch);
      }
      // Ground: dark violet, with a glow line on the horizon.
      const ground = g.createLinearGradient(0, horizon, 0, h);
      ground.addColorStop(0, "#2a0a3e"); ground.addColorStop(1, "#0b0418");
      g.fillStyle = ground; g.fillRect(0, horizon, w, h - horizon);
      const line = g.createLinearGradient(0, horizon - 6, 0, horizon + 10);
      line.addColorStop(0, "rgba(255,140,200,0)"); line.addColorStop(0.4, "rgba(255,170,210,0.9)"); line.addColorStop(1, "rgba(255,140,200,0)");
      g.fillStyle = line; g.fillRect(0, horizon - 6, w, 16);
    };

    // Shade behind the hero text (left column on wide screens, everything on phones), drawn after
    // the palms so the scene keeps its colour where there is no text.
    const paintShade = () => {
      const g = still.getContext("2d");
      if (w >= 900) {
        const left = Math.max(0, (w - Math.min(1180, w - 48)) / 2);
        const sh = g.createLinearGradient(left - 80, 0, left + Math.min(1180, w) * 0.62, 0);
        sh.addColorStop(0, "rgba(21,10,40,0.0)");
        sh.addColorStop(0.18, "rgba(21,10,40,0.58)");
        sh.addColorStop(0.7, "rgba(21,10,40,0.42)");
        sh.addColorStop(1, "rgba(21,10,40,0)");
        g.fillStyle = sh; g.fillRect(0, 0, w, h);
      } else {
        g.fillStyle = "rgba(21,10,40,0.45)"; g.fillRect(0, 0, w, h);
      }
    };

    const paintPalms = () => {
      if (!(palm.complete && palm.naturalWidth)) return;
      const g = still.getContext("2d");
      const ph = Math.min(h * 0.62, 560, w * 0.75), pw = ph * palm.naturalWidth / palm.naturalHeight;
      const base = horizon + h * 0.1;
      g.drawImage(palm, -pw * 0.28, base - ph, pw, ph);
      g.save(); g.translate(w, 0); g.scale(-1, 1);
      g.drawImage(palm, -pw * 0.22, base - ph * 0.86, pw * 0.86, ph * 0.86);
      g.restore();
    };

    const resize = () => {
      dpr = Math.min(window.devicePixelRatio || 1, 1.5);
      w = window.innerWidth; h = window.innerHeight;
      canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr);
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      horizon = Math.round(h * (w < 700 ? 0.6 : 0.66));
      stars = Array.from({ length: Math.round(Math.min(120, w * horizon / 9000)) }, () => ({
        x: Math.random() * w, y: Math.random() * horizon * 0.62,
        s: Math.random() < 0.15 ? 1.6 : 1, p: Math.random() * Math.PI * 2, f: 0.6 + Math.random() * 1.6,
      }));
      paintStill();
      paintPalms();
      paintShade();
    };

    const frame = () => {
      ctx.drawImage(still, 0, 0, w, h);
      // Stars twinkle.
      for (const st of stars) {
        const a = 0.25 + 0.55 * (0.5 + 0.5 * Math.sin(st.p + t * 0.001 * st.f));
        ctx.fillStyle = `rgba(255,236,250,${a * (1 - st.y / horizon)})`;
        ctx.fillRect(st.x, st.y, st.s, st.s);
      }
      // Road grid: lines fan out from the vanishing point; cross lines roll towards the viewer.
      const vx = w * 0.5, depth = h - horizon;
      ctx.lineWidth = 1.2;
      for (let i = -16; i <= 16; i++) {
        const xb = vx + i * w * 0.11;
        const grd = ctx.createLinearGradient(0, horizon, 0, h);
        grd.addColorStop(0, "rgba(255,79,168,0)"); grd.addColorStop(1, "rgba(255,79,168,0.55)");
        ctx.strokeStyle = grd;
        ctx.beginPath(); ctx.moveTo(vx + i * 4, horizon); ctx.lineTo(xb, h); ctx.stroke();
      }
      const n = 14, phase = (t * 0.00018) % 1;
      for (let i = 0; i < n; i++) {
        const p = (i + phase) / n;
        const y = horizon + depth * Math.pow(p, 2.4);
        ctx.fillStyle = `rgba(255,95,180,${0.12 + 0.5 * p})`;
        ctx.fillRect(0, y, w, 0.8 + p * 1.8);
      }
    };

    const loop = (now) => {
      raf = requestAnimationFrame(loop);
      const dt = now - last;
      if (dt < 31) return;
      last = now;
      t += Math.min(dt, 60);
      frame();
    };

    const run = () => {
      cancelAnimationFrame(raf);
      if (theme() !== "sunset" || document.hidden) return;
      if (!w) resize();
      if (reduceMotion.matches) { t = 4000; frame(); return; }
      last = performance.now();
      raf = requestAnimationFrame(loop);
    };

    // Text over the scene: dim it as the visitor scrolls past the hero.
    const dim = () => { canvas.style.opacity = String(1 - Math.min(window.scrollY / (h * 1.1 || 1), 0.62)); };
    window.addEventListener("scroll", dim, { passive: true });

    for (const im of [city, palm]) im.addEventListener("load", () => { if (w) { paintStill(); paintPalms(); paintShade(); if (reduceMotion.matches && theme() === "sunset") frame(); } });
    let rt = 0;
    window.addEventListener("resize", () => { clearTimeout(rt); rt = setTimeout(() => { if (theme() === "sunset") { resize(); run(); } else { w = 0; } }, 150); });
    document.addEventListener("visibilitychange", run);
    reduceMotion.addEventListener?.("change", run);
    window.addEventListener("pushfps-theme", run);
    dim();
    run();
  }

  // ---------------------------------------------------------------- frame lab
  // Two lanes show the same scene. The console lane only gets a new picture 30 times a second;
  // the PushFPS lane gets 60 or 120, and the tape under it marks which pictures are real (white)
  // and which are generated (green). The preview can't show more frames than the screen refreshes.
  function startLab() {
    const lab = $(".lab");
    if (!lab) return;
    const lanes = $$(".lane", lab).map((el) => ({
      kind: el.dataset.lane,
      view: $(".lane-view", el),
      tape: $(".tape", el),
    }));
    const seg = $$(".seg button", lab);
    const pauseBtn = $("#lab-pause");
    const note = $("#lab-note");
    const rateText = $("#push-rate");
    const css = getComputedStyle(document.documentElement);
    let GREEN = css.getPropertyValue("--green").trim() || "#8df237";
    let REAL = css.getPropertyValue("--real").trim() || "#f2f4f7";
    window.addEventListener("pushfps-theme", () => {
      const c = getComputedStyle(document.documentElement);
      GREEN = c.getPropertyValue("--green").trim() || "#8df237";
      REAL = c.getPropertyValue("--real").trim() || "#f2f4f7";
      if (paused) drawAll();
    });

    let target = 120, refresh = 60, paused = reduceMotion.matches, visible = true, raf = 0;
    let simTime = 0, lastNow = 0;
    const samples = [];

    lanes.forEach((l) => {
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      for (const c of [l.view, l.tape]) {
        c._w = c.width; c._h = c.height;
        c.width = c._w * dpr; c.height = c._h * dpr;
        c.getContext("2d").setTransform(dpr, 0, 0, dpr, 0, 0);
      }
    });

    const shownRate = () => Math.min(target, Math.round(refresh / 30) * 30 || 60);

    const updateNote = () => {
      rateText.textContent = `${target} frames each second`;
      if (target > refresh + 8) {
        note.hidden = false;
        note.textContent = `Your screen refreshes about ${Math.round(refresh)} times a second, so this preview can show at most ${Math.round(refresh / 30) * 30} FPS. Playing at ${target} FPS needs a ${target} Hz monitor.`;
      } else {
        note.hidden = true;
      }
    };

    const setTarget = (v) => {
      target = v;
      seg.forEach((b) => b.setAttribute("aria-checked", String(Number(b.dataset.target) === v)));
      updateNote();
      if (paused) drawAll();
    };
    seg.forEach((b) => b.addEventListener("click", () => setTarget(Number(b.dataset.target))));
    seg.forEach((b, i) => b.addEventListener("keydown", (e) => {
      if (e.key !== "ArrowLeft" && e.key !== "ArrowRight") return;
      const next = seg[(i + (e.key === "ArrowRight" ? 1 : seg.length - 1)) % seg.length];
      next.focus(); next.click(); e.preventDefault();
    }));

    // Scene: a road with lane marks and a car-like block crossing it. Position is a function of
    // time, sampled at the lane's frame times, so held frames are visible as judder.
    const SPEED = 330; // px per second across a 560 px lane
    const drawScene = (c, time) => {
      const ctx = c.getContext("2d");
      const W = c._w, H = c._h;
      ctx.clearRect(0, 0, W, H);
      // sky band and road
      const g = ctx.createLinearGradient(0, 0, 0, H);
      g.addColorStop(0, "#13171d"); g.addColorStop(1, "#0b0d10");
      ctx.fillStyle = g; ctx.fillRect(0, 0, W, H);
      // distant posts (slow parallax)
      ctx.fillStyle = "#1f252d";
      const farOff = (time * SPEED * 0.25) % 70;
      for (let x = -farOff; x < W + 70; x += 70) ctx.fillRect(x, 26, 4, 34);
      // road
      ctx.fillStyle = "#171b21"; ctx.fillRect(0, 64, W, 46);
      ctx.fillStyle = "#2b313a";
      const nearOff = (time * SPEED) % 64;
      for (let x = -nearOff; x < W + 64; x += 64) ctx.fillRect(x, 85, 30, 3);
      // the moving object: drives across, wraps around
      const span = W + 140;
      const x = ((time * SPEED * 0.55) % span) - 110;
      const y = 60;
      ctx.fillStyle = "#e9edf2";
      roundRect(ctx, x, y, 92, 30, 9); ctx.fill();
      ctx.fillStyle = "#0e1013";
      roundRect(ctx, x + 50, y + 5, 30, 11, 4); ctx.fill();
      ctx.fillStyle = "#0e1013";
      ctx.beginPath(); ctx.arc(x + 22, y + 31, 8, 0, Math.PI * 2); ctx.arc(x + 72, y + 31, 8, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = GREEN;
      ctx.fillRect(x + 4, y + 9, 4, 8);
    };

    const roundRect = (ctx, x, y, w, h, r) => {
      ctx.beginPath();
      ctx.moveTo(x + r, y); ctx.arcTo(x + w, y, x + w, y + h, r); ctx.arcTo(x + w, y + h, x, y + h, r);
      ctx.arcTo(x, y + h, x, y, r); ctx.arcTo(x, y, x + w, y, r); ctx.closePath();
    };

    // Tape: the last 0.4 s as one cell per frame, newest on the right, scrolling smoothly.
    const WINDOW = 0.4;
    const drawTape = (c, time, fps, kind) => {
      const ctx = c.getContext("2d");
      const W = c._w, H = c._h;
      ctx.clearRect(0, 0, W, H);
      const perReal = kind === "console" ? 1 : fps / 30;
      const frameDur = 1 / fps;
      const cellW = (W / (WINDOW / frameDur));
      const gap = Math.min(3, cellW * 0.22);
      const newest = Math.floor(time / frameDur);
      const frac = time / frameDur - newest;
      const count = Math.ceil(WINDOW / frameDur) + 2;
      for (let i = 0; i < count; i++) {
        const idx = newest - i;
        const xRight = W - (i + frac) * cellW;
        const real = (((idx % perReal) + perReal) % perReal) === 0;
        ctx.fillStyle = real ? REAL : GREEN;
        ctx.globalAlpha = i === 0 ? 1 : Math.max(0.25, 1 - (i * cellW) / W * 0.75);
        const x = xRight - cellW + gap / 2;
        ctx.fillRect(x, 2, Math.max(1, cellW - gap), H - 4);
      }
      ctx.globalAlpha = 1;
    };

    const drawAll = () => {
      for (const l of lanes) {
        const fps = l.kind === "console" ? 30 : shownRate();
        const t = Math.floor(simTime * fps) / fps;   // hold the picture until the lane's next frame
        drawScene(l.view, t);
        drawTape(l.tape, simTime, l.kind === "console" ? 30 : target, l.kind);
      }
    };

    const loop = (now) => {
      raf = requestAnimationFrame(loop);
      const dt = lastNow ? (now - lastNow) / 1000 : 0;
      lastNow = now;
      if (dt > 0 && dt < 0.1 && samples.length < 90) {
        samples.push(dt);
        if (samples.length === 90) {
          const sorted = samples.slice().sort((a, b) => a - b);
          refresh = 1 / sorted[45];
          if (!userPicked) setTarget(refresh >= 110 ? 120 : 60);
          updateNote();
        }
      }
      if (paused || !visible) return;
      simTime += Math.min(dt, 0.05);
      drawAll();
    };

    let userPicked = false;
    seg.forEach((b) => b.addEventListener("click", () => { userPicked = true; }));

    const setPaused = (p) => {
      paused = p;
      pauseBtn.textContent = p ? "Play" : "Pause";
      pauseBtn.setAttribute("aria-pressed", String(p));
    };
    pauseBtn.addEventListener("click", () => setPaused(!paused));
    setPaused(paused);

    if ("IntersectionObserver" in window) {
      new IntersectionObserver((entries) => { visible = entries[0].isIntersecting; }, { threshold: 0.05 }).observe(lab);
    }
    document.addEventListener("visibilitychange", () => { lastNow = 0; });

    simTime = 0.62;
    updateNote();
    drawAll();
    raf = requestAnimationFrame(loop);
  }

  // ---------------------------------------------------------------- feature tabs
  function startTabs() {
    const tabs = $$('.tabs [role="tab"]');
    const img = $("#show-img");
    const panel = $("#p-show");
    if (!tabs.length || !img) return;
    // Warm the cache so switching tabs doesn't flash.
    const warm = () => tabs.forEach((t) => { const i = new Image(); i.src = `assets/shots/${t.dataset.shot}.webp`; });
    if ("requestIdleCallback" in window) requestIdleCallback(warm); else setTimeout(warm, 1500);

    const select = (tab, focus) => {
      tabs.forEach((t) => { t.setAttribute("aria-selected", String(t === tab)); t.tabIndex = t === tab ? 0 : -1; });
      panel.setAttribute("aria-labelledby", tab.id);
      const src = `assets/shots/${tab.dataset.shot}.webp`;
      if (!img.src.endsWith(src)) {
        img.classList.add("swap");
        const next = new Image();
        next.onload = next.onerror = () => {
          img.src = src; img.alt = tab.dataset.alt || "";
          requestAnimationFrame(() => img.classList.remove("swap"));
        };
        next.src = src;
      }
      if (focus) tab.focus();
    };
    tabs.forEach((t, i) => {
      t.tabIndex = t.getAttribute("aria-selected") === "true" ? 0 : -1;
      t.addEventListener("click", () => select(t, false));
      t.addEventListener("keydown", (e) => {
        const keys = { ArrowDown: 1, ArrowRight: 1, ArrowUp: -1, ArrowLeft: -1 };
        if (e.key in keys) { select(tabs[(i + keys[e.key] + tabs.length) % tabs.length], true); e.preventDefault(); }
        else if (e.key === "Home") { select(tabs[0], true); e.preventDefault(); }
        else if (e.key === "End") { select(tabs[tabs.length - 1], true); e.preventDefault(); }
      });
    });
  }

  // ---------------------------------------------------------------- gallery lightbox
  function startGallery() {
    const dlg = $("#lightbox");
    if (!dlg || typeof dlg.showModal !== "function") return;
    const img = $("#lightbox-img"), cap = $("#lightbox-cap");
    $$(".shot").forEach((b) => b.addEventListener("click", () => {
      img.src = b.dataset.src; img.alt = b.dataset.cap; cap.textContent = b.dataset.cap;
      dlg.showModal();
    }));
    dlg.addEventListener("click", (e) => { if (e.target === dlg) dlg.close(); });
  }

  // ---------------------------------------------------------------- navigation state
  function startNav() {
    const nav = $(".nav");
    const onScroll = () => nav.classList.toggle("scrolled", window.scrollY > 8);
    window.addEventListener("scroll", onScroll, { passive: true });
    onScroll();
    const links = $$(".nav-links a");
    if (!("IntersectionObserver" in window)) return;
    const map = new Map(links.map((a) => [a.getAttribute("href").slice(1), a]));
    const io = new IntersectionObserver((entries) => {
      for (const e of entries) {
        if (!e.isIntersecting) continue;
        links.forEach((a) => a.classList.remove("active"));
        map.get(e.target.id)?.classList.add("active");
      }
    }, { rootMargin: "-45% 0px -50% 0px" });
    map.forEach((_, id) => { const s = document.getElementById(id); if (s) io.observe(s); });
  }

  // ---------------------------------------------------------------- latest release
  async function loadRelease() {
    try {
      const ctrl = new AbortController();
      const timer = setTimeout(() => ctrl.abort(), 6000);
      const res = await fetch("https://api.github.com/repos/DosukaSOL/PushFPS/releases/latest",
        { headers: { Accept: "application/vnd.github+json" }, signal: ctrl.signal });
      clearTimeout(timer);
      if (!res.ok) return;
      const rel = await res.json();
      const assets = rel.assets || [];
      const app = assets.find((a) => /^PushFPS-\d[^/]*-win64\.zip$/i.test(a.name));
      const obs = assets.find((a) => /^PushFPS-OBS-Plugin-.*\.zip$/i.test(a.name));
      const version = String(rel.tag_name || "").replace(/^v/, "");
      if (!/^\d+\.\d+\.\d+$/.test(version)) return;
      const date = rel.published_at ? new Date(rel.published_at).toLocaleDateString("en-GB", { day: "numeric", month: "long", year: "numeric" }) : "";
      if (app) {
        for (const id of ["hero-download", "dl-main"]) { const a = document.getElementById(id); if (a) a.href = app.browser_download_url; }
        const mb = Math.round(app.size / 1048576);
        $("#dl-version").textContent = `Version ${version}${date ? `, released ${date}` : ""}. ${mb} MB zip. Older versions and checksums are on the releases page.`;
        $("#release-line").textContent = `Version ${version}. Works with NVIDIA, AMD and Intel graphics.`;
      }
      if (obs) $("#dl-obs").href = obs.browser_download_url;
      const linux = assets.find((a) => /^PushFPS-[^/]*-linux-x86_64\.AppImage$/i.test(a.name));
      const mac = assets.find((a) => /^PushFPS-[^/]*-macos-universal\.zip$/i.test(a.name));
      if (linux) $("#dl-linux").href = linux.browser_download_url;
      if (mac) $("#dl-mac").href = mac.browser_download_url;
    } catch (_) { /* keep the releases page links */ }
  }

  const ready = (fn) => (document.readyState === "loading" ? document.addEventListener("DOMContentLoaded", fn) : fn());
  ready(() => { startThemes(); startSky(); startFlow(); startLab(); startTabs(); startGallery(); startNav(); loadRelease(); });
})();

// Setup guide: console tabs (arrow keys move between them).
(function () {
  var tabs = Array.prototype.slice.call(document.querySelectorAll(".ctabs [role=tab]"));
  if (!tabs.length) return;
  function pick(tab, focus) {
    tabs.forEach(function (t) {
      var on = t === tab;
      t.setAttribute("aria-selected", on ? "true" : "false");
      t.tabIndex = on ? 0 : -1;
      var panel = document.getElementById(t.getAttribute("aria-controls"));
      if (panel) panel.hidden = !on;
    });
    if (focus) tab.focus();
  }
  tabs.forEach(function (t, i) {
    t.addEventListener("click", function () { pick(t, false); });
    t.addEventListener("keydown", function (e) {
      var n = e.key === "ArrowRight" ? 1 : e.key === "ArrowLeft" ? -1 : 0;
      if (!n) return;
      e.preventDefault();
      pick(tabs[(i + n + tabs.length) % tabs.length], true);
    });
  });
})();
