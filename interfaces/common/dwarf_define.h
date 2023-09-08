/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef DWARF_DEFINE_H
#define DWARF_DEFINE_H

#include <cinttypes>
#include <string>

namespace OHOS {
namespace HiviewDFX {
#define DW_EH_VERSION           1
#define DW_EH_PE_FORMAT_MASK    0x0f    /* format of the encoded value */
#define DW_EH_PE_APPL_MASK      0x70    /* how the value is to be applied */

// https://dwarfstd.org/doc/DWARF5.pdf
// 7.7.1 DWARF Expressions
// Each operation is a 1-byte code that identifies that operation, followed by zero or more bytes of additional data
// Table 7.9: DWARF operation encodings
typedef enum {
    DW_OP_reserved1 = 0x01,
    DW_OP_reserved2,
    DW_OP_addr,  // constant address (size is target specific)
    DW_OP_reserved4,
    DW_OP_reserved5,
    DW_OP_deref,
    DW_OP_reserved7,
    DW_OP_const1u,  // 1-byte unsigned integer constant
    DW_OP_const1s,  // 1-byte signed integer constant
    DW_OP_const2u,  // 2-byte unsigned integer constant
    DW_OP_const2s,  // 2-byte signed integer constant
    DW_OP_const4u,  // 4-byte unsigned integer constant
    DW_OP_const4s,  // 4-byte signed integer constant
    DW_OP_const8u,  // 8-byte unsigned integer constant
    DW_OP_const8s,  // 8-byte signed integer constant
    DW_OP_constu,   // unsigned LEB128 integer constant
    DW_OP_consts,   // signed LEB128 integer constant
    DW_OP_dup,
    DW_OP_drop,
    DW_OP_over,
    DW_OP_pick,  // 1-byte stack index
    DW_OP_swap,
    DW_OP_rot,
    DW_OP_xderef,
    DW_OP_abs,
    DW_OP_and,
    DW_OP_div,
    DW_OP_minus,
    DW_OP_mod,
    DW_OP_mul,
    DW_OP_neg,
    DW_OP_not,
    DW_OP_or,
    DW_OP_plus,
    DW_OP_plus_uconst,  // ULEB128 addend
    DW_OP_shl,
    DW_OP_shr,
    DW_OP_shra,
    DW_OP_xor,
    DW_OP_bra,  // signed 2-byte constant
    DW_OP_eq,
    DW_OP_ge,
    DW_OP_gt,
    DW_OP_le,
    DW_OP_lt,
    DW_OP_ne,
    DW_OP_skip,
    DW_OP_lit0 = 0x30,  // literals 0 .. 31 = (DW_OP_lit0 + literal)
    DW_OP_lit1,
    DW_OP_lit2,
    DW_OP_lit3,
    DW_OP_lit4,
    DW_OP_lit5,
    DW_OP_lit6,
    DW_OP_lit7,
    DW_OP_lit8,
    DW_OP_lit9,
    DW_OP_lit10,
    DW_OP_lit11,
    DW_OP_lit12,
    DW_OP_lit13,
    DW_OP_lit14,
    DW_OP_lit15,
    DW_OP_lit16,
    DW_OP_lit17,
    DW_OP_lit18,
    DW_OP_lit19,
    DW_OP_lit20,
    DW_OP_lit21,
    DW_OP_lit22,
    DW_OP_lit23,
    DW_OP_lit24,
    DW_OP_lit25,
    DW_OP_lit26,
    DW_OP_lit27,
    DW_OP_lit28,
    DW_OP_lit29,
    DW_OP_lit30,
    DW_OP_lit31,
    DW_OP_reg0 = 0x50,  // reg 0 .. 31 = (DW_OP_reg0 + regnum)
    DW_OP_reg1,
    DW_OP_reg2,
    DW_OP_reg3,
    DW_OP_reg4,
    DW_OP_reg5,
    DW_OP_reg6,
    DW_OP_reg7,
    DW_OP_reg8,
    DW_OP_reg9,
    DW_OP_reg10,
    DW_OP_reg11,
    DW_OP_reg12,
    DW_OP_reg13,
    DW_OP_reg14,
    DW_OP_reg15,
    DW_OP_reg16,
    DW_OP_reg17,
    DW_OP_reg18,
    DW_OP_reg19,
    DW_OP_reg20,
    DW_OP_reg21,
    DW_OP_reg22,
    DW_OP_reg23,
    DW_OP_reg24,
    DW_OP_reg25,
    DW_OP_reg26,
    DW_OP_reg27,
    DW_OP_reg28,
    DW_OP_reg29,
    DW_OP_reg30,
    DW_OP_reg31,
    DW_OP_breg0 = 0x70,  // SLEB128 offset base register 0 .. 31 = (DW_OP_breg0 + regnum)
    DW_OP_breg1,
    DW_OP_breg2,
    DW_OP_breg3,
    DW_OP_breg4,
    DW_OP_breg5,
    DW_OP_breg6,
    DW_OP_breg7,
    DW_OP_breg8,
    DW_OP_breg9,
    DW_OP_breg10,
    DW_OP_breg11,
    DW_OP_breg12,
    DW_OP_breg13,
    DW_OP_breg14,
    DW_OP_breg15,
    DW_OP_breg16,
    DW_OP_breg17,
    DW_OP_breg18,
    DW_OP_breg19,
    DW_OP_breg20,
    DW_OP_breg21,
    DW_OP_breg22,
    DW_OP_breg23,
    DW_OP_breg24,
    DW_OP_breg25,
    DW_OP_breg26,
    DW_OP_breg27,
    DW_OP_breg28,
    DW_OP_breg29,
    DW_OP_breg30,
    DW_OP_breg31,
    DW_OP_regx,         // ULEB128 register
    DW_OP_fbreg,        // SLEB128 offset
    DW_OP_bregx,        // ULEB128 register, SLEB128 offset
    DW_OP_piece,        // ULEB128 size of piece
    DW_OP_deref_size,   // 1-byte size of data retrieved
    DW_OP_xderef_size,  // 1-byte size of data retrieved
    DW_OP_nop,
    DW_OP_push_object_address,
    DW_OP_call2,     // 2-byte offset of DIE
    DW_OP_call4,     // 4-byte offset of DIE
    DW_OP_call_ref,  // 4- or 8-byte offset of DIE
    // dwarf4 added
    DW_OP_form_tls_address,
    DW_OP_call_frame_cfa,
    DW_OP_bit_piece,       // ULEB128 size, ULEB128 offset
    DW_OP_implicit_value,  // ULEB128 size, block of that size
    DW_OP_stack_value,
    // dwarf5 added
    DW_OP_implicit_pointer,  // 4- or 8-byte offset of DIE, SLEB128 constant offset
    DW_OP_addrx,             // ULEB128 indirect address
    DW_OP_constx,            // ULEB128 indirect constant
    DW_OP_entry_value,       // ULEB128 size, block of that size
    DW_OP_const_type,        // ULEB128 type entry offset, 1-byte size, constant value
    DW_OP_regval_type,       // ULEB128 register number, ULEB128 constant offset
    DW_OP_deref_type,        // 1-byte size, ULEB128 type entry offset
    DW_OP_xderef_type,       // 1-byte size, ULEB128 type entry offset
    DW_OP_convert_type,      // ULEB128 type entry offset
    DW_OP_reinterpret_type,  // ULEB128 type entry offset
    DW_OP_lo_user = 0xe0,
    DW_OP_hi_user = 0xff
} DwarfOperater;

// DWARF Call Frame Information
// Call frame instructions are encoded in one or more bytes. The primary opcode is
// encoded in the high order two bits of the first byte (that is, opcode = byte >> 6).
// An operand or extended opcode may be encoded in the low order 6 bits.
// Table 7.29: Call frame instruction encodings
typedef enum {
    DW_CFA_advance_loc = 0x40,  // high 2-bits 0x1, low 6-bits delta
    DW_CFA_offset = 0x80,       // high 2-bits 0x2, low 6-bits register, ULEB128 offset
    DW_CFA_restore = 0xc0,      // high 2-bits 0x3, low 6-bits register
    // the value is the lower 6-bits, Operand1, Operand2
    DW_CFA_nop = 0x00,
    DW_CFA_set_loc,           // address
    DW_CFA_advance_loc1,      // 1-byte delta
    DW_CFA_advance_loc2,      // 2-byte delta
    DW_CFA_advance_loc4,      // 4-byte delta
    DW_CFA_offset_extended,   // ULEB128 register, ULEB128 offset
    DW_CFA_restore_extended,  // ULEB128 register
    DW_CFA_undefined,         // ULEB128 register
    DW_CFA_same_value,        // ULEB128 register
    DW_CFA_register,          // ULEB128 register, ULEB128 offset
    DW_CFA_remember_state,
    DW_CFA_restore_state,
    DW_CFA_def_cfa,             // ULEB128 register, ULEB128 offset
    DW_CFA_def_cfa_register,    // ULEB128 register
    DW_CFA_def_cfa_offset,      // ULEB128 offset
    DW_CFA_def_cfa_expression,  // BLOCK
    DW_CFA_expression = 0x10,   // ULEB128 register, BLOCK
    DW_CFA_offset_extended_sf,  // ULEB128 register, SLEB128 offset
    DW_CFA_def_cfa_sf,          // ULEB128 register, SLEB128 offset
    DW_CFA_def_cfa_offset_sf,   // SLEB128 offset
    DW_CFA_val_offset,          // ULEB128, ULEB128
    DW_CFA_val_offset_sf,       // ULEB128, SLEB128
    DW_CFA_val_expression,      // ULEB128, BLOCK

    DW_CFA_lo_user = 0x1c,
    // GNU extensions
    DW_CFA_GNU_args_size = 0x2e,
    DW_CFA_GNU_negative_offset_extended = 0x2f,

    DW_CFA_hi_user = 0x3c,
} DwarfCfa;

enum DwarfEncoding : uint8_t {
    DW_EH_PE_omit = 0xff,

