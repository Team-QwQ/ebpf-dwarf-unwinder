#ifndef DWUNW_CONFIG_H
#define DWUNW_CONFIG_H

#define DWUNW_VERSION_MAJOR 0
#define DWUNW_VERSION_MINOR 1
#define DWUNW_VERSION_PATCH 0

/*
 * Stage 1 focuses on skeleton wiring, so architecture fan-out is
 * limited to the three primary targets. Future updates may allow
 * callers to extend this list at runtime.
 */
#define DWUNW_MAX_ARCH_COUNT 3

#endif /* DWUNW_CONFIG_H */
