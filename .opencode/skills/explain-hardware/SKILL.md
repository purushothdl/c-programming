---
name: explain-hardware
description: Explain a register, peripheral or hardware behaviour (timers, GPIO, UART, SPI, clocks, NVIC) to Purushoth in plain concrete terms before any terminology. Use whenever they ask "explain", "what does X do", "why do we need Y" or say a previous explanation was confusing or too abstract, for any STM32F446RE register or code in this repo.
---

# Explain hardware

Purushoth is learning bare-metal STM32F446RE. Past explanations failed when they opened with field names, tables and abstract wording ("the load turns on when the pin goes low"). They worked when they opened with a physical picture, then attached names to it.

## Order of every explanation

1. **Answer the question asked.** If their understanding is partly wrong, say which part is right, then fix the wrong part first. Name any mix-up directly (for example "CCMR1 is shared, CCR1 and CCR2 are separate").
2. **Start from a physical picture.** Use a real thing they can imagine: an LED wired two ways, a stopwatch that wraps, a switch box on a wire. Draw the circuit or the timeline in a small text diagram. Never start with register names.
3. **Use real numbers.** Show ticks 0..19, CCR = 5, 25% duty, the actual pin voltage. One worked case beats a general rule.
4. **Then attach the names.** Introduce each register or field only after the picture needs it. Say where it lives (which register, which bits) and what question it answers.
5. **Separate what is shared from what is per channel.** Use a short table when several registers look alike.
6. **End with one summary table or the order of steps,** then offer the next step. Do not add extra topics.

## Writing rules (about 80% of ASD-STE100)

- Short sentences. One idea per sentence.
- Active voice. Name who does the action: "the hardware copies CNT into CCR1".
- One word for one thing. Do not rename a concept halfway through.
- No stacked abstractions. If a sentence needs "in general" or "conceptually", replace it with an example.
- Tables only for comparisons. Explain in prose and diagrams first.

## Facts

- Check register and bit details with the SVD (`svd/stm32f446re.svd`, absolute path) and RM0390 through `pdf-mcp`, as AGENTS.md says. Give the page or section.
- Mark anything stated from memory, so Purushoth knows to check it.
- Core peripherals come from PM0214, ST peripherals from RM0390.

## Visualizations

- Build one only when asked, as a single HTML file in `visualizations/`, matching the existing files.
- Show one path or one idea at a time. Avoid repeating the same row four times.
- Let the reader change one input and see where the signal changes.

## Worked example: output polarity (CCxP)

Bad: "CCxP sets whether the output is active high or low, depending on your load."

Good: Two ways to wire an LED. Wiring A: pin → LED → GND, so pin high turns it on. Wiring B: 3.3 V → LED → pin, so pin low turns it on. The timer only knows "on" and "off". CCxP = 0 says "on means high" (wiring A). CCxP = 1 says "on means low" (wiring B). Show the 20-tick strip for both. Then give the names. Then say how it differs from PWM mode 2, which changes *when* the channel is on, not the voltage.