    DW_EH_PE_absptr = 0x00,
    DW_EH_PE_uleb128 = 0x01,
    DW_EH_PE_udata2 = 0x02,
    DW_EH_PE_udata4 = 0x03,
    DW_EH_PE_udata8 = 0x04,
    DW_EH_PE_sleb128 = 0x09,
    DW_EH_PE_sdata2 = 0x0a,
    DW_EH_PE_sdata4 = 0x0b,
    DW_EH_PE_sdata8 = 0x0c,

    DW_EH_PE_pcrel = 0x10,
    DW_EH_PE_textrel = 0x20,
    DW_EH_PE_datarel = 0x30,
    DW_EH_PE_funcrel = 0x40,
    DW_EH_PE_aligned = 0x50,
    DW_EH_PE_indirect = 0x80,

    DW_EH_PE_udata1 = 0x0d,
    DW_EH_PE_sdata1 = 0x0e,
    DW_EH_PE_block = 0x0f,
};

// we only need to parse eh_frame and eh_frame_hdr to get the dwarf-encoded unwind info
// https://refspecs.linuxbase.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
// Parsed Common Information Entry Format
typedef struct {
    uintptr_t cieStart;
    uintptr_t cieEnd;
    uint32_t codeAlignFactor;
    int32_t dataAlignFactor;
    uintptr_t returnAddressRegister;
    bool hasAugmentationData;
    uintptr_t instructions;  // need instruction end ?
    // P
    uint8_t personality;
    uint8_t personalityEncoding;
    // L
    uint8_t lsdaEncoding;
    // R
    uint8_t pointerEncoding;
    uint8_t segmentSize;
} CommonInfoEntry;

// Parsed Frame Description Entry
// Table 8-3. Frame Description Entry Format
typedef struct {
    uintptr_t fdeStart;
    uintptr_t fdeEnd;
    uintptr_t pcStart;
    uintptr_t pcEnd;
    uintptr_t lsda;
    uintptr_t instructions;  // need instruction end ?
    uintptr_t cieAddr;
} FrameDescEntry;

} // namespace HiviewDFX
} // namespace OHOS
#endif
