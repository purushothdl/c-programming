/* Draggable divider for a grid whose last column is a side panel of width var(--side).
   Split.init({ grid, handle, key, def, min, rest, onChange }) -> { set(px), get() }
   Drag, arrow keys, or double-click (reset). The width is remembered per browser under `key`. */
(function () {
  function init(o) {
    var grid = o.grid, rz = o.handle, def = o.def || 320, min = o.min || 260, rest = o.rest || 420;
    var dragging = false;
    function get() { return parseInt(getComputedStyle(grid).getPropertyValue('--side'), 10) || def; }
    function set(px, save) {
      var max = Math.max(min, grid.clientWidth - 7 - rest);
      px = Math.max(min, Math.min(max, Math.round(px)));
      grid.style.setProperty('--side', px + 'px');
      if (save && o.key) { try { localStorage.setItem(o.key, String(px)); } catch (e) {} }
      return px;
    }
    function changed() { if (o.onChange) o.onChange(); }
    try { var saved = o.key && +localStorage.getItem(o.key); if (saved) set(saved); } catch (e) {}
    rz.addEventListener('pointerdown', function (e) { dragging = true; rz.setPointerCapture(e.pointerId); rz.classList.add('drag'); e.preventDefault(); });
    rz.addEventListener('pointermove', function (e) {
      if (!dragging) return;
      set(grid.getBoundingClientRect().right - e.clientX - 3); changed();
    });
    function end() { if (!dragging) return; dragging = false; rz.classList.remove('drag'); set(get(), true); changed(); }
    rz.addEventListener('pointerup', end); rz.addEventListener('pointercancel', end);
    rz.addEventListener('dblclick', function () { set(def, true); changed(); });
    rz.addEventListener('keydown', function (e) {
      if (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight') return;
      e.preventDefault(); e.stopPropagation();
      set(get() + (e.key === 'ArrowLeft' ? 24 : -24), true); changed();
    });
    addEventListener('resize', function () { set(get()); changed(); });
    return { set: set, get: get };
  }
  window.Split = { init: init };
})();
