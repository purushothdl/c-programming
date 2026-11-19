#!/usr/bin/env python3
"""Make quoted source lines in a lesson page match the real C file, and catch wrong line NUMBERS.

usage: fix-src.py page.html path/to/source.c [--check]

Finds every  [N, 'text']  literal (a source line for the code sheet).
  text check   text must equal line N of the file. Fix mode rewrites it, --check reports it.
  number check if the quoted text is not line N but appears at exactly one other line M, the number
               is probably stale (the file changed). Reported as WARN and never auto-moved, because
               rewriting the text under a wrong number would hide the bug. Fix the number by hand.
Exit 1 in --check mode if anything mismatches or warns. Out-of-range numbers are reported.
Gap [0, ''] and label [-1, 'X'] entries are skipped. Use one source file per run.
Still your job: check that each highlighted line holds the identifier the step is about."""
import re, sys
html, src = sys.argv[1], sys.argv[2]
check = '--check' in sys.argv
s = open(html).read()
lines = [l.rstrip() for l in open(src).read().split('\n')]
changed = warns = 0
def unesc(t): return t.replace("\\'", "'").replace('\\\\', '\\')
def fix(m):
    global changed, warns
    n = int(m.group(1))
    if n < 1: return m.group(0)
    if n > len(lines):
        warns += 1; print('WARN line %d is past the end of %s (%d lines)' % (n, src, len(lines))); return m.group(0)
    old = unesc(m.group(2)).rstrip()
    if old and old != lines[n - 1]:
        at = [i + 1 for i, l in enumerate(lines) if l == old]
        if len(at) == 1:
            warns += 1; print('WARN line %d: this text is at line %d in %s, so the number is stale' % (n, at[0], src))
    t = lines[n - 1].replace('\\', '\\\\').replace("'", "\\'")
    new = "[%d, '%s']" % (n, t)
    if new != m.group(0):
        changed += 1
        if check: print('MISMATCH line', n)
    return new
s2 = re.sub(r"\[(\d+), ?'((?:[^'\\]|\\.)*)'\]", fix, s)
if check:
    print(changed, 'mismatches,', warns, 'warnings'); sys.exit(1 if changed or warns else 0)
open(html, 'w').write(s2); print('rewrote', changed, 'lines in', html, '·', warns, 'warnings (stale numbers are not auto-fixed)')
