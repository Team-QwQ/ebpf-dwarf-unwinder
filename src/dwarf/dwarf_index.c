#include <string.h>

#include "dwunw/dwarf_index.h"

#include "cfi.h"

void
dwunw_dwarf_index_reset(struct dwunw_dwarf_index *index)
{
    if (!index) {
        return;
    }

    if (index->cies || index->fdes) {
        dwunw_cfi_free(index->cies, index->fdes);
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

    status = dwunw_cfi_build(&index->sections,
                             &index->cies,
                             &index->cie_count,
                             &index->fdes,
                             &index->fde_count);
    if (status != DWUNW_OK) {
        switch (status) {
        case DWUNW_ERR_NO_DEBUG_DATA:
        case DWUNW_ERR_NOT_IMPLEMENTED:
        case DWUNW_ERR_BAD_FORMAT:
            status = DWUNW_OK;
            break;
        default:
            return status;
        }
    }

    index->flags = 0;
    return DWUNW_OK;
}
