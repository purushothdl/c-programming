# Lesson catalogue

Pick lessons that answer real questions about the program. Each entry gives when to use it, the data it needs, and where a working example lives.

## 1. Sequence (the centrepiece) · `shared/seq.js`

**Use when** hardware, an ISR and main cooperate, or two contexts share state (UART RX/TX, DMA complete, timer capture, EXTI). Also for a function stepper: swap lanes to caller / function / output.

```js
var ctl = Seq.mount(document.getElementById('seq'), {
  file: 'lib/uart.c',                 // shown above the code sheet
  key: 'uartTxSide',                  // localStorage key for the divider width
  lanes: [{ id, t, s }],              // columns: id, title, subtitle. 3 to 5 lanes read best
  src: [[lineNo, 'exact source text'], [0, ''] /* gap */, [-1, 'LABEL'] /* section header */],
  scenarios: [{ id, tab, bug?, init, steps, lanes?, src?, dim?, sheetNote? }],
  lines: { scenarioId: [[lineNos], ...] },   // optional; or set `lines` on each step
  derive: function (state) { return { 'IRQ line': state.TXEIE && state.TXE ? 1 : 0 }; }
});
ctl.setScenario(i, scenario)  // replace one scenario (input-driven pages), resets to step 0
ctl.select(i); ctl.goto(n);
```

**Step** (one arrow, or one self-action when `f === t`):

```js
{ f: 'isr', t: 'main', label: 'return',      // lanes; label under 24 chars
  kind: '' | 'irq' | 'ret' | 'good' | 'bad', // solid, ember, dashed, green, red
  title: 'ISR returns', desc: 'One or two short sentences.',
  code: 'one-line snippet',                  // shown when the Code sheet is off
  lines: [107],                              // source lines running now (real numbers)
  set: { main: 'running' },                  // Watch values after this step
  note: 'quiet ≥ 20 000', noteAt: 'isr',     // optional yellow note under the arrow
  verdict: 'good' | 'bad', verdictText: '...' } // optional, usually on the last step
```

**Scenarios to include:** the clean path; the common nuisance (bounce, noise, retry); the hardware edge case (overrun, overcapture, ring full); at least one **bug tab** (`bug: true`, with `dim` and `sheetNote` if it edits real lines). A bug tab uses a red tab underline.

**Watch** rows come from `init` plus `derive`. Use the real variable and register names. Add `copy_*` rows when main holds local copies.

## 2. Tuning lab (truth vs verdict) · `tim/capture.html` `#debounce`

**Use when** a magic number can be too small or too large (debounce window, timeout, filter length).
- Data: sample runs with a *truth* label per event; say they are sample data.
- Engine: replay the real algorithm per event, compare verdict with truth, count false positives and misses.
- UI: log slider, live value with unit, "use the code's value" button, a data-set switch that breaks the *other* direction, result table with a bar and threshold line, and the **safe range**.

## 3. Register builder · `tim/capture.html` `#registers`

**Use when** setup writes several fields of one register or a field has a decode table.
- Verify offsets and widths with the SVD, decode tables with the manual (note the page).
- UI: clickable bit cells (`.bits .row`, `.bit`), field labels (`.f-names`), the C-form hex output, decoded meaning (`.kv`), a "load the program's values" button, and rule warnings (`.warnbox`) such as write-locked fields.

## 4. Budget table and model · `uart/clock-drift.html`

**Use when** a number must stay inside a tolerance (baud error, clock accuracy, timing margin).
- A table of real options computed with the program's own formula; click a row; a slider for the other side's error; a bar for budget used.
- A simple model drawn in SVG with the sample points, plus pass/fail strips over the whole range. State what the model ignores and cross-check one limit against a manual table.

## 5. Calculators

Small `.card`s with one or two inputs and live output: tick and wrap time, wrap-safe subtraction with hex inputs, bytes-blocked time. Show the naive wrong answer next to the right one when that is the lesson. Take constants from the source (compute string lengths from the real strings).

## 6. Code stepper from input · `uart/cli-numbers.html`

**Use when** a pure function is the lesson (print, parse, tokenize, checksum). Embed the source file as `var FMT = [...]` (exact lines), write one generator per function that replays the algorithm and returns a Seq scenario, and call `ctl.setScenario(0, scenario)` when the input changes. Provide presets that hit each branch (empty, bad digit, overflow).

## 7. Init order and gotchas

Short lists. Init order: numbered, register names, one highlight only if the source gives a reason (put it in the item). Gotchas: 4 to 6 items, one bold line plus one sentence, taken from source comments and manual notes.

## Page skeleton

nav → hero (eyebrow with source path, headline with one ember phrase, Garamond sentence, spec table of key constants) → flow strip (hardware cells marked) → lessons as numbered sections → gotchas → footer with source lines and manual pages. `template.html` has it.
