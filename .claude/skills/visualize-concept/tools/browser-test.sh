#!/bin/bash
# usage: browser-test.sh /abs/page.html "js to run after load" out.png [width] [height] [--mobile] [--at=#id]
# Runs Chrome headless on a temp copy of the page, runs your js after load, prints console output, saves a screenshot.
# Finds Chrome, Chromium or Edge on macOS and Linux by itself; set CHROME=/path/to/browser to override. Pass "" as the js for a plain screenshot.
#   --mobile   renders the page at 390px wide inside an iframe (Chrome headless will not go below ~500px otherwise)
#   --at=#id   crops the screenshot to start at that element (headless always captures from the top, scrollIntoView does nothing)
# Dark mode: js "document.documentElement.setAttribute('data-theme','dark'); ..."
# Assert instead of squinting: console.log('WATCH', document.querySelector('.watch').innerText) and compare in your head or with grep.
PAGE="$1"; JS="$2"; OUT="$3"; W=1280; H=1000; MOBILE=0; AT=""
shift 3
for a in "$@"; do case "$a" in --mobile) MOBILE=1;; --at=*) AT="${a#--at=}";; [0-9]*) if [ "$W" = 1280 ] && [ -z "$GOTW" ]; then W="$a"; GOTW=1; else H="$a"; fi;; esac; done
DIR=$(dirname "$PAGE"); TMP="$DIR/_t_$(basename "$PAGE")"; HARNESS="$DIR/_t_harness.html"
python3 - "$PAGE" "$TMP" "$JS" "$AT" <<'PY'
import sys
s=open(sys.argv[1]).read(); js=sys.argv[3]; at=sys.argv[4]
crop=""
if at:
    crop="var e=document.querySelector('%s');if(e){var y=e.getBoundingClientRect().top+scrollY-60;document.documentElement.style.overflow='hidden';document.body.style.transform='translateY(-'+y+'px)';}"%at.replace("'","\\'")
inj="<script>window.addEventListener('load',function(){setTimeout(function(){try{"+js+";"+crop+"}catch(e){console.log('TESTERR '+e.message+' '+(e.stack||'').split('\\n')[1])}},50)})</script></body>"
open(sys.argv[2],'w').write(s.replace('</body>',inj))
PY
C="${CHROME:-}"
if [ -z "$C" ]; then
  for c in "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" "/Applications/Chromium.app/Contents/MacOS/Chromium" "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge" google-chrome google-chrome-stable chromium chromium-browser chrome; do
    if [ -x "$c" ] || command -v "$c" >/dev/null 2>&1; then C="$c"; break; fi
  done
fi
if [ -z "$C" ] || ! { [ -x "$C" ] || command -v "$C" >/dev/null 2>&1; }; then echo "browser-test.sh: no usable Chrome or Chromium (tried: ${C:-none}). Install one or set CHROME=/path/to/chrome" >&2; rm -f "$TMP"; exit 2; fi
URL="file://$TMP"
if [ "$MOBILE" = 1 ]; then
  [ "$H" -lt 1400 ] && H=1400
  echo "<body style=\"margin:0\"><iframe src=\"file://$TMP\" style=\"width:390px;height:${H}px;border:0\"></iframe></body>" > "$HARNESS"
  URL="file://$HARNESS"; W=420
fi
"$C" --headless=new --disable-gpu --hide-scrollbars --enable-logging=stderr --v=0 --virtual-time-budget=6000 --window-size=$W,$H --screenshot="$OUT" "$URL" 2>&1 | grep -E "CONSOLE|TESTERR|Uncaught" | grep -v -E "fonts.g|ERR_INTERNET|ERR_NAME" | sed 's/^.*CONSOLE[^]]*\]//' | head -30
rm -f "$TMP" "$HARNESS"
