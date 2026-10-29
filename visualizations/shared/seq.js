/* Sequence lesson engine: tabs of scenarios, a lane diagram, step controls, a Watch table and a
   code sheet that follows the real source lines.

   var ctl = Seq.mount(hostElement, {
     lanes: [{ id, t, s }],                 // columns: id, title, subtitle
     scenarios: [{ id, tab, bug?, init, steps, lanes?, src?, dim?, sheetNote? }],
     src: [[lineNo, text], ...],            // default source; [0,''] draws a gap
     lines: { scenarioId: [[lineNos], ...] }, // optional; or put `lines` on each step
     derive: function (state) { return { label: value }; }, // optional extra Watch rows
     file: 'main.c', key: 'storage-key'
   });
   Step: { f, t, label, kind: ''|'irq'|'ret'|'good'|'bad', title, desc, code, set, lines, note, noteAt, verdict, verdictText }
   ctl.setScenario(index, scenario) swaps one scenario (for input-driven pages); ctl.select(index). */
(function () {
  var NS = 'http://www.w3.org/2000/svg';
  function svgEl(name, attrs, text) {
    var e = document.createElementNS(NS, name);
    for (var k in attrs) e.setAttribute(k, attrs[k]);
    if (text != null) e.textContent = text;
    return e;
  }
  var fmt = function (n) { return n.toLocaleString('en-US'); };
  function showVal(v) { return (typeof v === 'number' || /^\d{4,}$/.test(String(v))) ? fmt(Number(v)) : v; }

  var TEMPLATE =
    '<div class="win"><div class="top" role="tablist"><div class="dots"><i></i><i></i><i></i></div></div>' +
    '<div class="seq">' +
      '<div class="diagram"><div class="dwrap"><svg class="main-svg" role="img" aria-label="Sequence diagram"></svg>' +
        '<div class="sp" aria-hidden="true"></div><svg class="foot-svg" aria-hidden="true"></svg></div></div>' +
      '<div class="rz" role="separator" aria-orientation="vertical" aria-label="Resize diagram and side panel" tabindex="0" title="Drag to resize · double-click to reset"></div>' +
      '<div class="panel">' +
        '<div class="ctl"><button class="btn sm" data-a="prev" aria-label="Previous step">←</button>' +
          '<button class="btn sm dark" data-a="next">Next →</button><button class="btn sm ghost" data-a="play">Play</button>' +
          '<button class="btn sm ghost" data-a="rst">Reset</button><span class="n"></span></div>' +
        '<div><div class="ptitle"></div><div class="pdesc"></div><div class="delta" aria-live="polite"></div></div>' +
        '<div class="pcode"></div><div class="sverdict"></div><div class="watch"></div>' +
        '<aside class="sheet" aria-label="Source code"><div class="sheet-h"><b class="sheet-file"></b><span>lit = running now · shaded = already ran</span></div>' +
          '<div class="sheet-note"></div><div class="sheet-body"></div></aside>' +
      '</div></div></div>';

  function mount(host, cfg) {
    host.innerHTML = TEMPLATE;
    var root = host.firstElementChild;
    var q = function (s) { return root.querySelector(s); };
    var tabs = q('.top'), grid = q('.seq'), svg = q('.main-svg'), foot = q('.foot-svg'), sp = q('.sp'), diagramEl = q('.diagram');
    var scenarios = cfg.scenarios.slice();
    var sc = scenarios[0], cur = 0, timer = null;
    var sheetOpen = innerWidth >= 900, watchOpen = true;
    var W = 760, X0 = 76, DX = 152, BOXW = 116, TOP = 62, ROW = 42, lanes = cfg.lanes, laneIdx = {};
    var msgEls = [], lnEls = {}, tabEls = [];
    q('.sheet-file').textContent = cfg.file || 'source';

    function srcOf() { return sc.src || cfg.src || []; }
    function stepLines(i) {
      var st = sc.steps[i];
      if (st.lines) return st.lines;
      var l = cfg.lines && cfg.lines[sc.id];
      return (l && l[i]) || [];
    }
    function laneX(id) { return X0 + laneIdx[id] * DX; }
    function stateAt(n) {
      var st = {}, k, i;
      for (k in sc.init) st[k] = sc.init[k];
      for (i = 0; i < n; i++) { var set = sc.steps[i].set; if (set) for (k in set) st[k] = set[k]; }
      if (cfg.derive) { var d = cfg.derive(st); for (k in d) st[k] = d[k]; }
      return st;
    }

    /* ----- tabs and toggles ----- */
    function buildTabs() {
      tabEls.forEach(function (t) { t.remove(); }); tabEls = [];
      var first = tabs.querySelector('.sheet-tg');
      scenarios.forEach(function (s, i) {
        var b = document.createElement('button');
        b.className = 'tab' + (s.bug ? ' bug' : ''); b.textContent = s.tab;
        b.setAttribute('role', 'tab'); b.setAttribute('aria-selected', s === sc ? 'true' : 'false');
        b.onclick = function () { select(i); };
        tabs.insertBefore(b, first || null); tabEls.push(b);
      });
    }
    function toggle(label, title, on, fn) {
      var b = document.createElement('button');
      b.className = 'sheet-tg' + (tabs.querySelector('.sheet-tg') ? '' : ' first'); b.textContent = label; b.title = title;
      b.setAttribute('aria-pressed', on ? 'true' : 'false');
      b.onclick = function () { fn(); b.setAttribute('aria-pressed', fn.state() ? 'true' : 'false'); };
      tabs.appendChild(b); return b;
    }
    var tgWatch = function () { watchOpen = !watchOpen; relayout(); }; tgWatch.state = function () { return watchOpen; };
    var tgCode = function () { sheetOpen = !sheetOpen; build(); update(true); }; tgCode.state = function () { return sheetOpen; };
    toggle('Watch', 'Show or hide register values (changed values stay as chips)', true, tgWatch);
    toggle('{ } Code', 'Show or hide the source', sheetOpen, tgCode);

    /* ----- layout and drawing ----- */
    function layout() {
      var dw = diagramEl.clientWidth || 740;
      W = Math.max(500, Math.min(760, Math.floor((dw - 12) / 20) * 20));
      BOXW = W < 640 ? 98 : 116; X0 = BOXW / 2 + 8;
      lanes = sc.lanes || cfg.lanes; laneIdx = {};
      lanes.forEach(function (l, i) { laneIdx[l.id] = i; });
      DX = lanes.length > 1 ? (W - 2 * X0) / (lanes.length - 1) : 0;
      grid.className = 'seq' + (sheetOpen ? ' open' : '') + (watchOpen ? '' : ' nowatch');
    }
    function relayout() { var prev = W; layout(); if (W !== prev) { build(); update(true); } }

    function buildSheet() {
      var body = q('.sheet-body'); body.textContent = ''; lnEls = {};
      var dim = sc.dim || [];
      srcOf().forEach(function (r) {
        if (r[0] === 0) { var gp = document.createElement('div'); gp.className = 'gap'; body.appendChild(gp); return; }
        if (r[0] === -1) { var fn = document.createElement('div'); fn.className = 'fn'; fn.textContent = r[1]; body.appendChild(fn); return; }
        var d = document.createElement('div'); d.className = 'ln' + (dim.indexOf(r[0]) >= 0 ? ' dim' : '');
        var n = document.createElement('i'); n.textContent = r[0];
        var t = document.createElement('span'); t.textContent = r[1];
        d.appendChild(n); d.appendChild(t); body.appendChild(d); lnEls[r[0]] = d;
      });
      var note = q('.sheet-note'); note.textContent = sc.sheetNote || ''; note.style.display = sc.sheetNote ? 'block' : 'none';
    }

    function build() {
      layout(); buildSheet();
      svg.textContent = ''; foot.textContent = ''; sp.textContent = ''; msgEls = [];
      var H = TOP + sc.steps.length * ROW + 24;
      svg.setAttribute('viewBox', '0 0 ' + W + ' ' + H);
      foot.setAttribute('viewBox', '0 0 ' + W + ' 46');
      lanes.forEach(function (l) {
        var x = laneX(l.id);
        svg.appendChild(svgEl('line', { x1: x, y1: 44, x2: x, y2: H, 'class': 'life' }));
        [[svg, 6], [foot, 6]].forEach(function (p) {
          if (p[0] === foot) p[0].appendChild(svgEl('line', { x1: x, y1: 0, x2: x, y2: 6, 'class': 'life' }));
          p[0].appendChild(svgEl('rect', { x: x - BOXW / 2, y: 6, width: BOXW, height: 38, rx: 4, 'class': 'lane-box' }));
          p[0].appendChild(svgEl('text', { x: x, y: 22, 'class': 'lane-t' }, l.t));
          p[0].appendChild(svgEl('text', { x: x, y: 36, 'class': 'lane-s' }, l.s));
        });
        var li = document.createElement('i'); li.style.left = (x / W * 100) + '%'; sp.appendChild(li);
      });
      sc.steps.forEach(function (s, i) {
        var y = TOP + i * ROW + 14;
        var g = svgEl('g', { 'class': 'm ' + (s.kind || '') + ' future' });
        var x1 = laneX(s.f), x2 = laneX(s.t);
        g.appendChild(svgEl('text', { x: 18, y: y + 4, 'class': 'stepno' }, String(i + 1)));
        if (s.f === s.t) {
          g.appendChild(svgEl('path', { d: 'M' + x1 + ' ' + (y - 8) + ' h30 v16 h-30', 'class': 'm-line' }));
          g.appendChild(svgEl('path', { d: 'M' + (x1 + 6) + ' ' + (y + 4) + ' l8 4 l0 -8 z', 'class': 'm-head' }));
          g.appendChild(svgEl('text', { x: x1 + 38, y: y + 4, 'class': 'm-txt' }, s.label));
        } else {
          var dir = x2 > x1 ? 1 : -1, xe = x2 - dir;
          g.appendChild(svgEl('line', { x1: x1, y1: y, x2: xe - dir * 7, y2: y, 'class': 'm-line' }));
          g.appendChild(svgEl('path', { d: 'M' + xe + ' ' + y + ' l' + (-dir * 9) + ' -4.5 l0 9 z', 'class': 'm-head' }));
          g.appendChild(svgEl('text', { x: (x1 + x2) / 2, y: y - 6, 'text-anchor': 'middle', 'class': 'm-txt' }, s.label));
        }
        g.appendChild(svgEl('rect', { x: 0, y: y - ROW / 2, width: W, height: ROW, 'class': 'hit' }));
        g.addEventListener('click', function () { stop(); cur = i + 1; update(); });
        svg.appendChild(g); msgEls.push(g);
        if (s.note) {
          var nx = laneX(s.noteAt), nw = Math.max(70, s.note.length * 6.2 + 16);
          var ng = svgEl('g', { 'class': 'm future', style: 'pointer-events:none' });
          ng.appendChild(svgEl('rect', { x: nx - nw / 2, y: y + 8, width: nw, height: 18, rx: 3, 'class': 'note-box' }));
          ng.appendChild(svgEl('text', { x: nx, y: y + 20.5, 'class': 'note-t' }, s.note));
          svg.appendChild(ng); g._note = ng;
        }
      });
    }

    function updateSheet() {
      if (!sheetOpen) return;
      var seen = {}, now = {}, i, dim = sc.dim || [];
      for (i = 0; i < cur - 1; i++) stepLines(i).forEach(function (l) { seen[l] = 1; });
      if (cur) stepLines(cur - 1).forEach(function (l) { now[l] = 1; });
      var first = null;
      for (var k in lnEls) {
        lnEls[k].className = 'ln' + (dim.indexOf(+k) >= 0 ? ' dim' : '') + (now[k] ? ' cur' : seen[k] ? ' seen' : '');
        if (now[k] && (first === null || +k < first)) first = +k;
      }
      var body = q('.sheet-body');
      if (first !== null) body.scrollTop = Math.max(0, lnEls[first].offsetTop - body.clientHeight / 3);
      else if (cur === 0) body.scrollTop = 0;
    }

    function update(quiet) {
      var n = sc.steps.length;
      msgEls.forEach(function (g, i) {
        var shown = i < cur;
        g.setAttribute('class', 'm ' + (sc.steps[i].kind || '') + (shown ? '' : ' future') + (i === cur - 1 ? ' cur' : ''));
        if (g._note) g._note.setAttribute('class', 'm' + (shown ? '' : ' future'));
      });
      q('.n').textContent = cur + ' / ' + n;
      q('[data-a=prev]').disabled = cur === 0; q('[data-a=next]').disabled = cur >= n;
      var s = cur ? sc.steps[cur - 1] : null;
      q('.ptitle').textContent = s ? s.title : 'Ready';
      q('.pdesc').textContent = s ? s.desc : 'Press Next to run the first step. Faded arrows are still to come.';
      q('.pcode').textContent = s ? s.code : '// waiting';
      var v = q('.sverdict'); v.className = 'sverdict';
      if (s && s.verdict) { v.className = 'sverdict ' + s.verdict; v.textContent = s.verdictText; }
      var st = stateAt(cur), before = stateAt(Math.max(cur - 1, 0)), k;
      var dl = q('.delta'); dl.textContent = '';
      if (cur > 0) for (k in st) if (String(st[k]) !== String(before[k])) {
        var chip = document.createElement('span'); chip.textContent = k + ' = ' + showVal(st[k]); dl.appendChild(chip);
      }
      var w = q('.watch'); w.textContent = '';
      var h = document.createElement('h5'); h.textContent = 'Watch'; w.appendChild(h);
      for (k in st) {
        var r = document.createElement('div');
        r.className = 'r' + (cur > 0 && quiet !== true && String(st[k]) !== String(before[k]) ? ' chg' : '');
        var a = document.createElement('span'); a.textContent = k;
        var b = document.createElement('span'); b.textContent = showVal(st[k]);
        r.appendChild(a); r.appendChild(b); w.appendChild(r);
      }
      updateSheet();
      if (cur >= n) stop();
    }

    function stop() { if (timer) { clearInterval(timer); timer = null; } q('[data-a=play]').textContent = 'Play'; }
    function select(i) {
      stop(); sc = scenarios[i]; cur = 0;
      tabEls.forEach(function (t, j) { t.setAttribute('aria-selected', j === i ? 'true' : 'false'); });
      build(); update(true);
    }
    q('[data-a=next]').onclick = function () { stop(); if (cur < sc.steps.length) { cur++; update(); } };
    q('[data-a=prev]').onclick = function () { stop(); if (cur > 0) { cur--; update(); } };
    q('[data-a=rst]').onclick = function () { stop(); cur = 0; update(); };
    q('[data-a=play]').onclick = function () {
      if (timer) { stop(); return; }
      if (cur >= sc.steps.length) cur = 0;
      q('[data-a=play]').textContent = 'Pause'; cur++; update();
      timer = setInterval(function () { if (cur < sc.steps.length) { cur++; update(); } else stop(); }, 1500);
    };
    document.addEventListener('keydown', function (e) {
      var t = e.target.tagName; if (t === 'INPUT' || t === 'SELECT' || t === 'TEXTAREA') return;
      var r = root.getBoundingClientRect();
      if (r.bottom < 0 || r.top > innerHeight) return;
      if (e.key === 'ArrowRight') q('[data-a=next]').click();
      if (e.key === 'ArrowLeft') q('[data-a=prev]').click();
    });

    Split.init({ grid: grid, handle: q('.rz'), key: cfg.key || 'seqSide', def: 320, min: 260, rest: 420, onChange: relayout });
    buildTabs(); build(); update(true);

    return {
      select: select,
      setScenario: function (i, s) {
        var wasCur = scenarios[i] === sc;
        scenarios[i] = s; buildTabs();
        if (wasCur) { sc = s; cur = 0; stop(); build(); update(true); }
      },
      goto: function (n) { cur = Math.max(0, Math.min(sc.steps.length, n)); update(true); }
    };
  }
  window.Seq = { mount: mount };
})();
