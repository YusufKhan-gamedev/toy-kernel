#include "arch/idt.h"
#include "boot/stivale2.h"
#include "mem/phys.h"
#include "utils/print.h"

__attribute__((aligned(16))) //
static uint8_t stack[8192];

static struct stivale2_header_tag_framebuffer framebuffer_tag = {
  .tag = {.identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID, .next = 0},
  .framebuffer_width = 0,
  .framebuffer_height = 0,
  .framebuffer_bpp = 0,
};

__attribute__((section(".stivale2hdr"), used)) //
static struct stivale2_header stivale_hdr = {
  .entry_point = 0,
  .stack = (uint64_t) stack + sizeof(stack),
  .flags = 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4,
  .tags = (uint64_t) &framebuffer_tag,
};

static void *find_tag(struct stivale2_struct *boot_info, uint64_t id) {
  struct stivale2_tag *tag = (void *) boot_info->tags;

  while (tag) {
    if (tag->identifier == id)
      return tag;

    tag = (void *) tag->next;
  }

  return tag;
}

void kernel_main(struct stivale2_struct *boot_info) {
  struct stivale2_struct_tag_kernel_file *kernel_file = find_tag(boot_info, STIVALE2_STRUCT_TAG_KERNEL_FILE_ID);

  if (kernel_file)
    panic_load_symbols((elf64_header_t *) kernel_file->kernel_file);

  idt_init();
  phys_init(find_tag(boot_info, STIVALE2_STRUCT_TAG_MEMMAP_ID));

  while (1) {
    asm("hlt");
  }
}
