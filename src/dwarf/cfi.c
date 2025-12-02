#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "cfi.h"

#define DW_CFA_OPCODE_MASK      0xc0
#define DW_CFA_OPERAND_MASK     0x3f
#define DW_CFA_ADVANCE_LOC      0x40
#define DW_CFA_OFFSET           0x80
#define DW_CFA_RESTORE          0xc0

#define DW_EH_PE_absptr  0x00
#define DW_EH_PE_uleb128 0x01
#define DW_EH_PE_udata2  0x02
#define DW_EH_PE_udata4  0x03
#define DW_EH_PE_udata8  0x04
#define DW_EH_PE_sdata2  0x05
#define DW_EH_PE_sdata4  0x06
#define DW_EH_PE_sdata8  0x07
#define DW_EH_PE_signed  0x08

#define DW_EH_PE_pcrel   0x10
#define DW_EH_PE_textrel 0x20
#define DW_EH_PE_datarel 0x30
#define DW_EH_PE_funcrel 0x40
#define DW_EH_PE_aligned 0x50

#define DW_EH_PE_indirect 0x80

enum rule_kind {
	RULE_UNDEFINED = 0,
	RULE_SAME_VALUE,
	RULE_OFFSET,
};

struct reg_rule {
	enum rule_kind kind;
	int64_t offset;
};

/* Tracks the DWARF virtual machine state while replaying CIE/FDE programs. */
struct cfa_state {
	uint16_t cfa_reg;
	int64_t cfa_offset;
	struct reg_rule regs[DWUNW_REGSET_SLOTS];
};

/* Simple growable arrays keep parsing logic independent from libc extras. */
struct cie_vector {
	struct dwunw_cie_record *data;
	size_t count;
	size_t capacity;
};

struct fde_vector {
	struct dwunw_fde_record *data;
	size_t count;
	size_t capacity;
};

/* Establish the DWARF-specified defaults before executing any opcode. */
static void
cfa_state_reset(struct cfa_state *state)
{
	size_t i;

	state->cfa_reg = UINT16_MAX;
	state->cfa_offset = 0;
	for (i = 0; i < DWUNW_REGSET_SLOTS; ++i) {
		state->regs[i].kind = RULE_SAME_VALUE;
		state->regs[i].offset = 0;
	}
}

/* Tiny vector implementation keeps allocations amortized without pulling in
 * non-portable helpers. */
static dwunw_status_t
vector_reserve(void **data, size_t elem_size, size_t *capacity, size_t min_cap)
{
	size_t new_cap;
	void *tmp;

	if (*capacity >= min_cap) {
		return DWUNW_OK;
	}

	new_cap = *capacity ? *capacity * 2 : 8;
	if (new_cap < min_cap) {
		new_cap = min_cap;
	}

	tmp = realloc(*data, new_cap * elem_size);
	if (!tmp) {
		return DWUNW_ERR_IO;
	}

	*data = tmp;
	*capacity = new_cap;
	return DWUNW_OK;
}

static dwunw_status_t
cie_vector_append(struct cie_vector *vec, const struct dwunw_cie_record *rec)
{
	dwunw_status_t st = vector_reserve((void **)&vec->data,
									   sizeof(*vec->data),
									   &vec->capacity,
									   vec->count + 1);
	if (st != DWUNW_OK) {
		return st;
	}

	vec->data[vec->count++] = *rec;
	return DWUNW_OK;
}

static dwunw_status_t
fde_vector_append(struct fde_vector *vec, const struct dwunw_fde_record *rec)
{
	dwunw_status_t st = vector_reserve((void **)&vec->data,
									   sizeof(*vec->data),
									   &vec->capacity,
									   vec->count + 1);
	if (st != DWUNW_OK) {
		return st;
	}

	vec->data[vec->count++] = *rec;
	return DWUNW_OK;
}

static const struct dwunw_cie_record *
find_cie(const struct cie_vector *vec, uint64_t offset)
{
	size_t i;

	for (i = 0; i < vec->count; ++i) {
		if (vec->data[i].offset == offset) {
			return &vec->data[i];
		}
	}
	return NULL;
}

