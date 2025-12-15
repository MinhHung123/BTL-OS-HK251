/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid,
            addr_t size, addr_t *alloc_addr)
{
  if (size <= 0 || caller == NULL || caller->mm == NULL || alloc_addr == NULL)
    return -1;

  pthread_mutex_lock(&mmvm_lock);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (cur_vma == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct chosen;
  /* ưu tiên lấy từ danh sách free region trước */
  if (get_free_vmrg_area(caller, vmaid, size, &chosen) == 0) {
    caller->mm->symrgtbl[rgid].rg_start = chosen.rg_start;
    caller->mm->symrgtbl[rgid].rg_end   = chosen.rg_end;
    *alloc_addr = chosen.rg_start;
    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* không đủ free region -> tăng sbrk */
  addr_t grow_bytes;
#ifdef MM64
  grow_bytes = ((size + PAGING64_PAGESZ - 1) / PAGING64_PAGESZ) * PAGING64_PAGESZ;
#else
  grow_bytes = PAGING_PAGE_ALIGNSZ(size);
#endif

  addr_t old_brk = cur_vma->sbrk;

  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
  regs.a3 = grow_bytes;
  syscall(caller->krnl, caller->pid, 17, &regs);

  /* vùng user xin: [old_brk, old_brk + size) */
  caller->mm->symrgtbl[rgid].rg_start = old_brk;
  caller->mm->symrgtbl[rgid].rg_end   = old_brk + size;
  *alloc_addr = old_brk;

  /* phần dư (nếu có) trả vào free list */
  if (old_brk + size < old_brk + grow_bytes) {
    struct vm_rg_struct *extra =
      (struct vm_rg_struct*)malloc(sizeof(struct vm_rg_struct));
    extra->rg_start = old_brk + size;
    extra->rg_end   = old_brk + grow_bytes;
    extra->rg_next  = cur_vma->vm_freerg_list;
    cur_vma->vm_freerg_list = extra;
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  /* vmaid hiện không dùng nhưng giữ nguyên tham số để đúng prototype */
  (void)vmaid;

  pthread_mutex_lock(&mmvm_lock);

  /* kiểm tra input cơ bản */
  if (caller == NULL || caller->mm == NULL ||
      rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* TODO: Manage the collect freed region to freerg_list */
  struct vm_rg_struct *rgnode = get_symrg_byid(caller->mm, rgid);

  /* chưa từng alloc hoặc đã free rồi */
  if (rgnode == NULL ||
      (rgnode->rg_start == 0 && rgnode->rg_end == 0))
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *freerg_node =
      (struct vm_rg_struct *)malloc(sizeof(struct vm_rg_struct));
  if (!freerg_node) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end   = rgnode->rg_end;
  freerg_node->rg_next  = NULL;

  /* reset entry trong symrgtbl */
  rgnode->rg_start = 0;
  rgnode->rg_end   = 0;
  rgnode->rg_next  = NULL;

  /* enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, freerg_node);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  addr_t  addr;

  int val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  /* By default using vmaid = 0 */
  return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  int val = __free(proc, 0, reg_index);
  if (val == -1)
  {
    return -1;
  }
printf("%s:%d\n",__func__,__LINE__);
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif
  return 0;//val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */

int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = pte_get_entry(caller, pgn);

  if (pte & PAGING_PTE_SWAPPED_MASK) {
  } else if (PAGING_PAGE_PRESENT(pte)) {
    *fpn = PAGING_FPN(pte);
    return 0;
  } else {
    return -1; /* invalid page */
  }
  
  addr_t req_swpfpn = PAGING_SWP(pte);   /* swap frame của trang pgn cần nạp */
  addr_t vicpgn, vicfpn;
  uint32_t vicpte;
  struct sc_regs regs;
  addr_t newfpn;

  if (MEMPHY_get_freefp(caller->krnl->mram, &newfpn) < 0) {
  
    if (find_victim_page(mm, &vicpgn) < 0)
      return -1;

    vicpte = pte_get_entry(caller, vicpgn);
    vicfpn = PAGING_FPN(vicpte);

    addr_t vic_swpfpn;
    if (MEMPHY_get_freefp(caller->krnl->active_mswp, &vic_swpfpn) < 0)
      return -1;

    regs.a1 = SYSMEM_SWP_OP;
    regs.a2 = vicfpn;        /* src in mram */
    regs.a3 = vic_swpfpn;    /* dst in swap */
    syscall(caller->krnl, caller->pid, 17, &regs);

    pte_set_swap(caller, vicpgn, caller->krnl->active_mswp_id, vic_swpfpn);

    newfpn = vicfpn;
  }

  __swap_cp_page(caller->krnl->active_mswp, req_swpfpn,
                 caller->krnl->mram, newfpn);

  MEMPHY_put_freefp(caller->krnl->active_mswp, req_swpfpn);

  pte_set_fpn(caller, pgn, newfpn);

  enlist_pgn_node(&mm->fifo_pgn, pgn);

  *fpn = newfpn;
  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */

int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = phyaddr;
  regs.a3 = 0;
  syscall(caller->krnl, caller->pid, 17, &regs);
  *data = (BYTE)(regs.a3 & 0xFF);
  return 0;
}



/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */


  /* TODO 
   *  MEMPHY_write(caller->krnl->mram, phyaddr, value);
   *  MEMPHY WRITE with SYSMEM_IO_WRITE 
   * SYSCALL 17 sys_memmap
   */
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = phyaddr;
  regs.a3 = (addr_t)value;
  syscall(caller->krnl, caller->pid, 17, &regs); /* SYSCALL 17 sys_memmap */
  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid,
           addr_t offset, BYTE *data)
{
  /* vmaid chưa dùng tới, giữ lại để đúng prototype */
  (void)vmaid;

  if (caller == NULL || caller->mm == NULL || data == NULL)
    return -1;

  /* Lấy region từ symbol table */
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  if (currg == NULL)
    return -1;

  /* Region chưa được cấp phát hoặc đã free */
  if (currg->rg_start == 0 && currg->rg_end == 0)
    return -1;

  /* Kiểm tra offset hợp lệ */
  addr_t rg_size = currg->rg_end - currg->rg_start;
  if (offset < 0 || offset >= rg_size)
    return -1;

  /* Đọc dữ liệu qua paging */
  if (pg_getval(caller->mm,
                currg->rg_start + offset,
                data,
                caller) != 0)
    return -1;

  return 0;
}



/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    addr_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  *destination = data;
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid,
            addr_t offset, BYTE value)
{
  pthread_mutex_lock(&mmvm_lock);

  if (caller == NULL || caller->mm == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* kiểm tra offset không vượt quá region */
  if (offset < 0 || currg->rg_start + offset >= currg->rg_end)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}


/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    addr_t offset)
{
  int val = __write(proc, 0, destination, offset, data);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->krnl->mram);
#endif

  return val;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */

int free_pcb_memph(struct pcb_t *caller)
{
  pthread_mutex_lock(&mmvm_lock);

  int pagenum;
  int fpn;
  uint32_t pte;

#ifdef MM64
  /* với 64-bit, phải đọc PTE qua pte_get_entry */
  for (pagenum = 0; pagenum < PAGING64_MAX_PGN; ++pagenum) {
    pte = pte_get_entry(caller, pagenum);
    if (pte == 0) continue;

    if (PAGING_PAGE_PRESENT(pte) && !(pte & PAGING_PTE_SWAPPED_MASK)) {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->krnl->mram, fpn);
    } else if (pte & PAGING_PTE_SWAPPED_MASK) {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
    }
  }
#else
  /* 1-level page table cũ */
  for (pagenum = 0; pagenum < PAGING_MAX_PGN; ++pagenum) {
    pte = caller->mm->pgd[pagenum];
    if (PAGING_PAGE_PRESENT(pte)) {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->krnl->mram, fpn);
    } else if (pte & PAGING_PTE_SWAPPED_MASK) {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
    }
  }
#endif

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}



