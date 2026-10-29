# Design system

Cursor's parchment-and-ink look with OpenAI's restraint. Sharp, flat, quiet. All tokens and components live in `visualizations/shared/style.css`; pages link it and add only page-specific CSS in a small `<style>` block. Name page-specific classes so they do not clash with shared ones (shared uses `.lab`, `.bits`, `.bit`, `.seg`, `.chip`, `.card`, `.verdict-line`, `.sverdict`, `.bartrack`, `.ln`, `.sheet`, `.watch`).

## Tokens

| Token | Light | Dark | Use |
|---|---|---|---|
| `--bg` | `#fbfaf7` | `#0d0e11` | page |
| `--card` | `#ffffff` | `#15171c` | cards, window, inputs |
| `--card-2` | `#f5f3ee` | `#1c1f26` | tab bar, code, headers |
| `--line` / `--line-2` | `rgba(38,37,30,.15/.08)` | `rgba(255,255,255,.11/.06)` | borders, hairlines |
| `--ink` / `-2` / `-3` | `#1c1b16` / `#55544c` / `#8a8980` | `#eceef2` / `#a9aebb` / `#6b7080` | text, muted, faint |
| `--ember` | `#cf4300` | `#ff8650` | accent: **text, active step, thin rules only** |
| `--ok` / `--bad` / `--warn` | `#1b7a55` / `#cf2d56` / `#946310` | `#4cc690` / `#ff7d6e` / `#f0b04a` | meaning only: accepted, rejected, caution |
| `--btn` / `--btn-ink` | `#1c1b16` / `#fbfaf7` | `#eceef2` / `#0d0e11` | the one filled button |

**Adding a theme colour:** change only `--ember` (and its dark twin) per topic, for example one accent per peripheral. Keep `--ok`, `--bad`, `--warn` for meaning. Check text contrast of the accent on `--bg` is at least 4.5:1 in both modes.

Theme toggle: `data-theme` on `<html>`, system preference by default, dark values defined twice (media query guarded by `:root:not([data-theme="light"])`, and `:root[data-theme="dark"]`).

## Type

- Inter 400/500 for UI. Headlines weight 400 with tight tracking (`-.03em` at 52px). Never bold headlines.
- EB Garamond for the one intro paragraph only.
- JetBrains Mono for code, registers, numbers in tables, metadata. Use `font-feature-settings: "tnum"` on the body.
- Sizes: 14px body, 13px controls, 12px widgets and tables, 11px captions, 10px field labels.

## Shape

- 4px radius on everything. The window frame may use 8px.
- Hairline borders, no shadows except the single window frame shadow.
- One filled button (black) per action group, others are linen or ghost.
- Ember is never a fill, except a 2px active-tab underline and highlighted-line tint at 14%.

## Page layout

1. Sticky 48px nav: name, anchor links, theme toggle (30px square).
2. Hero: eyebrow (mono path), headline with one ember phrase, Garamond sentence, two buttons, and a spec table of the key constants.
3. Flow strip: the path of one event in 5 to 7 cells, hardware cells marked.
4. Sections: mono eyebrow `01 · topic`, h2, one-line description, then a lesson.
5. Footer: source path, manual chapter.

## The window lesson frame

Tab bar on top (file-tab style, mono 12px, ember underline on the active tab, red underline on a bug tab), toggles `Watch` and `{ } Code` on the right. Below: two columns with a draggable divider.

- **Left:** diagram (sequence SVG with mirrored lane boxes top and bottom; a spacer keeps lifelines running when the right column is taller).
- **Right (docked, never an overlay):** controls (←, Next, Play, Reset, counter) → step title and description (with changed-value chips when Watch is off) → one-line snippet (hidden when Code is on) → Watch table → Code sheet.
- Divider: drag, arrow keys, double-click resets to 320px, width saved per viewer. Min 260px, diagram keeps at least 420px. Under 900px everything stacks.

## Interaction patterns

- Steppers: Next/Prev/Play/Reset, click any arrow to jump, left/right keys while the section is on screen.
- Sliders on a log scale when values span orders of magnitude; show the number and unit live; add a "use the code's value" button.
- Verdict chips: green correct, red wrong, grey harmless. Always show *truth* beside the *result*.
- Changed values flash once (`flash` keyframes). Honour `prefers-reduced-motion`.
- Inputs and bits are keyboard-reachable (`tabindex`, Enter/Space on bits).