static dwunw_status_t
read_uleb(const uint8_t **cursor, const uint8_t *end, uint64_t *value)
{
	uint64_t result = 0;
	unsigned shift = 0;

	while (*cursor < end) {
		uint8_t byte = **cursor;
		(*cursor)++;
		result |= (uint64_t)(byte & 0x7f) << shift;
		if (!(byte & 0x80)) {
			*value = result;
			return DWUNW_OK;
		}
		shift += 7;
		if (shift >= 64) {
			return DWUNW_ERR_BAD_FORMAT;
		}
	}

	return DWUNW_ERR_BAD_FORMAT;
}

static dwunw_status_t
read_sleb(const uint8_t **cursor, const uint8_t *end, int64_t *value)
{
	int64_t result = 0;
	unsigned shift = 0;
	uint8_t byte;

	while (*cursor < end) {
		byte = **cursor;
		(*cursor)++;

		result |= ((int64_t)(byte & 0x7f)) << shift;
		shift += 7;

		if (!(byte & 0x80)) {
			if (shift < 64 && (byte & 0x40)) {
				result |= -((int64_t)1 << shift);
			}
			*value = result;
			return DWUNW_OK;
		}

		if (shift >= 64) {
			return DWUNW_ERR_BAD_FORMAT;
		}
	}

	return DWUNW_ERR_BAD_FORMAT;
}

static uint32_t
read_u32(const uint8_t *ptr)
{
	uint32_t v;
	memcpy(&v, ptr, sizeof(v));
	return v;
}

static uint64_t
read_u64(const uint8_t *ptr)
{
	uint64_t v;
	memcpy(&v, ptr, sizeof(v));
	return v;
}

static dwunw_status_t
read_fixed_size(const uint8_t **cursor, const uint8_t *end, size_t size, uint64_t *value)
{
	uint64_t v = 0;

	if ((size_t)(end - *cursor) < size) {
		return DWUNW_ERR_BAD_FORMAT;
	}

	if (size == 8) {
		v = read_u64(*cursor);
	} else if (size == 4) {
		v = read_u32(*cursor);
	} else if (size == 2) {
		uint16_t tmp;
		memcpy(&tmp, *cursor, sizeof(tmp));
		v = tmp;
	} else if (size == 1) {
		v = **cursor;
	} else {
		return DWUNW_ERR_BAD_FORMAT;
	}

	*cursor += size;
	*value = v;
	return DWUNW_OK;
}

/* .eh_frame encodes pointers relative to different bases; this routine
 * normalizes them into absolute addresses understood by the unwinder. */
static dwunw_status_t
read_encoded_pointer(uint8_t encoding,
			 const uint8_t **cursor,
			 const uint8_t *section_start,
			 const uint8_t *end,
			 uint64_t *value)
{
	uint64_t base = 0;
	uint64_t raw = 0;
	dwunw_status_t st;
	uint8_t format = encoding & 0x0f;
	uint8_t application = encoding & 0x70;

	if (encoding == DW_EH_PE_absptr) {
		format = DW_EH_PE_udata8;
		application = 0;
	}

	switch (format) {
	case DW_EH_PE_udata8:
		st = read_fixed_size(cursor, end, 8, &raw);
		break;
	case DW_EH_PE_udata4:
		st = read_fixed_size(cursor, end, 4, &raw);
		break;
	case DW_EH_PE_udata2:
		st = read_fixed_size(cursor, end, 2, &raw);
		break;
	case DW_EH_PE_uleb128:
		st = read_uleb(cursor, end, &raw);
		break;
	case DW_EH_PE_sdata4: {
		int64_t tmp;
		st = read_fixed_size(cursor, end, 4, (uint64_t *)&tmp);
		raw = (uint64_t)tmp;
		break;
	}
	case DW_EH_PE_sdata8: {
		int64_t tmp;
		st = read_fixed_size(cursor, end, 8, (uint64_t *)&tmp);
		raw = (uint64_t)tmp;
		break;
	}
	default:
		return DWUNW_ERR_NOT_IMPLEMENTED;
	}

	if (st != DWUNW_OK) {
		return st;
	}

	switch (application) {
	case 0:
		base = 0;
		break;
	case DW_EH_PE_pcrel:
		base = (uint64_t)(section_start + (*cursor - section_start));
		break;
	default:
		return DWUNW_ERR_NOT_IMPLEMENTED;
	}

	*value = raw + base;
	return DWUNW_OK;
}

