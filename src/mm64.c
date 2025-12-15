// /*
//  * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
//  */

// /* LamiaAtrium release
//  * Source Code License Grant: The authors hereby grant to Licensee
//  * personal permission to use and modify the Licensed Source Code
//  * for the sole purpose of studying while attending the course CO2018.
//  */

// /*
//  * PAGING based Memory Management
//  * Memory management unit mm/mm.c
//  */

#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#if defined(MM64)

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
    { // page swapped (NOT present in RAM)
      CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);   // <<< FIX: swapped => not present
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
	/* Extract page direactories */
	*pgd = (addr&PAGING64_ADDR_PGD_MASK)>>PAGING64_ADDR_PGD_LOBIT;
	*p4d = (addr&PAGING64_ADDR_P4D_MASK)>>PAGING64_ADDR_P4D_LOBIT;
	*pud = (addr&PAGING64_ADDR_PUD_MASK)>>PAGING64_ADDR_PUD_LOBIT;
	*pmd = (addr&PAGING64_ADDR_PMD_MASK)>>PAGING64_ADDR_PMD_LOBIT;
	*pt = (addr&PAGING64_ADDR_PT_MASK)>>PAGING64_ADDR_PT_LOBIT;

	/* TODO: implement the page direactories mapping */

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
	/* Shift the address to get page num and perform the mapping*/
	return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT,
                         pgd,p4d,pud,pmd,pt);
}

static inline addr_t paging64_pgn(addr_t addr)
{
  return addr >> PAGING64_ADDR_PT_SHIFT;  // 4KB page => shift 12
}

// static uint64_t* get_next_level_table64(uint64_t *entry) {
//     uint64_t content = *entry;
    
//     // Nếu bit Present chưa bật -> Chưa có bảng con
//     if (!(content & PAGING_PTE_PRESENT_MASK)) {
//         // Cấp phát bảng mới (512 entries * 8 bytes)
//         uint64_t *new_table = calloc(512, sizeof(uint64_t)); // calloc để clean về 0
        
//         // Ghi địa chỉ bảng mới vào entry của bảng cũ
//         *entry = (uint64_t)new_table | PAGING_PTE_PRESENT_MASK;
//         return new_table;    }

//     return (uint64_t *)(content & PAGING64_TABLE_ADDR_MASK);
// }

// static uint32_t* get_next_level_table32(uint64_t *entry) {
//     uint64_t content = *entry;

//     if (!(content & PAGING_PTE_PRESENT_MASK)) {
//         uint32_t *new_table = calloc(512, sizeof(uint32_t));
//         *entry = (uint64_t)new_table | PAGING_PTE_PRESENT_MASK;
//         return new_table;
//     }

//     return (uint32_t *)(content & PAGING64_TABLE_ADDR_MASK);
// }

// Bảng 64-bit cho PGD / P4D / PUD / PMD
// static uint64_t* get_next_level_table64(uint64_t *entry) {
//     if (*entry == 0) {
//         void *p = NULL;
//         if (posix_memalign(&p, PAGING64_PAGESZ, PAGING64_PAGESZ) != 0)
//             return NULL;
//         memset(p, 0, PAGING64_PAGESZ);
//         *entry = (uint64_t)p;      // CHỈ LƯU POINTER, KHÔNG OR FLAG
//         return (uint64_t*)p;
//     }
//     return (uint64_t*)(*entry);    // Trả pointer thuần
// }

// // Bảng PT 32-bit (entries là PTE 32-bit)
// static uint32_t* get_next_level_table32(uint64_t *entry) {
//     if (*entry == 0) {
//         void *p = NULL;
//         if (posix_memalign(&p, PAGING64_PAGESZ, PAGING64_PAGESZ) != 0)
//             return NULL;
//         memset(p, 0, PAGING64_PAGESZ);
//         *entry = (uint64_t)p;      // pointer thuần
//         return (uint32_t*)p;
//     }
//     return (uint32_t*)(*entry);
// }

static uint64_t* get_next_level_table64(uint64_t *entry) {
    uint64_t content = *entry;
    
    // Nếu bit Present chưa bật -> Chưa có bảng con
  if (!(content & PAGING64_PTE_PRESENT_MASK)) {
        // Cấp phát bảng mới (512 entries * 8 bytes)
        uint64_t *new_table = calloc(512, sizeof(uint64_t)); // calloc để clean về 0
        
        // Ghi địa chỉ bảng mới vào entry của bảng cũ
    *entry = (uint64_t)new_table | PAGING64_PTE_PRESENT_MASK;
        return new_table;    }

    return (uint64_t *)(content & PAGING64_TABLE_ADDR_MASK);
}

