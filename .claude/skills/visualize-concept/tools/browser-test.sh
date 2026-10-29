#!/bin/bash
# usage: browser-test.sh /abs/path/page.html "js to run after load" out.png [width] [height]
# Needs Google Chrome (macOS path below). Console output of the js is printed; open out.png to look at the page.
# Pass "" as the js for a plain screenshot. To test dark mode run: "document.documentElement.setAttribute('data-theme','dark'); ..."
# Injects the js into a temp copy next to the page, screenshots it, prints console output.
PAGE="$1"; JS="$2"; OUT="$3"; DIR=$(dirname "$PAGE"); TMP="$DIR/_t_$(basename "$PAGE")"
python3 - "$PAGE" "$TMP" "$JS" <<'PY'
import sys
s=open(sys.argv[1]).read()
inj="<script>window.addEventListener('load',function(){setTimeout(function(){try{"+sys.argv[3]+"}catch(e){console.log('TESTERR '+e.message+' '+(e.stack||'').split('\\n')[1])}},50)})</script></body>"
open(sys.argv[2],'w').write(s.replace('</body>',inj))
PY
C="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
"$C" --headless=new --disable-gpu --hide-scrollbars --enable-logging=stderr --v=0 --virtual-time-budget=6000 --window-size=${4:-1280},${5:-1000} --screenshot="$OUT" "file://$TMP" 2>&1 | grep -E "CONSOLE|TESTERR|Uncaught" | grep -v -E "fonts.g|ERR_INTERNET|ERR_NAME" | sed 's/^.*CONSOLE[^]]*\]//' | head -30
rm -f "$TMP"
