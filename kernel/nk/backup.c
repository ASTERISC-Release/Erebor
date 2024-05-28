/*
 * Function: sva_wrmsr
 *
 * Description:
 *  SVA Intrinsic to load a value in an MSR. The given value should be
 *  given in edx:eax and the MSR should be given in ecx. If the MSR is
 *  EFER, we need to make sure that the NXE bit is enabled. 
 */
// void
// sva_wrmsr() {
//     uint64_t val;
//     unsigned int msr;
//     __asm__ __volatile__ (
//         "wrmsr\n"
//         : "=c" (msr), "=a" (val)
//         :
//         : "rax", "rcx", "rdx"
//     );
//     if ((msr == MSR_REG_EFER) && !(val & EFER_NXE)) {
//       // panic("SVA: attempt to clear the EFER.NXE bit: %x.", val);
//     }
// }

/*
 * Function: declare_ptp_and_walk_pt_entries
 *
 * Descriptions:
 *  This function recursively walks a page table and it's entries to initalize
 *  the SVA data structures for the given page. This function is meant to
 *  initialize SVA data structures so they mirror the static page table setup
 *  by a kernel. However, it uses the paging structure itself to walk the
 *  pages, which means it should be agnostic to the operating system being
 *  employed upon. The function only walks into page table pages that are valid
 *  or enabled. It also makes sure that if a given page table is already active
 *  in SVA then it skips over initializing its entries as that could cause an
 *  infinite loop of recursion. This is an issue in FreeBSD as they have a
 *  recursive mapping in the pml4 top level page table page.
 *  
 *  If a given page entry is marked as having a larger page size, such as may
 *  be the case with a 2MB page size for PD entries, then it doesn't traverse
 *  the page. Therefore, if the kernel page tables are configured correctly
 *  this won't initialize any SVA page descriptors that aren't in use.
 *
 *  The primary objective of this code is to for each valid page table page:
 *      [1] Initialize the page_desc for the given page
 *      [2] Set the page permissions as read only
 *
 * Assumptions:
 *  - The number of entries per page assumes a amd64 paging hardware mechanism.
 *    As such the number of entires per a 4KB page table page is 2^9 or 512
 *    entries. 
 *  - This page referenced in pageMapping has already been determined to be
 *    valid and requires SVA metadata to be created.
 *
 * Inputs:
 *   pageMapping: Page mapping associated with the given page being traversed.
 *                This mapping identifies the physical address/frame of the
 *                page table page so that SVA can initialize it's data
 *                structures then recurse on each entry in the page table page. 
 *  numPgEntries: The number of entries for a given level page table. 
 *     pageLevel: The page level of the given mapping {1,2,3,4}.
 *
 *
 * TODO: 
 *  - Modify the page entry number to be dynamic in some way to accomodate
 *    differing numbers of entries. This only impacts how we traverse the
 *    address structures. The key issue is that we don't want to traverse an
 *    entry that randomly has the valid bit set, but not have it point to a
 *    real page. For example, if the kernel did not zero out the entire page
 *    table page and only inserted a subset of entries in the page table, the
 *    non set entries could be identified as holding valid mappings, which
 *    would then cause this function to traverse down truly invalid page table
 *    pages. In FreeBSD this isn't an issue given the way they initialize the
 *    static mapping, but could be a problem given differnet intialization
 *    methods.
 *
 *  - Add code to mark direct map page table pages to prevent the OS from
 *    modifying them.
 *
 */
// #define DEBUG_INIT 0
// void 
// declare_ptp_and_walk_pt_entries(uintptr_t pageEntryPA, unsigned long
//         numPgEntries, enum page_type_t pageLevel ) 
// { 
//   int i;
//   int traversedPTEAlready;
//   enum page_type_t subLevelPgType;
//   unsigned long numSubLevelPgEntries;
//   page_desc_t *thisPg;
//   page_entry_t pageMapping; 
//   page_entry_t *pagePtr;

//   /* Store the pte value for the page being traversed */
//   pageMapping = pageEntryPA & PG_FRAME;

