#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwunw/dwunw_api.h"
#include "dwunw/unwind.h"
#include "memleak_events.h"

static const char *
get_fixture_path(void)
{
    const char *path = getenv("DWUNW_TEST_FIXTURE");
    assert(path && "DWUNW_TEST_FIXTURE env is required");
    return path;
}

static void
populate_event(struct memleak_event *evt)
{
    memset(evt, 0, sizeof(*evt));
    evt->arch = MEMLEAK_ARCH_X86_64;
    evt->reg_version = MEMLEAK_REGSET_VERSION;
    evt->sp = 0x7fff0000u;
    evt->pc = 0x401000u;
    evt->regs[0] = 0x1234;
    evt->regs[1] = 0x5678;
}

static void
run_capture_once(struct dwunw_context *ctx, const char *module_path)
{
    struct memleak_event evt;
    struct dwunw_regset regs;
    struct dwunw_frame frames[2];
    struct dwunw_unwind_request req;
    size_t written = 0;

    populate_event(&evt);
    assert(memleak_event_to_regset(&evt, &regs) == DWUNW_OK);

    memset(frames, 0, sizeof(frames));

    req.module_path = module_path;
    req.regs = &regs;
    req.frames = frames;
    req.max_frames = 2;
    req.options = DWUNW_OPTION_NONE;

    assert(dwunw_capture(ctx, &req, &written) == DWUNW_OK);
    assert(written == 1);
    assert(frames[0].pc == evt.pc);
    assert(frames[0].sp == evt.sp);
    assert(strcmp(frames[0].module_path, module_path) == 0);
    assert(frames[0].flags & DWUNW_FRAME_FLAG_PARTIAL);
}

int
main(void)
{
    const char *module_path = get_fixture_path();
    struct dwunw_context ctx;

    assert(dwunw_init(&ctx) == DWUNW_OK);

    run_capture_once(&ctx, module_path);
    run_capture_once(&ctx, module_path);

    dwunw_shutdown(&ctx);

    puts("integration: ok");
    return 0;
}
