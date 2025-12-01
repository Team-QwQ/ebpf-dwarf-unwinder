# Copilot Instructions

## Repository Context
- Cross-platform DWARF-driven eBPF unwinder; public APIs live in `include/dwunw/` and are exercised by `tests/unit/*.c`.
- Layout (see AGENTS.md): production code under `src/`, docs in `doc/`, specs/plans tracked in Chinese under `specs/` and `plans/`, and `src/ref/` is read-only reference material.
- Key components: `src/core/` for lifecycle + arch registry, `src/dwarf/` for ELF/DWARF ingestion (`dwunw_elf_open`, `dwunw_dwarf_index_init`), `src/arch/<arch>/arch_ops.c` for per-ISA helpers, and `src/unwinder/dwunw_unwind.c` for frame capture orchestration.

## Architecture Guide
- `include/dwunw/status.h` defines the errno-style contract; always return `dwunw_status_t` from public helpers.
- `dwunw_regset_prepare` (core/dwunw_arch_registry.c) normalizes the register window before any unwind; new entry points must call it rather than rolling their own structs.
- `dwunw_module_cache` (`src/dwarf/module_cache.c`) is a fixed 16-slot cache keyed by module path; always pair `dwunw_module_cache_acquire` with `_release` even on failures to avoid leaking handles.
- `dwunw_capture` currently emits a single root frame by querying arch ops; extending past frame[0] means plumbing through DWARF FDE parsing in `src/dwarf/` rather than modifying arch glue.

## Build & Test Workflow
- Primary targets: `make ARCH=x86_64|arm64|mips32 all` builds `build/<arch>/libdwunw.a` using toolchains wired in `mk/toolchain.mk`; override via `CROSS_COMPILE=...` when needed.
- `make test` compiles fixtures (`tests/fixtures/dwarf_fixture.c`) with the host compiler, sets `DWUNW_TEST_FIXTURE` per invocation, and runs `tests/unit/*`; mimic this env var when running binaries manually.
- `make examples` builds `examples/bpf_memleak/memleak_user` and expects libbpf headers/libs; pass `LIBBPF_CFLAGS` and `LIBBPF_LDLIBS` if your environment stores them outside the default paths.
- `make print-config` is the quickest way to confirm which compiler/flags an agent resolved before launching a long build.

## Coding Patterns
- Stick to portable C11 with libc/syscall primitives only; no glibc-only helpers inside `src/` because the unwinder may run inside minimal eBPF loaders.
- Per-arch ops follow the same skeleton as `src/arch/x86_64/arch_ops.c`: expose `normalize`, `compute_cfa`, `read_return_addr`, `open_frame`, and return `DWUNW_ERR_NOT_IMPLEMENTED` for unsupported hooks so shared tests continue to pass.
- ELF handling loads whole files into memory (`dwunw_elf_load_image`) and slices sections via offsets; every new parser must bounds-check before dereferencing to maintain the `DWUNW_ERR_BAD_FORMAT` guarantees.
- When touching frames, respect `DWUNW_MAX_PATH_LEN` and `DWUNW_FRAME_FLAG_PARTIAL` from `include/dwunw/unwind.h`; root frames are partial until DWARF expansion runs.

## Documentation & Workflow
- Specs/plans/docs stay in Chinese and should use mermaid diagrams when depicting flows; no English change logs inside those files—update content in place.
- Stage-gated flow (`/spec` → `/plan` → `/do`) is mandatory once invoked; do not modify `src/` during `/spec` or `/plan`.
- Record toolchain notes, ABI quirks, or new validation steps under `doc/` so future agents understand cross-compilation expectations.

## Common Pitfalls
- Never modify or depend on `src/ref/*` when implementing production features; treat it solely as inspiration.
- Tests rely on plain `assert` and exit on first failure; add fresh cases beside `tests/unit/test_*.c` instead of creating a new framework to keep CI predictable.
- Module cache and ELF handles are not thread-safe; if you add concurrency, centralize locking at the cache layer instead of sprinkling mutexes across callers.
- When plumbing new data into the unwinder, wire headers under `include/dwunw/` first so both library code and external consumers stay in sync.
