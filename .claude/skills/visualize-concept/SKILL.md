---
name: visualize-concept
description: Build an interactive HTML lesson page for a firmware program, peripheral or concept in this repo (event sequences between hardware, ISR and main; register bit builders; parameter-tuning labs; calculators; code steppers). Use when Purushoth asks to visualize, "make a page", "big pictures little words", or understand an interrupt/event flow, timing, a function or a register setup by stepping through it. Every number on the page is checked against the SVD, the manuals and the C source first.
---

# Visualize a concept

Output: one HTML page, `visualizations/<topic>/<name>.html` (`uart/`, `tim/`, `spi/`, ...), light and dark mode. It is a **lesson, not a poster**: the reader steps, drags and clicks, and the page shows what changes. Add the page to `visualizations/index.html`.

Pages share four files in `visualizations/shared/`. **Do not copy their code into a page.**

| File | Gives you |
|---|---|
| `style.css` | tokens, nav, hero, flow strip, window frame, labs, tables, bit builder, sequence and code-sheet styles |
| `theme.js` | light/dark toggle (`<button class="tg" id="theme"></button>`) |
| `split.js` | draggable divider, `Split.init(...)` |
| `seq.js` | the sequence lesson: `Seq.mount(el, { lanes, scenarios, src, ... })`. API in `lessons.md` |

Start from `template.html` in this folder. Working examples: `tim/capture.html` (sequence, tuning lab, register builder, calculators), `uart/cli-tx.html` (sequence with bug tabs and a derived Watch row), `uart/cli-numbers.html` (generated steps from user input), `uart/clock-drift.html` (budget table and a model with strips).

## Principles

- **Big pictures, little words.** Diagrams and live widgets carry the idea. Prose is short, plain, no jargon, no odd metaphors. Follow `explain-hardware`: picture first, names second.
- **Practical over pretty.** Each widget answers "what happens if I change X?" or "who does what next?". If it only decorates, cut it.
- **Correct before clever.** Widgets compute from real values. Wrong numbers in an interactive page teach the wrong thing faster than a wrong paragraph.
- **Small and sharp.** 14px text, 12px widgets, 4px corners, hairline borders. See `design-system.md`.

## Workflow

1. **Read the source** (`programs/<name>/…`, `lib/*.c`). Note constants, ISR logic, init order, and comments that give reasons. Line numbers move: always re-read the file now, never reuse numbers from an old page.
2. **Verify facts first**, before writing widget data:
   - Register names, addresses, bit positions: SVD (absolute path `…/c-programming/svd/stm32f446re.svd`, device name `STM32F446`). Never write a bit position from memory.
   - Tables and sequences (filter tables, enable order, flag-clear rules, tolerance tables): RM0390 or PM0214 via `pdf-mcp`, starting at `docs/contents.md`. Keep the page number and cite it on the page.
   - Derived numbers (tick, wrap time, BRR, byte time): compute them and compare with the source comments.
3. **Pick 3 or 4 lessons** from `lessons.md`. Typical: a **sequence** (centrepiece), a **tuning lab** or **model** for the magic number, a **register builder**, one or two **calculators**, then short **init order** and **gotchas** lists.
4. **Build** from `template.html`. Put source lines in the page as `[N, 'text']` literals, then run `tools/fix-src.py page.html source.c` so the text is exact. For input-driven steps (a function stepper), embed the source file as an array and slice it by line number.
5. **Replay the real algorithm in JS** for anything with logic (ISR, parser, debounce). Never hand-write verdicts or intermediate values. The sample run must reproduce the numbers in the source comments.
6. **Test in the browser.** `tools/browser-test.sh /abs/page.html "js" out.png` runs Chrome headless, clicks through the page with your JS, prints console output and saves a screenshot. Check: no console errors, the highlighted source lines are the ones you meant, both themes, a narrow width, and the interactions you added. Read the PNG.
7. **Report**: what was verified and where (SVD fields, manual pages), what is sample or illustrative data, what is a simplified model, and what was not verified.

## Rules that came from review

- Light mode must not look dim: near-white canvas, white cards, dark ink.
- The Code sheet shows **real source lines with real line numbers**, skipping blank and comment-only lines, and keeps trailing comments. Highlight follows true control flow, including the path back into the loop after an ISR returns and tail-chained interrupts. For a bug variant that is not in the source, use `dim` (struck lines) and `sheetNote`.
- Include at least one **"what goes wrong" scenario** next to the correct one (torn read, arm before push, never disarm). Failures teach more than the happy path.
- **Do not model hardware timing from memory.** If an ordering depends on a flag timing (TXE rising, TC), read the manual text and say what you relied on. Mark simplified models on the page.
- If the program's clock or a constant is a choice (HSI vs crystal), say which one the program uses.
- Watch can be hidden; changed values then stay as chips under the step description. Keep the side column docked, never an overlay.
- If you highlight one item in a list, say why in the item.
- `localStorage` only for per-viewer convenience (theme, divider width), always in try/catch.
- A page may be wrong in places the author cannot see. Prefer fewer claims, each backed by the source or the manual.

## Do not

- Do not invent register values, addresses or manual quotes.
- Do not add gradients, glows, pills, three-colour themes or decorative illustrations.
- Do not say the page was seen working unless a browser test ran. Say what the test covered.
- Do not write an essay inside the page. If a paragraph runs past three lines, turn it into a widget or cut it.