static uint32_t* get_next_level_table32(uint64_t *entry) {
    uint64_t content = *entry;

  if (!(content & PAGING64_PTE_PRESENT_MASK)) {
        uint32_t *new_table = calloc(512, sizeof(uint32_t));
    *entry = (uint64_t)new_table | PAGING64_PTE_PRESENT_MASK;
        return new_table;
    }

    return (uint32_t *)(content & PAGING64_TABLE_ADDR_MASK);
}

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
    addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
    uint64_t *pgd_tbl;
    uint64_t *p4d_tbl;
    uint64_t *pud_tbl;
    uint64_t *pmd_tbl;
    uint32_t *pt_tbl;
    uint32_t *pte;

    if (caller == NULL || caller->mm == NULL)
        return -1;

    /* Tách pgn thành các chỉ số cấp trang */
    get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

    /* Điều hướng qua các bảng cấp: PGD -> P4D -> PUD -> PMD -> PT */
    pgd_tbl = caller->mm->pgd;
    p4d_tbl = get_next_level_table64(&pgd_tbl[pgd_idx]);
    pud_tbl = get_next_level_table64(&p4d_tbl[p4d_idx]);
    pmd_tbl = get_next_level_table64(&pud_tbl[pud_idx]);
    pt_tbl  = get_next_level_table32(&pmd_tbl[pmd_idx]);

    /* PTE cuối cùng */
    pte = &pt_tbl[pt_idx];

    /* Trang nằm trên swap: không còn hiện diện trong RAM */
    CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);
    SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
    CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

    /* Ghi thông tin vị trí trong swap */
    SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
    SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

    return 0;
}


/*
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
    addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
    uint64_t *pgd_tbl;
    uint64_t *p4d_tbl;
    uint64_t *pud_tbl;
    uint64_t *pmd_tbl;
    uint32_t *pt_tbl;
    uint32_t *pte;

    if (caller == NULL || caller->mm == NULL)
        return -1;

    /* Lấy index từng cấp từ số trang ảo */
    get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

    /* Chuỗi bảng: PGD -> P4D -> PUD -> PMD -> PT */
    pgd_tbl = caller->mm->pgd;
    p4d_tbl = get_next_level_table64(&pgd_tbl[pgd_idx]);
    pud_tbl = get_next_level_table64(&p4d_tbl[p4d_idx]);
    pmd_tbl = get_next_level_table64(&pud_tbl[pud_idx]);
    pt_tbl  = get_next_level_table32(&pmd_tbl[pmd_idx]);

    pte = &pt_tbl[pt_idx];

    /* Thiết lập trang đang hiện diện trong RAM tại frame fpn */
    SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
    CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
    CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

    /* Ghi lại FPN mới */
    CLRBIT(*pte, PAGING_PTE_FPN_MASK);
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
    addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
    uint64_t *tbl;
    uint64_t entry;
    uint32_t *pt_tbl;

    if (caller == NULL || caller->mm == NULL || caller->mm->pgd == NULL)
        return 0;

    /* Phân rã số trang thành các chỉ số cấp */
    get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

    /* Bắt đầu từ PGD */
    tbl = caller->mm->pgd;

    /* PGD -> P4D */
    entry = tbl[pgd_idx];
    if (!(entry & PAGING64_PTE_PRESENT_MASK))
        return 0;
    tbl = (uint64_t *)(entry & PAGING64_TABLE_ADDR_MASK);

    /* P4D -> PUD */
    entry = tbl[p4d_idx];
    if (!(entry & PAGING64_PTE_PRESENT_MASK))
        return 0;
    tbl = (uint64_t *)(entry & PAGING64_TABLE_ADDR_MASK);

    /* PUD -> PMD */
    entry = tbl[pud_idx];
    if (!(entry & PAGING64_PTE_PRESENT_MASK))
        return 0;
    tbl = (uint64_t *)(entry & PAGING64_TABLE_ADDR_MASK);

    /* PMD -> PT */
    entry = tbl[pmd_idx];
    if (!(entry & PAGING64_PTE_PRESENT_MASK))
        return 0;
    pt_tbl = (uint32_t *)(entry & PAGING64_TABLE_ADDR_MASK);

    /* PT -> PTE */
    return pt_tbl[pt_idx];
}



