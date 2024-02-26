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

#ifndef DFX_NONLINUX_DEFINE_H
#define DFX_NONLINUX_DEFINE_H

#include <stdint.h>

#ifndef PROT_READ
#define PROT_READ	0x1 // Page can be read.
#endif

#ifndef PROT_WRITE
#define PROT_WRITE	0x2 // Page can be written.
#endif

#ifndef PROT_EXEC
#define PROT_EXEC	0x4 // Page can be executed.
#endif

#ifndef PROT_NONE
#define PROT_NONE	0x0 // Page can not be accessed.
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((void *) -1)
#endif

#if defined __x86_64__ && !defined __ILP32__
# define __WORDSIZE	64
#elif defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64
# define __WORDSIZE	64
#else
# define __WORDSIZE	32
#endif

#define __ELF_NATIVE_CLASS __WORDSIZE

#define ElfW(type)	_ElfW (Elf, __ELF_NATIVE_CLASS, type)
#define _ElfW(e, w, t)	_ElfW_1 (e, w, _##t)
#define _ElfW_1(e, w, t)	e##w##t

#define EM_386		 3 // Intel 80386
#define EM_ARM		40 // ARM
#define EM_X86_64	62 // AMD x86-64 architecture
#define EM_AARCH64	183 // ARM AARCH64
#define EM_RISCV	243 // RISCV

#define PT_LOAD		1 // Loadable program segment
#define PT_DYNAMIC	2 // Dynamic linking information

#define PF_X (1 << 0) // Segment is executable

#define SHT_SYMTAB 2 // Symbol table
#define SHT_DYNSYM 11 // Dynamic linker symbol table

#define DT_NULL		0 // Marks end of dynamic section
#define DT_PLTGOT	3 // Processor defined value
#define DT_STRTAB	5 // Address of string table
#define DT_STRSZ	10 // Size of string table
#define DT_SONAME	14 // Name of shared object

#define SHN_UNDEF	0 // Undefined section

#define STT_FUNC	2 // Symbol is a code object
#define STT_GNU_IFUNC	10 // Symbol is indirect code object

#define NT_GNU_BUILD_ID	3

#define ELF32_ST_TYPE(val)		((val) & 0xf)

/* Type for a 16-bit quantity. */
typedef uint16_t Elf32_Half;
typedef uint16_t Elf64_Half;

/* Types for signed and unsigned 32-bit quantities. */
typedef uint32_t Elf32_Word;
typedef	int32_t Elf32_Sword;
typedef uint32_t Elf64_Word;
typedef	int32_t Elf64_Sword;

/* Types for signed and unsigned 64-bit quantities. */
typedef uint64_t Elf32_Xword;
typedef	int64_t Elf32_Sxword;
typedef uint64_t Elf64_Xword;
typedef	int64_t Elf64_Sxword;

/* Type of addresses. */
typedef uint32_t Elf32_Addr;
typedef uint64_t Elf64_Addr;

/* Type of file offsets. */
typedef uint32_t Elf32_Off;
typedef uint64_t Elf64_Off;

/* Type for section indices, which are 16-bit quantities. */
typedef uint16_t Elf32_Section;
typedef uint16_t Elf64_Section;

/* Type for version symbol information. */
typedef Elf32_Half Elf32_Versym;
typedef Elf64_Half Elf64_Versym;

#define EI_NIDENT (16)

typedef struct {
    unsigned char   e_ident[EI_NIDENT]; // Magic number and other info
    Elf32_Half      e_type; // Object file type
    Elf32_Half      e_machine; // Architecture
    Elf32_Word      e_version; // Object file version
    Elf32_Addr      e_entry; // Entry point virtual address
    Elf32_Off       e_phoff; // Program header table file offset
    Elf32_Off       e_shoff; // Section header table file offset
    Elf32_Word      e_flags; // Processor-specific flags
    Elf32_Half      e_ehsize; // ELF header size in bytes
    Elf32_Half      e_phentsize; // Program header table entry size
    Elf32_Half      e_phnum; // Program header table entry count
    Elf32_Half      e_shentsize; // Section header table entry size
    Elf32_Half      e_shnum; // Section header table entry count
    Elf32_Half      e_shstrndx; // Section header string table index
} Elf32_Ehdr;

typedef struct {
    unsigned char   e_ident[EI_NIDENT]; // Magic number and other info
    Elf64_Half      e_type; // Object file type
    Elf64_Half      e_machine; // Architecture
    Elf64_Word      e_version; // Object file version
    Elf64_Addr      e_entry; // Entry point virtual address
    Elf64_Off       e_phoff; // Program header table file offset
    Elf64_Off       e_shoff; // Section header table file offset
    Elf64_Word      e_flags; // Processor-specific flags
    Elf64_Half      e_ehsize; // ELF header size in bytes
    Elf64_Half      e_phentsize; // Program header table entry size
    Elf64_Half      e_phnum; // Program header table entry count
    Elf64_Half      e_shentsize; // Section header table entry size
    Elf64_Half      e_shnum; // Section header table entry count
    Elf64_Half      e_shstrndx; // Section header string table index
} Elf64_Ehdr;

