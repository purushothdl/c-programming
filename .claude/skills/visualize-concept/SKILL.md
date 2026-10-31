---
name: visualize-concept
description: Build an interactive HTML lesson page for a firmware program, a peripheral, or any systems concept (OS scheduling, runtimes, Rust or C behaviour, protocols, system design): event sequences between actors, register bit builders, parameter-tuning labs, calculators, code steppers. Use when Purushoth asks to visualize, "make a page", "big pictures little words", or understand an event flow, timing, a function or a mechanism by stepping through it. Every claim on the page is checked against its authority first (SVD and manuals for hardware, source or docs plus a real run for software).
---

# Visualize a concept

Output: one HTML page, `visualizations/<topic>/<name>.html` (`uart/`, `tim/`, `spi/`, ...), light and dark mode. It is a **lesson, not a poster**: the reader steps, drags and clicks, and the page shows what changes. Add the page to `visualizations/index.html`.

Pages share these files in `visualizations/shared/` and `visualizations/themes/`. **Do not copy their code into a page.**

| File | Gives you |
|---|---|
| `style.css` | tokens, nav, hero, flow strip, window frame, labs, tables, bit builder, sequence and code-sheet styles |
| `theme.js` | light/dark toggle (`<button class="tg" id="theme"></button>`) |
| `themes.js` | design switch in the nav (Original, Claude, Notion, ...) and its loader. Each design is one CSS file in `themes/<id>.css` |
| `split.js` | draggable divider, `Split.init(...)` |
| `seq.js` | the sequence lesson: `Seq.mount(el, { lanes, scenarios, src, ... })`. API in `lessons.md` |

Start from `template.html` in this folder (full page), or `template-minimal.html` (one sequence, no Watch, no Code) when the concept needs a single picture. The skeleton pieces (hero, flow strip, lessons, gotchas) are optional and can come in any order; keep only the ones the lesson needs. Working examples: `tim/capture.html` (sequence, tuning lab, register builder, calculators), `uart/cli-tx.html` (sequence with bug tabs and a derived Watch row), `uart/cli-numbers.html` (generated steps from user input), `uart/clock-drift.html` (budget table and a model with strips), `tim/channels.html` (a gate pipeline with a live analyser, no sequence), `spi/explorer.html` (a stepper in the window frame), `os/go-scheduler.html` (software concept: a sequence with Watch but no code, and a second sequence with neither).

**Reading budget.** For one page read `SKILL.md`, `template.html` and the one example closest to your lesson. Open `lessons.md` (the `seq.js` gotchas are in it), `design-system.md`, `style.css` and `seq.js` only when you need them. The first run of a checker or the browser test will tell you what you got wrong.

## Sources by topic

Say on the page where each claim was checked. Pick the authority for the topic:

| Topic | Authority | Check |
|---|---|---|
| STM32 peripherals | SVD, RM0390 (`pdf-mcp`) | bit positions from the SVD, tables with page numbers |
| Cortex-M core | PM0214 | as above |
| OS and kernel behaviour | the man page, kernel or POSIX docs | a small program, `strace`/`dtruss` or `perf` output for anything about behaviour |
| Runtimes (Go, others) | the runtime source in the installed toolchain (`$(go env GOROOT)/src/runtime`) | quote function names and line numbers *with the version*, then run a small program to confirm the behaviour and get numbers |
| Rust | the Rust reference, std docs | the compiler: real error text, `rustc --emit asm` for codegen claims |
| C | the C standard, compiler docs | compile and run, `-S` for assembly |
| System design | no single authority | label every number as an assumption and show its formula |

Numbers you measured are *sample data*: give the machine, the tool version, and say it is one run. Never present a recalled number as measured.

## Principles

