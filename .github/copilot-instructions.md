# Copilot Instructions

## Repository Context
- Project goal: build a cross-platform DWARF-powered eBPF stack unwinder (see README.md). Keep portability in mind for arm64, mips, and other targets.
- Expected layout (from AGENTS.md): `src/` (main code), `src/ref/` (reference-only examples), `doc/`, `specs/`, `plans/`, and `tests/`. Create folders as needed rather than mixing concerns.

## Coding Standards
- Write C; prefer portable, libc-level facilities so code compiles across toolchains (kernel-style constraints apply because eBPF integrations often lack glibc).
- Prioritize readability and maintainability: modularize unwinder stages (symbol parsing, DWARF decoding, frame reconstruction) so new CPU backends can plug in cleanly.
- Design for robustness: defensive checks for malformed DWARF, missing symbols, or unsupported ABIs are mandatory. Fail with actionable error paths rather than silent fallbacks.
- Performance matters: avoid unnecessary heap allocations inside hot unwinding loops; prefer stack buffers or arenas that can be reused per trace.

## Documentation & Specs
- All docs/specs/plans must be written in Chinese. Use mermaid diagrams for flows (e.g., unwinder state machine) when visuals help.
- Stage-gated workflow applies when the user invokes `/spec`, `/plan`, or `/do`; never touch source before `/do` in that mode. Specs live under `specs/`; plans use `plans/YYYY-MM-DD-short-title.md`.
- Keep reference-only code in `src/ref/` untouched when implementing real features; instead, port patterns into `src/` with project-level styling.

## Testing Expectations
- Tests should be portable and runnable on generic Linux toolchains; avoid vendor-specific harnesses unless absolutely required.
- Provide clear stdout summaries so CI/log parsing can spot regressions without extra tooling. When simulating unwinds, dump both the expected and actual call stacks for comparison.

## Workflow Tips
- Before coding, confirm which stage (spec/plan/do) you are in and mention it back to the user if ambiguous.
- When adding new components, co-locate helper headers/implementation files under `src/` subdirectories that mirror functional areas (e.g., `src/dwarf/`, `src/unwinder/`).
- If adding cross-compilation support, document the exact toolchain in `doc/` and keep build scripts parameterized so new architectures can be slotted in.
