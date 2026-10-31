/* Design themes. Each theme is one stylesheet in visualizations/themes/<id>.css that overrides shared tokens.
   To add a theme: drop themes/<id>.css and add one line to THEMES below.
   Pick with ?theme=<id>, the switch in the nav, or the choice remembered in this browser.
   Load it in <head> after the page's own <style>, so theme rules win over page layout. */
(function () {
  var THEMES = [
    { id: 'original', label: 'Original' },
    { id: 'claude', label: 'Claude', fonts: 'https://fonts.googleapis.com/css2?family=Source+Serif+4:opsz,wght@8..60,400;8..60,500&family=Inter:wght@400;500;580;600&display=swap' },
    { id: 'notion', label: 'Notion', fonts: 'https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=Source+Serif+4:opsz,wght@8..60,400&display=swap' }
  ];
  var script = document.currentScript;
  var base = script ? script.src.replace(/[^\/]*$/, '') : '../shared/';

  function find(id) {
    return THEMES.filter(function (t) { return t.id === id; })[0] || THEMES[0];
  }
  function current() {
    var id = null;
    try { id = new URLSearchParams(location.search).get('theme'); } catch (e) {}
    try { id = id || localStorage.getItem('design'); } catch (e) {}
    return find(id);
  }
  function drop(id) {
    var el = document.getElementById(id);
    if (el) el.parentNode.removeChild(el);
  }
  function apply(t) {
    document.documentElement.setAttribute('data-design', t.id);
    drop('design-css');
    drop('design-fonts');
    if (t.id === 'original') return;
    var css = document.createElement('link');
    css.id = 'design-css'; css.rel = 'stylesheet'; css.href = base + '../themes/' + t.id + '.css';
    document.head.appendChild(css);
    if (t.fonts) {
      var f = document.createElement('link');
      f.id = 'design-fonts'; f.rel = 'stylesheet'; f.href = t.fonts;
      document.head.appendChild(f);
    }
  }

  var now = current();
  apply(now);

  document.addEventListener('DOMContentLoaded', function () {
    var tg = document.getElementById('theme');
    if (!tg) return;
    var seg = document.createElement('div');
    seg.className = 'seg';
    seg.setAttribute('role', 'group');
    seg.setAttribute('aria-label', 'Design theme');
    seg.title = 'Design theme';
    THEMES.forEach(function (t) {
      var b = document.createElement('button');
      b.type = 'button';
      b.textContent = t.label;
      b.setAttribute('aria-pressed', t.id === now.id ? 'true' : 'false');
      b.onclick = function () {
        try { localStorage.setItem('design', t.id); } catch (e) {}
        apply(t);
        Array.prototype.forEach.call(seg.querySelectorAll('button'), function (x) {
          x.setAttribute('aria-pressed', x === b ? 'true' : 'false');
        });
      };
      seg.appendChild(b);
    });
    tg.parentNode.insertBefore(seg, tg);
  });
})();
