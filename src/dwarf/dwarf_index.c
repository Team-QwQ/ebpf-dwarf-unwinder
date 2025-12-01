#include <string.h>

#include "dwunw/dwarf_index.h"

void
dwunw_dwarf_index_reset(struct dwunw_dwarf_index *index)
{
    if (!index) {
        return;
    }

    memset(index, 0, sizeof(*index));
}

dwunw_status_t
dwunw_dwarf_index_init(struct dwunw_dwarf_index *index,
                      const struct dwunw_elf_handle *handle)
{
    dwunw_status_t status;

    if (!index || !handle) {
        return DWUNW_ERR_INVALID_ARG;
    }

    dwunw_dwarf_index_reset(index);

    status = dwunw_elf_collect_dwarf(handle, &index->sections);
    if (status != DWUNW_OK) {
        return status;
    }

    index->flags = 0;
    return DWUNW_OK;
}
