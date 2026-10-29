#!/usr/bin/env python3
"""Make quoted source lines in a lesson page match the real C file exactly.

usage: fix-src.py page.html path/to/source.c [--check]

Finds every  [N, 'text']  literal (a source line for the code sheet) and rewrites text to
line N of the source file. With --check it only reports mismatches and exits 1 if any.
Out-of-range numbers are left alone, so use one source file per run."""
import re, sys
html, src = sys.argv[1], sys.argv[2]
check = '--check' in sys.argv
s = open(html).read()
lines = open(src).read().split('\n')
changed = 0
def fix(m):
    global changed
    n = int(m.group(1))
    if n < 1 or n > len(lines):
        return m.group(0)
    t = lines[n - 1].replace('\\', '\\\\').replace("'", "\\'")
    new = "[%d, '%s']" % (n, t)
    if new != m.group(0):
        changed += 1
        if check: print('MISMATCH line', n)
    return new
s2 = re.sub(r"\[(\d+), ?'((?:[^'\\]|\\.)*)'\]", fix, s)
if check:
    print(changed, 'mismatches'); sys.exit(1 if changed else 0)
open(html, 'w').write(s2); print('rewrote', changed, 'lines in', html)