/* A CIE describes shared defaults for a batch of FDEs. Capture only the
 * fields we need later when executing unwind bytecode. */
static dwunw_status_t
parse_cie(const struct dwunw_dwarf_section *section,
	  const uint8_t *entry_start,
	  const uint8_t *payload,
	  const uint8_t *entry_end,
	  bool is_eh,
	  struct cie_vector *cies)
{
	struct dwunw_cie_record cie;
	const char *augmentation;
	size_t aug_len;
	dwunw_status_t st;

	memset(&cie, 0, sizeof(cie));
	cie.offset = (uint64_t)(entry_start - section->data);
	cie.ptr_encoding = DW_EH_PE_absptr;
	cie.address_size = 8;

	if (payload >= entry_end) {
		return DWUNW_ERR_BAD_FORMAT;
	}

	cie.version = *payload++;
	augmentation = (const char *)payload;
	aug_len = strnlen(augmentation, (size_t)(entry_end - payload));
	if (payload + aug_len >= entry_end) {
		return DWUNW_ERR_BAD_FORMAT;
	}
	payload += aug_len + 1;
	cie.augmentation = augmentation;
	cie.augmentation_len = aug_len;

	st = read_uleb(&payload, entry_end, &cie.code_align);
	if (st != DWUNW_OK) {
		return st;
	}

	st = read_sleb(&payload, entry_end, &cie.data_align);
	if (st != DWUNW_OK) {
		return st;
	}

	uint64_t return_reg = 0;
	st = read_uleb(&payload, entry_end, &return_reg);
	if (st != DWUNW_OK) {
		return st;
	}
	cie.return_reg = (uint8_t)return_reg;

	if (!is_eh && cie.version >= 3) {
		if (payload + 2 > entry_end) {
			return DWUNW_ERR_BAD_FORMAT;
		}
		cie.address_size = *payload++;
		payload++; /* segment selector size */
	}

	if (aug_len > 0 && augmentation[0] == 'z') {
		uint64_t aug_size = 0;
		const uint8_t *aug_end;

		st = read_uleb(&payload, entry_end, &aug_size);
		if (st != DWUNW_OK) {
			return st;
		}

		if (payload + aug_size > entry_end) {
			return DWUNW_ERR_BAD_FORMAT;
		}

		aug_end = payload + aug_size;

		for (size_t i = 1; i < aug_len; ++i) {
			if (augmentation[i] == 'R') {
				if (payload >= aug_end) {
					return DWUNW_ERR_BAD_FORMAT;
				}
				cie.ptr_encoding = *payload++;
			} else {
				/* skip unsupported augmentation data */
				payload = aug_end;
				break;
			}
		}

		payload = aug_end;
	}

	cie.instructions = payload;
	cie.instructions_size = (size_t)(entry_end - payload);

	return cie_vector_append(cies, &cie);
}

/* Each FDE covers a PC range; link it back to its parent CIE and cache the
 * decoded instruction stream for later evaluation. */
static dwunw_status_t
parse_fde(const struct dwunw_dwarf_section *section,
	  const uint8_t *entry_start,
	  uint32_t cie_pointer,
	  const uint8_t *payload,
	  const uint8_t *entry_end,
	  bool is_eh,
	  const struct cie_vector *cies,
	  struct fde_vector *fdes)
{
	const struct dwunw_cie_record *cie;
	struct dwunw_fde_record fde;
	uint64_t cie_offset;
	dwunw_status_t st;
	uint8_t default_encoding;
	const uint8_t *section_start = section->data;