//   /* Set the page pointer for the given page */
// // #if USE_VIRT
//   // uintptr_t pagePhysAddr = pageMapping & PG_FRAME;
//   // pagePtr = (page_entry_t *) getVirtual(pagePhysAddr);
// // #else
//   pagePtr = (page_entry_t*) getVirtual((uintptr_t)(pageMapping & PG_FRAME));
// // #endif

//   /* Get the page_desc for this page */
//   thisPg = getPageDescPtr(pageMapping);

//   /* Mark if we have seen this traversal already */
//   traversedPTEAlready = (thisPg->type != PG_UNUSED);

// #if DEBUG_INIT >= 1
//   /* Character inputs to make the printing pretty for debugging */
//   char * indent = "";
//   char * l5s = "L5:";
//   char * l4s = "\tL4:";
//   char * l3s = "\t\tL3:";
//   char * l2s = "\t\t\tL2:";
//   char * l1s = "\t\t\t\tL1:";

//   switch (pageLevel){
//     case PG_L5:
//         indent = l5s;
//         printk("%sSetting L5 Page: mapping:0x%lx\n", indent, pageMapping);
//         break;
//     case PG_L4:
//         indent = l4s;
//         printk("%sSetting L4 Page: mapping:0x%lx\n", indent, pageMapping);
//         break;
//     case PG_L3:
//         indent = l3s;
//         printk("%sSetting L3 Page: mapping:0x%lx\n", indent, pageMapping);
//         break;
//     case PG_L2:
//         indent = l2s;
//         printk("%sSetting L2 Page: mapping:0x%lx\n", indent, pageMapping);
//         break;
//     case PG_L1:
//         indent = l1s;
//         printk("%sSetting L1 Page: mapping:0x%lx\n", indent, pageMapping);
//         break;
//     default:
//         break;
//   }
// #endif

//   /*
//    * For each level of page we do the following:
//    *  - Set the page descriptor type for this page table page
//    *  - Set the sub level page type and the number of entries for the
//    *    recursive call to the function.
//    */
//   switch(pageLevel){

//     case PG_L5:

//       thisPg->type = PG_L5;       /* Set the page type to L4 */
//       thisPg->user = 0;           /* Set the priv flag to kernel */
//       ++(thisPg->count);
//       subLevelPgType = PG_L4;
//       numSubLevelPgEntries = NPGDEPG;//    numPgEntries;
//       break;

//     case PG_L4:

//       thisPg->type = PG_L4;       /* Set the page type to L4 */
//       thisPg->user = 0;           /* Set the priv flag to kernel */
//       ++(thisPg->count);
//       subLevelPgType = PG_L3;
//       numSubLevelPgEntries = NP4DEPG;//    numPgEntries;
//       break;

//     case PG_L3:
      
//       /* TODO: Determine why we want to reassign an L4 to an L3 */
//       if (thisPg->type != PG_L4)
//         thisPg->type = PG_L3;       /* Set the page type to L3 */
//       thisPg->user = 0;           /* Set the priv flag to kernel */
//       ++(thisPg->count);
//       subLevelPgType = PG_L2;
//       numSubLevelPgEntries = NPUDEPG; //numPgEntries;
//       break;

//     case PG_L2:
      
//       /* 
//        * If my L2 page mapping signifies that this mapping references a 1GB
//        * page frame, then get the frame address using the correct page mask
//        * for a L3 page entry and initialize the page_desc for this entry.
//        * Then return as we don't need to traverse frame pages.
//        */
//       if ((pageMapping & PG_PS) != 0) {
// #if DEBUG_INIT >= 1
//         printk("\tIdentified 1GB page...\n");
// #endif
//         unsigned long index = (pageMapping & ~PUDMASK) / pageSize;
//         page_desc[index].type = PG_TKDATA;
//         page_desc[index].user = 0;           /* Set the priv flag to kernel */
//         ++(page_desc[index].count);
//         return;
//       } else {
//         thisPg->type = PG_L2;       /* Set the page type to L2 */
//         thisPg->user = 0;           /* Set the priv flag to kernel */
//         ++(thisPg->count);
//         subLevelPgType = PG_L1;
//         numSubLevelPgEntries = NPMDPG; // numPgEntries;
//       }
//       break;

