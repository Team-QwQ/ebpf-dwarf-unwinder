#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwunw/dwunw_api.h"
#include "dwunw/unwind.h"

static const char *
get_fixture_path(void)
{
    const char *path = getenv("DWUNW_TEST_FIXTURE");
    assert(path && "DWUNW_TEST_FIXTURE env is required");
    return path;
}

static void
test_single_frame(void)
{
    struct dwunw_context ctx;
    struct dwunw_regset regs;
    struct dwunw_frame frames[4];
    struct dwunw_unwind_request req;
    size_t written = 0;

    assert(dwunw_init(&ctx) == DWUNW_OK);
    assert(dwunw_regset_prepare(&regs, DWUNW_ARCH_X86_64) == DWUNW_OK);

    memset(frames, 0, sizeof(frames));
    regs.sp = 0x1000;
    regs.pc = 0x2000;

    memset(&req, 0, sizeof(req));
    req.module_path = get_fixture_path();
    req.regs = &regs;
    req.frames = frames;
    req.max_frames = 4;
    req.options = DWUNW_OPTION_NONE;

    assert(dwunw_capture(&ctx, &req, &written) == DWUNW_OK);
    assert(written == 1);
    assert(frames[0].pc == regs.pc);
    assert(frames[0].sp == regs.sp);
    assert(strcmp(frames[0].module_path, req.module_path) == 0);

    dwunw_shutdown(&ctx);
}

static void
test_invalid_inputs(void)
{
    struct dwunw_context ctx;
    struct dwunw_unwind_request req = {0};
    size_t written = 0;

    assert(dwunw_init(&ctx) == DWUNW_OK);
    assert(dwunw_capture(NULL, &req, &written) == DWUNW_ERR_INVALID_ARG);
    assert(dwunw_capture(&ctx, NULL, &written) == DWUNW_ERR_INVALID_ARG);
    dwunw_shutdown(&ctx);
}

int
main(void)
{
    test_invalid_inputs();
    test_single_frame();
    puts("unwinder: ok");
    return 0;
}
