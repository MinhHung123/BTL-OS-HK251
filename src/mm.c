/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */
 
 /* NOTICE this moudle is deprecated in LamiaAtrium release
  *        the structure is maintained for future 64bit-32bit
  *        backward compatible feature or PAE feature 
  */
 
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !defined(MM64)
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(addr_t *pte,
             int pre,    // present
             addr_t fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             addr_t swpoff) // swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0)
        return -1;  // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}

/*
 * get_pd_from_pagenum - Parse address to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
  printf("[ERROR] %s: This feature 32 bit mode is deprecated\n", __func__);
  return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
  printf("[ERROR] %s: This feature 32 bit mode is deprecated\n", __func__);
  return 0;
}

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
  struct krnl_t *krnl = caller->krnl;
  addr_t *pte = &krnl->mm->pgd[pgn];
	
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
  struct krnl_t *krnl = caller->krnl;
  addr_t *pte = &krnl->mm->pgd[pgn];

  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}


/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
  struct krnl_t *krnl = caller->krnl;
  return krnl->mm->pgd[pgn];
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
	struct krnl_t *krnl = caller->krnl;
	krnl->mm->pgd[pgn]=pte_val;
	
	return 0;
}

/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum)                      // num of mapping page
{
  printf("[ERROR] %s: This feature 32 bit mode is deprecated\n", __func__);
  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum,                      // num of mapping page
                    struct framephy_struct *frames, // list of the mapped frames
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{                                                   // no guarantee all given pages are mapped
  addr_t a = addr;
  struct framephy_struct *fp = frames;
  for (int i = 0; i < pgnum; i++)
  {
      if (!fp) return -1;

      addr_t pgn = PAGING_PGN(a);
      pte_set_fpn(caller, pgn, fp->fpn);

      fp = fp->fp_next;
      a += PAGING_PAGESZ;
  }

  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
  struct memphy_struct *mram = caller->krnl->mram;
  *frm_lst = NULL;
  for(int i=0; i<req_pgnum; i++) {
    addr_t fpn;
    if(MEMPHY_get_freefp(mram, &fpn) <0) {
      /* Failed allocation, release all obtained frames */
      // struct framephy_struct *iter = *frm_lst;
      // struct framephy_struct *tmp;
      // while(iter != NULL) {
      //   tmp = iter;
      //   iter = iter->fp_next;
      //   MEMPHY_release_fp(mram, tmp->fpn);
      // free(tmp);
      // }
      // *frm_lst = NULL;
      return -1;
    }
    struct framephy_struct *newfp = malloc(sizeof(struct framephy_struct));
    newfp->fpn = fpn;
    newfp->fp_next = *frm_lst;
    newfp->owner = caller->krnl->mm;
    *frm_lst = newfp;
  }
  return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_ram(struct pcb_t *caller,
                  addr_t astart, addr_t aend,
                  addr_t mapstart, int incpgnum,
                  struct vm_rg_struct *ret_rg)
{
    struct framephy_struct *frame = NULL;

    /* Cấp phát incpgnum frame vật lý */
    if (alloc_pages_range(caller, incpgnum, &frame) < 0)
        return -1;

    /* Map các frame này vào vùng ảo bắt đầu tại astart */
    if (vmap_page_range(caller, astart, incpgnum, frame, ret_rg) < 0)
        return -1;

    return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
  for(int i=0; i<PAGING_PAGESZ; i++) {
    BYTE val;
    MEMPHY_read(mpsrc, srcfpn * PAGING_PAGESZ + i, &val);
    MEMPHY_write(mpdst, dstfpn * PAGING_PAGESZ + i, val);
  }
  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  mm->pgd = (uint32_t *)calloc(PAGING_MAX_PGN, sizeof(uint32_t));
  if(!mm->pgd) return -1;
  struct vm_area_struct *vma0 = (struct vm_area_struct *)malloc(sizeof(struct vm_area_struct));
  if(!vma0) return -1;
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = 0;
  vma0->sbrk = 0;
  vma0->vm_next = NULL;
  vma0->vm_freerg_list = NULL;
  mm->mmap = vma0;
  mm->fifo_pgn = NULL;
  memset(mm->symrgtbl, 0, sizeof(mm->symrgtbl));
  return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
  struct vm_rg_struct *rg = malloc(sizeof(struct vm_rg_struct));
  if(!rg) return NULL;
  rg->rg_start = rg_start;
  rg->rg_end = rg_end;
  rg->rg_next = NULL;
  return rg;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;
  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
  struct pgn_t *newnode = malloc(sizeof(struct pgn_t));
  if(!newnode) return -1;
  newnode->pgn = pgn;
  newnode->pg_next = *plist;
  *plist = newnode;
  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  printf("[ERROR] %s: This feature 32 bit mode is deprecated\n", __func__);
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  printf("[ERROR] %s: This feature 32 bit mode is deprecated\n", __func__);
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  printf("[ERROR] %s: This feature 32 bit mode is deprecated\n", __func__);
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("[ERROR] %s: This feature 32 bit mode is deprecated\n", __func__);
  return 0;
}

int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
    struct mm_struct *mm = caller->krnl->mm;

    /* Nếu end = -1 hoặc lớn hơn số page tối đa, cắt về PAGING_MAX_PGN */
    if (end == (uint32_t)-1 || end > PAGING_MAX_PGN)
        end = PAGING_MAX_PGN;

    if (start > end)
        return 0;

    printf("---- Page Table ----\n");
    for (uint32_t pgn = start; pgn < end; pgn++) {
        uint32_t e = mm->pgd[pgn];
        if (e & PAGING_PTE_PRESENT_MASK) {
            addr_t fpn = GETVAL(e,
                                PAGING_PTE_FPN_MASK,
                                PAGING_PTE_FPN_LOBIT);
            printf("VPN %u -> FPN %u\n", pgn, fpn);
        }
    }
    printf("---------------------\n");
    return 0;
}


#endif //ndef MM64

/*
 * 32-bit Paging Memory Manager (clean version)
 * CO2018 – LamiaAtrium
 */


/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */
 
 /* NOTICE this moudle is deprecated in LamiaAtrium release
  *        the structure is maintained for future 64bit-32bit
  *        backward compatible feature or PAE feature 
  */
 
