/* Theme toggle. Fills <button class="tg" id="theme"> with its icons and switches data-theme on <html>.
   Light/dark follows the system until the viewer picks one; the choice is remembered per browser. */
(function () {
  var root = document.documentElement, btn = document.getElementById('theme');
  try { var s = localStorage.getItem('theme'); if (s) root.setAttribute('data-theme', s); } catch (e) {}
  if (!btn) return;
  btn.setAttribute('aria-label', 'Toggle light and dark mode');
  btn.title = 'Toggle theme';
  btn.innerHTML =
    '<svg class="sun" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>' +
    '<svg class="moon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M20 14.5A8 8 0 0 1 9.5 4 8 8 0 1 0 20 14.5z"/></svg>';
  btn.addEventListener('click', function () {
    var cur = root.getAttribute('data-theme') || (matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');
    var next = cur === 'dark' ? 'light' : 'dark';
    root.setAttribute('data-theme', next);
    try { localStorage.setItem('theme', next); } catch (e) {}
  });
})();