- **Big pictures, little words.** Diagrams and live widgets carry the idea. Prose is short, plain, no jargon, no odd metaphors. Follow `explain-hardware`: picture first, names second.
- **Practical over pretty.** Each widget answers "what happens if I change X?" or "who does what next?". If it only decorates, cut it.
- **Correct before clever.** Widgets compute from real values. Wrong numbers in an interactive page teach the wrong thing faster than a wrong paragraph.
- **Small and sharp.** 14px text, 12px widgets, 4px corners, hairline borders. See `design-system.md`.

## The brief decides the layout

The templates are defaults, not requirements. Before building:
- **Detailed brief** (the user says what to see, touch or leave out): follow it. Design the page around it, reuse shared pieces where they fit, and write page-specific widgets where they do not. Do not add a hero, flow strip, Watch or Code sheet that was not asked for.
- **Vague brief** ("visualize X"): pick the shape yourself from `lessons.md` ("Other shapes" too), and say in one line which shape you chose and why. Do not ask unless the answer changes what you verify.
- **Always, whatever the layout:** the theme, claims checked against their authority, measured numbers labelled as one run, a "what goes wrong" case when the idea allows one, a browser test, and a note of what was simplified.

## Workflow

1. **Read the source** (`programs/<name>/…`, `lib/*.c`). Note constants, ISR logic, init order, and comments that give reasons. Line numbers move: always re-read the file now, never reuse numbers from an old page. Get numbers with `grep -n`, not from memory.
2. **Verify facts first**, before writing widget data:
   - Register names, addresses, bit positions: SVD (absolute path `…/c-programming/svd/stm32f446re.svd`, device name `STM32F446`). Never write a bit position from memory.
   - Tables and sequences (filter tables, enable order, flag-clear rules, tolerance tables): RM0390 or PM0214 via `pdf-mcp`, starting at `docs/contents.md`. Keep the page number and cite it on the page.
   - Derived numbers (tick, wrap time, BRR, byte time): compute them and compare with the source comments.
3. **Pick 3 or 4 lessons** from `lessons.md`. Typical: a **sequence** (centrepiece), a **tuning lab** or **model** for the magic number, a **register builder**, one or two **calculators**, then short **init order** and **gotchas** lists.
4. **Build** from `template.html`. Keep the `<script src="../shared/themes.js"></script>` line just before `</head>` (after the page's own `<style>`), so the design switch works and theme rules win. Do not put colours, fonts or radii in the page's `<style>`: use the tokens (`--bg`, `--card`, `--ink`, `--ember`, `--line`, `--btn`, ...). If a new class needs a per-design look, add a rule to each `themes/*.css`. Put source lines in the page as `[N, 'text']` literals, then run `tools/fix-src.py page.html source.c --check`. It fixes wrong text (drop `--check`) and **warns when a number is stale** (the text sits at another line). It cannot know a number is the *right* one for the step: after it passes, confirm each highlighted line contains the identifier the step is about. For input-driven steps (a function stepper), embed the source file as an array and slice it by line number. Seq prints console warnings for highlights that are not in `src`; treat any as a bug.
5. **Replay the real algorithm in JS** for anything with logic (ISR, parser, debounce). Never hand-write verdicts or intermediate values. The sample run must reproduce the numbers in the source comments. When one page shows the same event in two places (a sequence and a lab), make both read from one model; check their **phase** too (does the first arrival start at t = 0 or one byte-time in?) and compare the two outputs.
6. **Test in the browser.** `tools/browser-test.sh /abs/page.html "js" out.png [--mobile] [--at=#id]` runs Chrome headless, runs your JS, prints console output and saves a screenshot. `--mobile` renders at 390px; `--at=#id` crops to a section (headless always captures from the top). Turn checks into console assertions (`console.log('WATCH', ...)`, the highlighted `.ln.cur` line numbers, verdict text) rather than only reading the PNG. Check: no console errors or Seq warnings, the highlighted source lines are the ones you meant, every design in the switch (click each button in `.seg`, wait about 500 ms for the stylesheet to load, then read the background), light and dark, 390px, and each interaction you added. Then read the PNG, at least once in a non-Original design.
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