/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */

int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  if (!mm || !retpgn)
    return -1;

  struct pgn_t *pg = mm->fifo_pgn;
  if (!pg)
    return -1;

  /* Nếu chỉ có 1 trang trong FIFO */
  if (pg->pg_next == NULL) {
    *retpgn = pg->pgn;
    mm->fifo_pgn = NULL;
    free(pg);
    return 0;
  }


  /* Nhiều hơn 1 trang  (FIFO oldest) */
  struct pgn_t *prev = NULL;
  while (pg->pg_next) {
    prev = pg;
    pg = pg->pg_next;
  }

  *retpgn = pg->pgn;
  prev->pg_next = NULL;
  free(pg);

  return 0;
}



/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller,
                       int vmaid,
                       int size,
                       struct vm_rg_struct *newrg)
{
  if (caller == NULL || caller->mm == NULL ||
      newrg == NULL || size <= 0)
    return -1;

  struct vm_area_struct *vma = get_vma_by_num(caller->mm, vmaid);
  if (vma == NULL)
    return -1;

  struct vm_rg_struct *rgit = vma->vm_freerg_list;
  if (rgit == NULL)
    return -1;

  newrg->rg_start = -1;
  newrg->rg_end   = -1;

  while (rgit != NULL) {
    if (rgit->rg_start + size <= rgit->rg_end) {
      /* cắt 1 đoạn từ đầu free region */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end   = rgit->rg_start + size;

      if (newrg->rg_end < rgit->rg_end) {
        /* vẫn còn dư ở free region này -> dời start lên */
        rgit->rg_start = newrg->rg_end;
      } else {
        /* dùng hết region này -> bỏ nó, lấy node kế tiếp thế vào */
        struct vm_rg_struct *nxt = rgit->rg_next;
        if (nxt) {
          rgit->rg_start = nxt->rg_start;
          rgit->rg_end   = nxt->rg_end;
          rgit->rg_next  = nxt->rg_next;
          free(nxt);
        } else {
          rgit->rg_start = rgit->rg_end;
          rgit->rg_next  = NULL;
        }
      }
      break;
    }
    rgit = rgit->rg_next;
  }

  if (newrg->rg_start == -1)
    return -1;
  return 0;
}


/*
 * Clean Paging Memory Library
 * CO2018 – LamiaAtrium
 */

// /*
/*  Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 *
 *  LamiaAtrium release - System Library
 *  Memory Module Library libmem.c
 *
 *  NOTE (MM64 + MMDBG):
 *  - Fix victim selection (avoid segfault when FIFO has 1 node)
 *  - Fix address->(pgn,off) + phyaddr calculation under MM64 (4KB pages)
 *  - Treat swapped page correctly even if PRESENT bit was set by older code
 *  - Add MMDBG traces (stderr + fflush) to see where it crashes
 */

/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

