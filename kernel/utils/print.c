#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "../arch/port.h"
#include "print.h"

static elf64_header_t *kernel_elf_header;
static elf64_section_header_t *kernel_symbol_table;
static elf64_section_header_t *kernel_string_table;

static void print_char(char ch) {
  port_out8(0x3f8, ch);
}

static void print_string_n(const char *string, size_t len) {
  for (size_t i = 0; i < len; i++) {
    port_out8(0x3f8, string[i]);
  }
}

static void print_string(const char *string) {
  print_string_n(string, strlen(string));
}

static void print_int(int64_t value) {
  char buffer[20] = {0};

  if (!value) {
    print_char('0');
  } else {
    bool sign = value < 0;

    if (sign)
      value = -value;

    int i = 0;

    for (i = 18; value; i--) {
      buffer[i] = (value % 10) + '0';
      value = value / 10;
    }

    if (sign)
      buffer[i] = '-';
    else
      i++;

    print_string(buffer + i);
  }
}

static void print_uint(uint64_t value) {
  char buffer[21] = {0};

  if (!value) {
    print_char('0');
  } else {
    int i = 0;

    for (i = 19; value; i--) {
      buffer[i] = (value % 10) + '0';
      value = value / 10;
    }

    print_string(buffer + i + 1);
  }
}

static void print_hex(uint64_t value) {
  char buffer[17] = {0};

  if (!value) {
    print_string("0x0");
  } else {
    int i;

    for (i = 15; value; i--) {
      buffer[i] = "0123456789abcdef"[value & 0xf];
      value = value >> 4;
    }

    print_string("0x");
    print_string(buffer + i + 1);
  }
}

static bool resolve_symbol(uint64_t rip, const char **name, uint64_t *offset) {
  if (!kernel_elf_header || !kernel_symbol_table || !kernel_string_table)
    goto error;

  elf64_symbol_t *symbols = (elf64_symbol_t *) ((uint64_t) kernel_elf_header + kernel_symbol_table->sh_offset);

  for (size_t i = 0; i < kernel_symbol_table->sh_size / kernel_symbol_table->sh_entsize; i++) {
    elf64_symbol_t *symbol = &symbols[i];

    if (rip < symbol->st_value || rip > symbol->st_value + symbol->st_size)
      continue;

    if (symbol->st_name < kernel_string_table->sh_size) {
      *name = (const char *) ((uint64_t) kernel_elf_header + kernel_string_table->sh_offset + symbol->st_name);
      *offset = rip - symbol->st_value;

      return true;
    }
  }

error:
  *name = NULL;
  *offset = 0;

  return false;
}

void panic_load_symbols(elf64_header_t *elf) {
  if (memcmp(elf->e_ident, ELFMAG, ELFMAGSZ) != 0)
    klog_panic("Given kernel ELF file is not valid");

  kernel_elf_header = elf;

  for (size_t i = 0; i < elf->e_shnum; i++) {
    elf64_section_header_t *section = (elf64_section_header_t *) ((uint64_t) elf + elf->e_shoff + elf->e_shentsize * i);

    if (section->sh_type == SHT_SYMTAB && !kernel_symbol_table) {
      kernel_symbol_table = section;
    } else if (section->sh_type == SHT_STRTAB && kernel_symbol_table && kernel_symbol_table->sh_link == i) {
      kernel_string_table = section;
    }
  }

  assert(kernel_symbol_table);
  assert(kernel_string_table);
}

void panic_backtrace() {
  typedef struct {
    uint64_t rbp;
    uint64_t rip;
  } stack_frame_t;

  stack_frame_t *stack_frame;

  asm("mov %%rbp, %0" : "=r"(stack_frame) : : "memory");

  while (stack_frame && stack_frame->rip) {
    const char *name;

    uint64_t offset;

    if (resolve_symbol(stack_frame->rip, &name, &offset)) {
      klog_error("Backtrace: %p [%s+%p]", stack_frame->rip, name, offset);
    } else {
      klog_error("Backtrace: %p [?]", stack_frame->rip);
    }

    stack_frame = (stack_frame_t *) stack_frame->rbp;
  }

  while (1) {
    asm("cli");
    asm("hlt");
  }
}

void println(const char *format, ...) {
  va_list args;

  va_start(args, format);

  while (1) {
    while (*format && *format != '%') {
      print_char(*format++);
    }

    if (!*format++)
      break;

    switch (*format++) {
    case 's': {
      char *str = va_arg(args, char *);

      print_string(str ? str : "(null)");
    }; break;
    case 'S': {
      char *str = va_arg(args, char *);

      if (str)
        print_string_n(str, va_arg(args, size_t));
      else
        print_string("(null)");
    }; break;
    case 'd':
      print_int(va_arg(args, int32_t));
      break;
    case 'u':
      print_uint(va_arg(args, uint32_t));
      break;
    case 'x':
      print_hex(va_arg(args, uint32_t));
      break;
    case 'D':
      print_int(va_arg(args, int64_t));
      break;
    case 'U':
      print_uint(va_arg(args, uint64_t));
      break;
    case 'X':
      print_hex(va_arg(args, uint64_t));
      break;
    case 'p':
      print_hex(va_arg(args, uintptr_t));
      break;
    case 'c':
      print_char((char) va_arg(args, int));
      break;
    default:
      print_char('?');
      break;
    }
  }

  va_end(args);

  print_char('\n');
}