	if (is_eh) {
		uint64_t field_offset = (uint64_t)(payload - section_start);
		cie_offset = field_offset - cie_pointer;
	} else {
		cie_offset = cie_pointer;
	}

	cie = find_cie(cies, cie_offset);
	if (!cie) {
		return DWUNW_ERR_BAD_FORMAT;
	}

	memset(&fde, 0, sizeof(fde));
	fde.cie = cie;
	fde.offset = (uint64_t)(entry_start - section_start);
	default_encoding = cie->ptr_encoding ? cie->ptr_encoding : DW_EH_PE_absptr;

	st = read_encoded_pointer(default_encoding,
							  &payload,
							  section_start,
							  entry_end,
							  &fde.pc_begin);
	if (st != DWUNW_OK) {
		return st;
	}

	st = read_encoded_pointer(default_encoding & 0x0f,
							  &payload,
							  section_start,
							  entry_end,
							  &fde.pc_range);
	if (st != DWUNW_OK) {
		return st;
	}

	if (cie->augmentation_len > 0 && cie->augmentation[0] == 'z') {
		uint64_t aug_size = 0;
		st = read_uleb(&payload, entry_end, &aug_size);
		if (st != DWUNW_OK) {
			return st;
		}
		if (payload + aug_size > entry_end) {
			return DWUNW_ERR_BAD_FORMAT;
		}
		payload += aug_size;
	}

	fde.instructions = payload;
	fde.instructions_size = (size_t)(entry_end - payload);
	return fde_vector_append(fdes, &fde);
}

/* Walk an entire .debug_frame or .eh_frame section, demultiplexing the mixed
 * stream of CIEs and FDEs into separate tables. */
static dwunw_status_t
parse_section(const struct dwunw_dwarf_section *section,
	  bool is_eh,
	  struct cie_vector *cies,
	  struct fde_vector *fdes)
{
	const uint8_t *ptr;
	const uint8_t *end;

	if (!section->data || section->size == 0) {
		return DWUNW_OK;
	}

	ptr = section->data;
	end = section->data + section->size;

	while (ptr + 4 <= end) {
		const uint8_t *entry_start = ptr;
		uint32_t length = read_u32(ptr);
		ptr += 4;

		if (length == 0) {
			break;
		}

		if (ptr + length > end) {
			return DWUNW_ERR_BAD_FORMAT;
		}

		const uint8_t *entry_end = ptr + length;
		uint32_t id = read_u32(ptr);
		ptr += 4;

		if ((is_eh && id == 0) || (!is_eh && id == 0xffffffff)) {
			dwunw_status_t st = parse_cie(section,
										  entry_start,
										  ptr,
										  entry_end,
										  is_eh,
										  cies);
			if (st != DWUNW_OK) {
				return st;
			}
		} else {
			dwunw_status_t st = parse_fde(section,
										  entry_start,
										  id,
										  ptr,
										  entry_end,
										  is_eh,
										  cies,
										  fdes);
			if (st != DWUNW_OK) {
				return st;
			}
		}

		ptr = entry_end;
	}

	return DWUNW_OK;
}

dwunw_status_t
dwunw_cfi_build(const struct dwunw_dwarf_sections *sections,
				struct dwunw_cie_record **cies_out,
				size_t *cie_count,
				struct dwunw_fde_record **fdes_out,
				size_t *fde_count)
{
	struct cie_vector cies = {0};
	struct fde_vector fdes = {0};
	dwunw_status_t st;

	if (!sections || !cies_out || !cie_count || !fdes_out || !fde_count) {
		return DWUNW_ERR_INVALID_ARG;
	}

	*cies_out = NULL;
	*cie_count = 0;
	*fdes_out = NULL;
	*fde_count = 0;

	st = parse_section(&sections->eh_frame, true, &cies, &fdes);
	if (st != DWUNW_OK) {
		dwunw_cfi_free(cies.data, fdes.data);
		return st;
	}

	st = parse_section(&sections->debug_frame, false, &cies, &fdes);
	if (st != DWUNW_OK) {
		dwunw_cfi_free(cies.data, fdes.data);
		return st;
	}

	if (fdes.count == 0) {
		dwunw_cfi_free(cies.data, fdes.data);
		return DWUNW_ERR_NO_DEBUG_DATA;
	}

	*cies_out = cies.data;
	*cie_count = cies.count;
	*fdes_out = fdes.data;
	*fde_count = fdes.count;
	return DWUNW_OK;
}