//     case PG_L1:
//       /* 
//        * If my L1 page mapping signifies that this mapping references a 2MB
//        * page frame, then get the frame address using the correct page mask
//        * for a L2 page entry and initialize the page_desc for this entry. 
//        * Then return as we don't need to traverse frame pages.
//        */
//       if ((pageMapping & PG_PS) != 0){
// #if DEBUG_INIT >= 1
//         printk("\tIdentified 2MB page...\n");
// #endif
//         /* The frame address referencing the page obtained */
//         unsigned long index = (pageMapping & ~PMDMASK) / pageSize;
//         page_desc[index].type = PG_TKDATA;
//         page_desc[index].user = 0;           /* Set the priv flag to kernel */
//         ++(page_desc[index].count);
//         return;
//       } else {
//         thisPg->type = PG_L1;       /* Set the page type to L1 */
//         thisPg->user = 0;           /* Set the priv flag to kernel */
//         ++(thisPg->count);
//         subLevelPgType = PG_TKDATA;
//         numSubLevelPgEntries = NPTEPG;//      numPgEntries;
//       }
//       break;

//     default:
//       panic("SVA: walked an entry with invalid page type.");
//   }
  
//   /* 
//    * There is one recursive mapping, which is the last entry in the PML4 page
//    * table page. Thus we return before traversing the descriptor again.
//    * Notice though that we keep the last assignment to the page as the page
//    * type information. 
//    */
//    // Rahul: Check how this translates to Linux
//   if(traversedPTEAlready) {
// #if DEBUG_INIT >= 1
//     printk("%sRecursed on already initialized page_desc\n", indent);
// #endif
//     return;
//   }

// #if DEBUG_INIT >= 1
//   u_long nNonValPgs=0;
//   u_long nValPgs=0;
// #endif
//   /* 
//    * Iterate through all the entries of this page, recursively calling the
//    * walk on all sub entries.
//    */
//   for (i = 0; i < numSubLevelPgEntries; i++){
// #if 0
//     /*
//      * Do not process any entries that implement the direct map.  This prevents
//      * us from marking physical pages in the direct map as kernel data pages.
//      */
//     if ((pageLevel == PG_L4) && (i == (0xfffffe0000000000 / 0x1000))) {
//       continue;
//     }
// #endif
// #if OBSOLETE
//     //pagePtr += (sizeof(page_entry_t) * i);
//     //page_entry_t *nextEntry = pagePtr;
// #endif
//     page_entry_t * nextEntry = & pagePtr[i];

// #if DEBUG_INIT >= 5
//     printk("%sPagePtr in loop: %p, val: 0x%lx\n", indent, nextEntry, *nextEntry);
// #endif

//     /* 
//      * If this entry is valid then recurse the page pointed to by this page
//      * table entry.
//      */
//     if (*nextEntry & PG_V) {
// #if DEBUG_INIT >= 1
//       nValPgs++;
// #endif 

//       /* 
//        * If we hit the level 1 pages we have hit our boundary condition for
//        * the recursive page table traversals. Now we just mark the leaf page
//        * descriptors.
//        */
//       if (pageLevel == PG_L1){
// #if DEBUG_INIT >= 2
//           printk("%sInitializing leaf entry: pteaddr: %p, mapping: 0x%lx\n",
//                   indent, nextEntry, *nextEntry);
// #endif
//       } else {
// #if DEBUG_INIT >= 2
//       printk("%sProcessing:pte addr: %p, newPgAddr: %p, mapping: 0x%lx\n",
//               indent, nextEntry, (*nextEntry & PG_FRAME), *nextEntry ); 
// #endif
//           // printk("[Next - %d]: %lx %lx", i, nextEntry, *nextEntry);
//           declare_ptp_and_walk_pt_entries((uintptr_t)*nextEntry,
//                   numSubLevelPgEntries, subLevelPgType); 
//       }
//     } 
// #if DEBUG_INIT >= 1
//     else {
//       nNonValPgs++;
//     }
// #endif
//   }

// #if DEBUG_INIT >= 1
//   SVA_ASSERT((nNonValPgs + nValPgs) == 512, "Wrong number of entries traversed");

//   printk("%sThe number of || non valid pages: %lu || valid pages: %lu\n",
//           indent, nNonValPgs, nValPgs);
// #endif

// }