typedef struct {
    Elf32_Word	p_type; // Segment type
    Elf32_Off	p_offset; // Segment file offset
    Elf32_Addr	p_vaddr; // Segment virtual address
    Elf32_Addr	p_paddr; // Segment physical address
    Elf32_Word	p_filesz; // Segment size in file
    Elf32_Word	p_memsz; // Segment size in memory
    Elf32_Word	p_flags; // Segment flags
    Elf32_Word	p_align; // Segment alignment
} Elf32_Phdr;

typedef struct {
    Elf64_Word	p_type; // Segment type
    Elf64_Word	p_flags; // Segment flags
    Elf64_Off	p_offset; // Segment file offset
    Elf64_Addr	p_vaddr; // Segment virtual address
    Elf64_Addr	p_paddr; // Segment physical address
    Elf64_Xword	p_filesz; // Segment size in file
    Elf64_Xword	p_memsz; // Segment size in memory
    Elf64_Xword	p_align; // Segment alignment
} Elf64_Phdr;

typedef struct {
    Elf32_Word	sh_name; // Section name (string tbl index)
    Elf32_Word	sh_type; // Section type
    Elf32_Word	sh_flags; // Section flags
    Elf32_Addr	sh_addr;  // Section virtual addr at execution
    Elf32_Off	sh_offset; // Section file offset
    Elf32_Word	sh_size; // Section size in bytes
    Elf32_Word	sh_link; // Link to another section
    Elf32_Word	sh_info;  // Additional section information
    Elf32_Word	sh_addralign; // Section alignment
    Elf32_Word	sh_entsize; // Entry size if section holds table
} Elf32_Shdr;

typedef struct {
    Elf64_Word	sh_name; // Section name (string tbl index)
    Elf64_Word	sh_type; // Section type
    Elf64_Xword	sh_flags; // Section flags
    Elf64_Addr	sh_addr; // Section virtual addr at execution
    Elf64_Off	sh_offset; // Section file offset
    Elf64_Xword	sh_size; // Section size in bytes
    Elf64_Word	sh_link; // Link to another section
    Elf64_Word	sh_info; // Additional section information
    Elf64_Xword	sh_addralign; // Section alignment
    Elf64_Xword	sh_entsize; // Entry size if section holds table
} Elf64_Shdr;

typedef struct {
    Elf32_Sword	   d_tag; // Dynamic entry type
    union {
        Elf32_Word d_val; // Integer value
        Elf32_Addr d_ptr; // Address value
    } d_un;
} Elf32_Dyn;

typedef struct {
    Elf64_Sxword	d_tag; // Dynamic entry type
    union {
        Elf64_Xword d_val; // Integer value
        Elf64_Addr  d_ptr; // Address value
    } d_un;
} Elf64_Dyn;

typedef struct {
    Elf32_Word      st_name; // Symbol name (string tbl index)
    Elf32_Addr      st_value; // Symbol value
    Elf32_Word      st_size; // Symbol size
    unsigned char   st_info; // Symbol type and binding
    unsigned char   st_other; // Symbol visibility
    Elf32_Section   st_shndx; // Section index
} Elf32_Sym;

typedef struct {
    Elf64_Word      st_name; // Symbol name (string tbl index)
    unsigned char	st_info; // Symbol type and binding
    unsigned char   st_other; // Symbol visibility
    Elf64_Section   st_shndx; // Section index
    Elf64_Addr      st_value; // Symbol value
    Elf64_Xword	    st_size; // Symbol size
} Elf64_Sym;

typedef struct {
    Elf32_Word n_namesz; // Length of the note's name.
    Elf32_Word n_descsz; // Length of the note's descriptor.
    Elf32_Word n_type; // Type of the note.
} Elf32_Nhdr;

typedef struct {
    Elf64_Word n_namesz; // Length of the note's name.
    Elf64_Word n_descsz; // Length of the note's descriptor.
    Elf64_Word n_type; // Type of the note.
} Elf64_Nhdr;

#define	ELFMAG  "\177ELF"
#define	SELFMAG	 4

#define EI_CLASS  4 // File class byte index
#define ELFCLASSNONE  0 // Invalid class
#define ELFCLASS32	1 // 32-bit objects
#define ELFCLASS64	2 // 64-bit objects

/* Sharing types (must choose one and only one of these). */
#define MAP_SHARED	0x01 // Share changes.
#define MAP_PRIVATE	0x02 // Changes are private.

#endif