/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{

  addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
  get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

   // 1. Truy cập PGD
  uint64_t *pgd_table = caller->mm->pgd;
  // 2. Truy cập P4D
  uint64_t *p4d_table = get_next_level_table64(&pgd_table[pgd_idx]);
  // 3. Truy cập PUD
  uint64_t *pud_table = get_next_level_table64(&p4d_table[p4d_idx]);
  // 4. Truy cập PMD
  uint64_t *pmd_table = get_next_level_table64(&pud_table[pud_idx]);
  // 5. Truy cập PT (Page Table cuối cùng)
  uint32_t *pt_table  = get_next_level_table32(&pmd_table[pmd_idx]);

  pt_table[pt_idx] = pte_val; 
  
  return 0;

}

/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum)                      // num of mapping page
{
  //int pgit = 0;
  //uint64_t pattern = 0xdeadbeef;

  /* TODO memset the page table with given pattern
   */

  int pgit = 0;
  uint32_t pattern = 0xdeadbeef;


  for (pgit = 0; pgit < pgnum; pgit++)
  { 
    addr_t pgn = PAGING_PGN(addr + pgit * PAGING_PAGESZ);
    pte_set_entry(caller, pgn, pattern);
  }
    
  return 0;

}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                       addr_t addr,                    // start address (aligned)
                       int pgnum,                      // number of pages
                       struct framephy_struct *frames, // mapped frames
                       struct vm_rg_struct *ret_rg)    // out: mapped region
{
    struct framephy_struct *cur_frm = frames;
    int i;
    addr_t cur_pgn;

    if (caller == NULL || caller->mm == NULL)
        return -1;

    /* Nếu có yêu cầu trả về vùng ảo, thiết lập biên vùng */
    if (ret_rg != NULL) {
        ret_rg->rg_start = addr;
        ret_rg->rg_end   = addr + (addr_t)pgnum * PAGING_PAGESZ;
    }

    /* Ánh xạ lần lượt từng frame vật lý vào các trang ảo tương ứng */
    for (i = 0; i < pgnum; i++) {
        if (cur_frm == NULL) {
            fprintf(stderr, "vmap_page_range: not enough frames\n");
            return -1;
        }

        cur_pgn = PAGING_PGN(addr + (addr_t)i * PAGING_PAGESZ);

        /* Ghi FPN vào PTE */
        pte_set_fpn(caller, cur_pgn, cur_frm->fpn);

        /* Thêm trang vào danh sách FIFO để sau này dùng cho thay thế trang */
        enlist_pgn_node(&caller->mm->fifo_pgn, cur_pgn);

        cur_frm = cur_frm->fp_next;
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
  /* TODO: allocate the page 
  //caller-> ...
  //frm_lst-> ...
  */


/*
  for (pgit = 0; pgit < req_pgnum; pgit++)
  {
    // TODO: allocate the page 
    if (MEMPHY_get_freefp(caller->mram, &fpn) == 0)
    {
      newfp_str->fpn = fpn;
    }
    else
    { // TODO: ERROR CODE of obtaining somes but not enough frames
    }
  }
*/


  /* End TODO */

  addr_t fpn;
  int pgit;
  struct framephy_struct *newfp_str = NULL;

 for (pgit =0 ; pgit < req_pgnum; pgit++)
 {
    newfp_str = malloc(sizeof(struct framephy_struct));
    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) == 0)
    {
      newfp_str->fpn = fpn;
      newfp_str->owner = caller->krnl->mm;

      newfp_str->fp_next = *frm_lst;
      *frm_lst = newfp_str;
      
    } else {
      // Hết RAM
      free(newfp_str);
      while(*frm_lst != NULL) {
        struct framephy_struct *tmp = *frm_lst;
        *frm_lst = (*frm_lst)->fp_next;
        MEMPHY_put_freefp(caller->krnl->mram, tmp->fpn);
        free(tmp);
      }
      return -3000; // Out of memory error code
    }


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
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  addr_t ret_alloc = 0;
//  int pgnum = incpgnum;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  // ret_alloc = alloc_pages_range(caller, pgnum, &frm_lst);

  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);


  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000)
  {
    return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
   vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

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
  int cellidx;
  addr_t addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
// int init_mm(struct mm_struct *mm, struct pcb_t *caller)
// {
//   // (1) Nên zero luôn struct mm cho chắc (rất khuyến khích)
//   memset(mm, 0, sizeof(*mm));

//   struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
//   if (!vma0) return -1;

//   if (posix_memalign((void**)&mm->pgd, PAGING64_PAGESZ, PAGING64_PAGESZ) != 0)
//     return -1;
//   memset(mm->pgd, 0, PAGING64_PAGESZ);

//   mm->p4d = NULL;
//   mm->pud = NULL;
//   mm->pmd = NULL;
//   mm->pt  = NULL;
//   mm->fifo_pgn = NULL;

//   /* By default the owner comes with at least one vma */
//   vma0->vm_id    = 0;
//   vma0->vm_start = 0;
//   vma0->vm_end   = 0;
//   vma0->sbrk     = 0;

//   // *** QUAN TRỌNG: init head list về NULL ***
//   vma0->vm_freerg_list = NULL;

//   // Tạo region trống để quản lý danh sách free 
//   struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
//   enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

//   vma0->vm_next = NULL;
//   vma0->vm_mm   = mm;

//   mm->mmap = vma0;

//   // Nếu mm có symrgtbl là mảng tĩnh thì nên zero luôn:
//   // memset(mm->symrgtbl, 0, sizeof(mm->symrgtbl));

//   return 0;
// }

int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));

   mm->pgd = calloc(512, sizeof(uint64_t)); // calloc để toàn bộ entry = 0 (Invalid)
  /* TODO init page table directory */
  // Pre-allocate the first table at each level for index 0
  mm->p4d = calloc(512, sizeof(uint64_t));
  mm->pud = calloc(512, sizeof(uint64_t));
  mm->pmd = calloc(512, sizeof(uint64_t));
  mm->pt  = calloc(512, sizeof(uint32_t));
  
  // Link them together
  mm->pgd[0] = (uint64_t)mm->p4d | PAGING64_PTE_PRESENT_MASK;
  mm->p4d[0] = (uint64_t)mm->pud | PAGING64_PTE_PRESENT_MASK;
  mm->pud[0] = (uint64_t)mm->pmd | PAGING64_PTE_PRESENT_MASK;
  mm->pmd[0] = (uint64_t)mm->pt | PAGING64_PTE_PRESENT_MASK;

  mm->fifo_pgn = NULL;

  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;

  //Tạo region trống để qaurn lý danh sách free 
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /* TODO update VMA0 next */
  vma0->vm_next = NULL; // Chưa có VMA nào khác nên next = NULL

  /* Point vma owner backward */
  vma0->vm_mm = mm; // VMA0 thuộc về mm

  /* TODO: update mmap */
  mm->mmap = vma0; // Gán VMA0 cho mmap của mm

  // Nếu mm có symrgtbl là mảng tĩnh thì nên zero luôn:
  // memset(mm->symrgtbl, 0, sizeof(mm->symrgtbl));

  return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
  struct vm_rg_struct *rgnode = (struct vm_rg_struct *)malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
  struct pgn_t *pnode = (struct pgn_t *)malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) { printf("NULL list\n"); return -1;}
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[" FORMAT_ADDR "]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[" FORMAT_ADDR "->"  FORMAT_ADDR "]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[" FORMAT_ADDR "]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}