void
dwunw_cfi_free(struct dwunw_cie_record *cies,
			   struct dwunw_fde_record *fdes)
{
	free(cies);
	free(fdes);
}

const struct dwunw_fde_record *
dwunw_cfi_find_fde(const struct dwunw_fde_record *fdes,
				   size_t count,
				   uint64_t pc)
{
	size_t i;

	if (!fdes) {
		return NULL;
	}

	for (i = 0; i < count; ++i) {
		const struct dwunw_fde_record *fde = &fdes[i];
		if (pc >= fde->pc_begin && pc < fde->pc_begin + fde->pc_range) {
			return fde;
		}
	}

	return NULL;
}

static uint64_t
reg_value(const struct dwunw_regset *regs, uint16_t reg)
{
	if (reg == UINT16_MAX) {
		return 0;
	}
	if (reg < DWUNW_REGSET_SLOTS) {
		return regs->regs[reg];
	}
	return 0;
}

/* Resolve a DW_CFA rule into an actual register value by reading memory
 * relative to the computed CFA. */
static dwunw_status_t
apply_rule(enum rule_kind kind,
	   int64_t offset,
	   uint64_t cfa,
	   dwunw_memory_read_fn reader,
	   void *reader_ctx,
	   uint64_t *out_value)
{
	dwunw_status_t st;

	switch (kind) {
	case RULE_OFFSET: {
		uint64_t addr = cfa + offset;
		st = reader(reader_ctx, addr, out_value, sizeof(*out_value));
		return st;
	}
	default:
		return DWUNW_ERR_NOT_IMPLEMENTED;
	}
}

/* Interpret the DWARF call-frame opcodes until we either finish the program
 * or advance past the target PC. */
