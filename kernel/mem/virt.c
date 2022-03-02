#include <stdbool.h>

#include "../utils/print.h"
#include "../utils/utils.h"
#include "virt.h"

static page_table_t *kernel_pt;
static vaddr_t hhdm_offset;

static page_table_t *get_next_pt(page_table_t *pt, size_t index, bool create_if_missing) {
  if (!pt)
    return NULL;

  if (IS_SET(pt->entries[index], PTE_P))
    return (page_table_t *) (hhdm_offset + (pt->entries[index] & PTE_ADDR_MASK));

  if (!create_if_missing)
    return NULL;

  paddr_t phys_addr = phys_alloc(1);

  pt->entries[index] = PTE_P | PTE_W | PTE_U | phys_addr;

  return (page_table_t *) (hhdm_offset + phys_addr);
}

static void pt_map_single(page_table_t *pt, vaddr_t virt, paddr_t phys, uint64_t flags) {
  size_t pml4_index = (virt >> 39) & 0x1ff;
  size_t pml3_index = (virt >> 30) & 0x1ff;
  size_t pml2_index = (virt >> 21) & 0x1ff;
  size_t pml1_index = (virt >> 12) & 0x1ff;

  page_table_t *pml3 = get_next_pt(pt, pml4_index, true);
  page_table_t *pml2 = get_next_pt(pml3, pml3_index, true);
  page_table_t *pml1 = get_next_pt(pml2, pml2_index, true);

  if (IS_SET(pml1->entries[pml1_index], PTE_P))
    klog_panic("Tried to map a virtual address %p that's already mapped to %p", virt, pml1->entries[pml1_index] & PTE_ADDR_MASK);

  pml1->entries[pml1_index] = flags | phys;
}

static void pt_unmap_single(page_table_t *pt, vaddr_t virt) {
  size_t pml4_index = (virt >> 39) & 0x1ff;
  size_t pml3_index = (virt >> 30) & 0x1ff;
  size_t pml2_index = (virt >> 21) & 0x1ff;
  size_t pml1_index = (virt >> 12) & 0x1ff;

  page_table_t *pml3 = get_next_pt(pt, pml4_index, false);
  page_table_t *pml2 = get_next_pt(pml3, pml3_index, false);
  page_table_t *pml1 = get_next_pt(pml2, pml2_index, false);

  if (!pml1 || IS_CLEAR(pml1->entries[pml1_index], PTE_P))
    klog_panic("Tried to unmap a virtual address %p that isn't mapped", virt);

  pml1->entries[pml1_index] = 0;

  asm("invlpg (%0)" : : "r"(virt));
}

static void pt_map(page_table_t *pt, vaddr_t virt, paddr_t phys, size_t size, uint64_t flags) {
  assert_msg((virt & 0xfff) == 0, "Virtual address must be page aligned");
  assert_msg((phys & 0xfff) == 0, "Physical address must be page aligned");
  assert_msg((size & 0xfff) == 0, "Size must be expressed in pages");

  for (size_t i = 0; i < size / 4096; i++) {
    pt_map_single(pt, virt + i * 4096, phys + i * 4096, flags);
  }
}

static void pt_unmap(page_table_t *pt, vaddr_t virt, size_t size) {
  assert_msg((virt & 0xfff) == 0, "Virtual address must be page aligned");
  assert_msg((size & 0xfff) == 0, "Size must be expressed in pages");

  for (size_t i = 0; i < size / 4096; i++) {
    pt_unmap_single(pt, virt + i * 4096);
  }
}

void vm_map(address_space_t *vm, vaddr_t virt, paddr_t phys, size_t size, uint64_t flags) {
  pt_map(vm->pt, virt, phys, size, flags);
}

void vm_unmap(address_space_t *vm, vaddr_t virt, size_t size) {
  pt_unmap(vm->pt, virt, size);
}

void vm_switch(address_space_t *vm) {
  asm("mov %0, %%cr3" : : "r"((vaddr_t) vm->pt - hhdm_offset));
}

address_space_t *virt_new_vm() {
  return NULL;
}

void virt_init(struct stivale2_struct_tag_pmrs *pmrs_tag,                       //
               struct stivale2_struct_tag_kernel_base_address *kernel_base_tag, //
               struct stivale2_struct_tag_hhdm *hhdm_tag)                       //
{
  hhdm_offset = hhdm_tag->addr;
  kernel_pt = (page_table_t *) (hhdm_offset + phys_alloc(1));

  pt_map(kernel_pt, 0, 0, GIB(4), PTE_P | PTE_W);
  pt_map(kernel_pt, hhdm_offset, 0, GIB(4), PTE_P | PTE_W);

  for (size_t i = 0; i < pmrs_tag->entries; i++) {
    struct stivale2_pmr *pmr = &pmrs_tag->pmrs[i];

    uint64_t map_flags = PTE_P;

    if (IS_SET(pmr->permissions, STIVALE2_PMR_WRITABLE))
      map_flags |= PTE_W;

    if (IS_CLEAR(pmr->permissions, STIVALE2_PMR_EXECUTABLE))
      map_flags |= PTE_NX;

    paddr_t phys_addr = kernel_base_tag->physical_base_address + (pmr->base - kernel_base_tag->virtual_base_address);

    pt_map(kernel_pt, pmr->base, phys_addr, ALIGN_UP(pmr->length, 4096), map_flags);
  }

  asm("mov %0, %%cr3" : : "r"((vaddr_t) kernel_pt - hhdm_offset));
}