// int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
// {
//    int pgn_start, pgn_end;
//   int pgit;
//   addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;

//   if (end == -1) {
//       pgn_start = 0;
//       pgn_end = PAGING_MAX_PGN;
//   } else {
//       pgn_start = PAGING_PGN(start);
//       pgn_end = PAGING_PGN(end);
//   }

//   printf("print_pgtbl: %ld - %ld\n", (long)start, (long)end);

//   for (pgit = pgn_start; pgit < pgn_end; pgit++) 
//   {

//      uint32_t pte = pte_get_entry(caller, pgit);

//      if (PAGING_PAGE_PRESENT(pte)) {
//          addr_t virtual_addr = (addr_t)pgit * PAGING_PAGESZ;
         
//          // Lấy chi tiết các index để in ra
//          get_pd_from_address(virtual_addr, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

//          printf("%08lx: [PGD:%ld] [P4D:%ld] [PUD:%ld] [PMD:%ld] [PT:%ld] -> FPN:%d\n", 
//                 (unsigned long)virtual_addr, 
//                 (long)pgd_idx, (long)p4d_idx, (long)pud_idx, (long)pmd_idx, (long)pt_idx, 
//                 PAGING_FPN(pte));
//      }
//   }

//   return 0;
// }

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
  // Print the page table structure addresses
  printf("print_pgtbl:\n PDG=%p P4g=%p PUD=%p PMD=%p PT=%p\n", 
         (void*)caller->mm->pgd,
         (void*)caller->mm->p4d, 
         (void*)caller->mm->pud,
         (void*)caller->mm->pmd,
         (void*)caller->mm->pt);

  return 0;
}

#endif  //def MM64

// /*
//  * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
//  */

// /* LamiaAtrium release
//  * Source Code License Grant: The authors hereby grant to Licensee
//  * personal permission to use and modify the Licensed Source Code
//  * for the sole purpose of studying while attending the course CO2018.
//  */

// /*
//  * 64-bit PAGING based Memory Management
//  * Memory management unit mm/mm64.c
//  */

/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