static dwunw_status_t
execute_cfi(const struct dwunw_cie_record *cie,
		const uint8_t *program,
		size_t program_size,
		uint64_t pc_begin,
		uint64_t target_pc,
		struct cfa_state *state,
		const struct cfa_state *initial)
{
	const uint8_t *cursor = program;
	const uint8_t *end = program + program_size;
	uint64_t pc_offset = 0;

	while (cursor < end) {
		uint8_t opcode = *cursor++;

		if ((opcode & DW_CFA_OPCODE_MASK) == DW_CFA_ADVANCE_LOC) {
			uint8_t delta = opcode & DW_CFA_OPERAND_MASK;
			pc_offset += delta * cie->code_align;
			if (pc_begin + pc_offset > target_pc) {
				break;
			}
			continue;
		}

		if ((opcode & DW_CFA_OPCODE_MASK) == DW_CFA_OFFSET) {
			uint16_t reg = opcode & DW_CFA_OPERAND_MASK;
			uint64_t offset = 0;
			dwunw_status_t st = read_uleb(&cursor, end, &offset);
			if (st != DWUNW_OK) {
				return st;
			}
			if (reg < DWUNW_REGSET_SLOTS) {
				state->regs[reg].kind = RULE_OFFSET;
				state->regs[reg].offset = (int64_t)offset * cie->data_align;
			}
			continue;
		}

		if ((opcode & DW_CFA_OPCODE_MASK) == DW_CFA_RESTORE) {
			uint16_t reg = opcode & DW_CFA_OPERAND_MASK;
			if (initial && reg < DWUNW_REGSET_SLOTS) {
				state->regs[reg] = initial->regs[reg];
			}
			continue;
		}

		switch (opcode) {
		case 0x00: /* DW_CFA_nop */
			break;
		case 0x01: { /* DW_CFA_set_loc */
			uint64_t loc = 0;
			dwunw_status_t st = read_fixed_size(&cursor,
												end,
												cie->address_size,
												&loc);
			if (st != DWUNW_OK) {
				return st;
			}
			pc_offset = loc - pc_begin;
			if (pc_begin + pc_offset > target_pc) {
				cursor = end;
			}
			break;
		}
		case 0x02: { /* DW_CFA_advance_loc1 */
			uint64_t delta;
			dwunw_status_t st = read_fixed_size(&cursor, end, 1, &delta);
			if (st != DWUNW_OK) {
				return st;
			}
			pc_offset += delta * cie->code_align;
			if (pc_begin + pc_offset > target_pc) {
				cursor = end;
			}
			break;
		}
		case 0x03: { /* DW_CFA_advance_loc2 */
			uint64_t delta;
			dwunw_status_t st = read_fixed_size(&cursor, end, 2, &delta);
			if (st != DWUNW_OK) {
				return st;
			}
			pc_offset += delta * cie->code_align;
			if (pc_begin + pc_offset > target_pc) {
				cursor = end;
			}
			break;
		}
		case 0x04: { /* DW_CFA_advance_loc4 */
			uint64_t delta;
			dwunw_status_t st = read_fixed_size(&cursor, end, 4, &delta);
			if (st != DWUNW_OK) {
				return st;
			}
			pc_offset += delta * cie->code_align;
			if (pc_begin + pc_offset > target_pc) {
				cursor = end;
			}
			break;
		}
		case 0x0c: { /* DW_CFA_def_cfa */
			uint64_t reg = 0;
			uint64_t offset = 0;
			dwunw_status_t st = read_uleb(&cursor, end, &reg);
			if (st != DWUNW_OK) {
				return st;
			}
			st = read_uleb(&cursor, end, &offset);
			if (st != DWUNW_OK) {
				return st;
			}
			state->cfa_reg = (uint16_t)reg;
			state->cfa_offset = (int64_t)offset;
			break;
		}
		case 0x0d: { /* DW_CFA_def_cfa_register */
			uint64_t reg = 0;
			dwunw_status_t st = read_uleb(&cursor, end, &reg);
			if (st != DWUNW_OK) {
				return st;
			}
			state->cfa_reg = (uint16_t)reg;
			break;
		}
		case 0x0e: { /* DW_CFA_def_cfa_offset */
			uint64_t offset = 0;
			dwunw_status_t st = read_uleb(&cursor, end, &offset);
			if (st != DWUNW_OK) {
				return st;
			}
			state->cfa_offset = (int64_t)offset;
			break;
		}
		case 0x0f: { /* DW_CFA_def_cfa_expression */
			return DWUNW_ERR_NOT_IMPLEMENTED;
		}
		case 0x10: { /* DW_CFA_expression */
			return DWUNW_ERR_NOT_IMPLEMENTED;
		}
		case 0x11: { /* DW_CFA_offset_extended */
			uint64_t reg = 0;
			uint64_t offset = 0;
			dwunw_status_t st = read_uleb(&cursor, end, &reg);
			if (st != DWUNW_OK) {
				return st;
			}
			st = read_uleb(&cursor, end, &offset);
			if (st != DWUNW_OK) {
				return st;
			}
			if (reg < DWUNW_REGSET_SLOTS) {
				state->regs[reg].kind = RULE_OFFSET;
				state->regs[reg].offset = (int64_t)offset * cie->data_align;
			}
			break;
		}
		case 0x12: { /* DW_CFA_restore_extended */
			uint64_t reg = 0;
			dwunw_status_t st = read_uleb(&cursor, end, &reg);
			if (st != DWUNW_OK) {
				return st;
			}
			if (initial && reg < DWUNW_REGSET_SLOTS) {
				state->regs[reg] = initial->regs[reg];
			}
			break;
		}
		case 0x1a: { /* DW_CFA_def_cfa_sf */
			uint64_t reg = 0;
			int64_t offset;
			dwunw_status_t st = read_uleb(&cursor, end, &reg);
			if (st != DWUNW_OK) {
				return st;
			}
			st = read_sleb(&cursor, end, &offset);
			if (st != DWUNW_OK) {
				return st;
			}
			state->cfa_reg = (uint16_t)reg;
			state->cfa_offset = offset * cie->data_align;
			break;
		}
		case 0x1b: { /* DW_CFA_def_cfa_offset_sf */
			int64_t offset;
			dwunw_status_t st = read_sleb(&cursor, end, &offset);
			if (st != DWUNW_OK) {
				return st;
			}
			state->cfa_offset = offset * cie->data_align;
			break;
		}
		default:
			return DWUNW_ERR_NOT_IMPLEMENTED;
		}
	}

	return DWUNW_OK;
}

dwunw_status_t
dwunw_cfi_eval(const struct dwunw_fde_record *fde,
			   uint64_t pc,
			   struct dwunw_regset *regs,
			   dwunw_memory_read_fn reader,
			   void *reader_ctx,
			   struct dwunw_frame *frame)
{
    /* Replay the CIE defaults and FDE instructions to recover caller state. */
	struct cfa_state current;
	struct cfa_state initial;
	dwunw_status_t st;
	uint64_t cfa_value;
	uint64_t ra_value;

	if (!fde || !frame || !regs || !reader) {
		return DWUNW_ERR_INVALID_ARG;
	}

	if (pc < fde->pc_begin || pc >= fde->pc_begin + fde->pc_range) {
		return DWUNW_ERR_INVALID_ARG;
	}

	cfa_state_reset(&current);
	initial = current;

	/* Apply CIE defaults */
	st = execute_cfi(fde->cie,
					 fde->cie->instructions,
					 fde->cie->instructions_size,
					 fde->pc_begin,
					 UINT64_MAX,
					 &current,
					 &initial);
	if (st != DWUNW_OK && st != DWUNW_ERR_NOT_IMPLEMENTED) {
		return st;
	}
	initial = current;

	/* Apply FDE instructions up to target PC */
	st = execute_cfi(fde->cie,
					 fde->instructions,
					 fde->instructions_size,
					 fde->pc_begin,
					 pc,
					 &current,
					 &initial);
	if (st != DWUNW_OK && st != DWUNW_ERR_NOT_IMPLEMENTED) {
		return st;
	}

	if (current.cfa_reg == UINT16_MAX) {
		return DWUNW_ERR_NOT_IMPLEMENTED;
	}

	cfa_value = reg_value(regs, current.cfa_reg) + current.cfa_offset;

	switch (current.regs[fde->cie->return_reg].kind) {
	case RULE_SAME_VALUE:
		ra_value = regs->regs[fde->cie->return_reg];
		break;
	case RULE_OFFSET:
		st = apply_rule(RULE_OFFSET,
						current.regs[fde->cie->return_reg].offset,
						cfa_value,
						reader,
						reader_ctx,
						&ra_value);
		if (st != DWUNW_OK) {
			return st;
		}
		break;
	default:
		return DWUNW_ERR_NOT_IMPLEMENTED;
	}

	frame->pc = ra_value;
	frame->ra = ra_value;
	frame->sp = cfa_value;
	frame->cfa = cfa_value;
	frame->flags = 0;

	/* Update register snapshot for caller frame */
	for (uint16_t reg = 0; reg < DWUNW_REGSET_SLOTS; ++reg) {
		switch (current.regs[reg].kind) {
		case RULE_SAME_VALUE:
			break;
		case RULE_OFFSET: {
			uint64_t value;
			st = apply_rule(current.regs[reg].kind,
							current.regs[reg].offset,
							cfa_value,
							reader,
							reader_ctx,
							&value);
			if (st != DWUNW_OK) {
				return st;
			}
			regs->regs[reg] = value;
			break;
		}
		default:
			regs->regs[reg] = 0;
			break;
		}
	}

	regs->pc = ra_value;
	regs->sp = cfa_value;

	return DWUNW_OK;
}
