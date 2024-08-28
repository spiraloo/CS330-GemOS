#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/*
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables
 * */

void *table_walk(u64 addr, struct exec_context* current)
{   
    u16 pte_offset = (addr >> 12) & 0x1FF; // Bits 20-12
    u16 pmd_offset = (addr >> 21) & 0x1FF; // Bits 30-21
    u16 pud_offset = (addr >> 30) & 0x1FF; // Bits 39-30
    u16 pgd_offset = (addr >> 39) & 0x1FF; // Bits 47-39

    u64 *pgd = (u64 *)osmap(current->pgd);
    if (pgd == NULL)
    {
        return NULL;
    }

    u64 pgd_t = *(pgd + pgd_offset);

    if ((pgd_t & 1) == 0)
    {
        u64 pfn_new = (u64)os_pfn_alloc(OS_PT_REG);
        if (pfn_new == 0)
        {
            return NULL;
        }

        // change the left 52 bit of pgd_t with *pfn bits
        pgd_t = (pfn_new << 12) | (pgd_t & 0xFFF);
        pgd_t = pgd_t | 1 | 8 | 16;
        *(pgd + pgd_offset) = pgd_t;
    }

    // get the pud entry of the virtual address using pud_offset and pgd_t
    u64 *pud = (u64 *)osmap(pgd_t >> 12);
    if (pud == NULL)
    {
        return NULL;
    }

    u64 pud_t = *(pud + pud_offset);

    if ((pud_t & 1) == 0)
    {
        u64 pfn_new = (u64)os_pfn_alloc(OS_PT_REG);
        if (pfn_new == 0)
        {
            return NULL;
        }

        // change the left 52 bit of pud_t with *pfn bits
        pud_t = (pfn_new << 12) | (pud_t & 0xFFF);
        pud_t = pud_t | 1 | 8 | 16;
        *(pud + pud_offset) = pud_t;
    }

    // get the pmd entry of the virtual address using pmd_offset and pud_t
    u64 *pmd = (u64 *)osmap(pud_t >> 12);
    if (pmd == NULL)
    {
        return NULL;
    }

    u64 pmd_t = *(pmd + pmd_offset);

    if ((pmd_t & 1) == 0)
    {
        u64 pfn_new = (u64)os_pfn_alloc(OS_PT_REG);
        if (pfn_new == 0)
        {
            return NULL;
        }

        // change the left 52 bit of pmd_t with *pfn bits
        pmd_t = (pfn_new << 12) | (pmd_t & 0xFFF);
        pmd_t = pmd_t | 1 | 8 | 16;
        *(pmd + pmd_offset) = pmd_t;
    }

    // get the pte entry of the virtual address using pte_offset and pmd_t
    u64 *pte = (u64 *)osmap(pmd_t >> 12);
    if (pte == NULL)
    {
        return NULL;
    }

    return (pte + pte_offset);
}

int remove_page(u64 addr, u64 *pgd)
{
    printk("inside remove page\n");

    u16 pte_offset = (addr >> 12) & 0x1FF; // Bits 20-12
    u16 pmd_offset = (addr >> 21) & 0x1FF; // Bits 30-21
    u16 pud_offset = (addr >> 30) & 0x1FF; // Bits 39-30
    u16 pgd_offset = (addr >> 39) & 0x1FF; // Bits 47-39

    u64 pgd_t = *(pgd + pgd_offset);
    if ((pgd_t & 1) == 0)
    {
        return -1;
    }

    u64 *pud = (u64 *)osmap(pgd_t >> 12);
    u64 pud_t = *(pud + pud_offset);
    if ((pud_t & 1) == 0)
    {
        return -1;
    }

    u64 *pmd = (u64 *)osmap(pud_t >> 12);
    u64 pmd_t = *(pmd + pmd_offset);
    if ((pmd_t & 1) == 0)
    {
        return -1;
    }

    u64 *pte = (u64 *)osmap(pmd_t >> 12);
    u64 pte_t = *(pte + pte_offset);
    if ((pte_t & 1) == 0)
    {
        return -1;
    }

    // free the physical page
    os_pfn_free(USER_REG, pte_t >> 12);
    pte_t = 0;
    *(pte + pte_offset) = pte_t;

    // check if all entries in pte is zero

    int i;
    for (i = 0; i < 512; i++)
    {
        if (*(pte + i) != 0)
        {
            return 0;
        }
    }

    // free the pte
    os_pfn_free(OS_PT_REG, pmd_t >> 12);
    pmd_t = 0;
    *(pmd + pmd_offset) = pmd_t;

    // check if all entries in pmd is zero

    for (i = 0; i < 512; i++)
    {
        if (*(pmd + i) != 0)
        {
            return 0;
        }
    }

    // free the pmd
    os_pfn_free(OS_PT_REG, pud_t >> 12);
    pud_t = 0;
    *(pud + pud_offset) = pud_t;

    // check if all entries in pud is zero

    for (i = 0; i < 512; i++)
    {
        if (*(pud + i) != 0)
        {
            return 0;
        }
    }

    // free the pud

    os_pfn_free(OS_PT_REG, pgd_t >> 12);
    pgd_t = 0;
    *(pgd + pgd_offset) = pgd_t;

    return 0;
}

int change_flags_page(u64 addr, u64 *pgd, int prot)
{
    printk("inside remove page\n");

    u16 pte_offset = (addr >> 12) & 0x1FF; // Bits 20-12
    u16 pmd_offset = (addr >> 21) & 0x1FF; // Bits 30-21
    u16 pud_offset = (addr >> 30) & 0x1FF; // Bits 39-30
    u16 pgd_offset = (addr >> 39) & 0x1FF; // Bits 47-39

    u64 pgd_t = *(pgd + pgd_offset);
    if ((pgd_t & 1) == 0)
    {
        return -1;
    }

    u64 *pud = (u64 *)osmap(pgd_t >> 12);
    u64 pud_t = *(pud + pud_offset);
    if ((pud_t & 1) == 0)
    {
        return -1;
    }

    u64 *pmd = (u64 *)osmap(pud_t >> 12);
    u64 pmd_t = *(pmd + pmd_offset);
    if ((pmd_t & 1) == 0)
    {
        return -1;
    }

    u64 *pte = (u64 *)osmap(pmd_t >> 12);
    u64 pte_t = *(pte + pte_offset);
    if ((pte_t & 1) == 0)
    {
        return -1;
    }

    // change acces flags of pte_t
    if (prot == (PROT_READ | PROT_WRITE))
    {
        pte_t = (pte_t | 8);
        printk("-----access changed from read to write-----\n");
    }
    if (prot == PROT_READ)
        pte_t = pte_t - (pte_t & 8);

    *(pte + pte_offset) = pte_t;
    asm volatile("invlpg (%0)" ::"r"(addr)
                 : "memory");

    return 0;
}

int is_valid(struct exec_context *current, u64 start, u64 end)
{
    // check if start address is between any vm area
    struct vm_area *temp_1 = current->vm_area;
    while (temp_1->vm_next != NULL)
    {
        if (start >= temp_1->vm_start && start < temp_1->vm_end)
        {
            printk("address is already mapped using case 1\n");
            return -EINVAL;
        }
        temp_1 = temp_1->vm_next;
    }

    // check if end address is between any vm area
    struct vm_area *temp_2 = current->vm_area;
    while (temp_2->vm_next != NULL)
    {
        if (end > temp_2->vm_start && end <= temp_2->vm_end)
        {
            printk("address is already mapped using case 2\n");
            return -EINVAL;
        }
        temp_2 = temp_2->vm_next;
    }

    // check if any vm area is between start and end
    struct vm_area *temp_3 = current->vm_area;
    while (temp_3->vm_next != NULL)
    {
        if (temp_3->vm_start >= start && temp_3->vm_end <= end)
        {
            printk("address is already mapped using case 3\n");
            return -EINVAL;
        }
        temp_3 = temp_3->vm_next;
    }

    return 0;
}

/**
 * mprotect System call Implementation.
 */

long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{

    printk("inside vm_area_protect\n");
    // Validate input parameters
    if (length <= 0)
    {
        printk("length is invalid\n");
        return -EINVAL; // Invalid length
    }
    // addr not between MMAP_AREA_START and MMAP_AREA_END
    if (addr < MMAP_AREA_START || addr > MMAP_AREA_END)
    {
        printk("addr is not between MMAP_AREA_START and MMAP_AREA_END\n");
        return -EINVAL;
    }

    if ((prot != (PROT_READ | PROT_WRITE)) && (prot != PROT_READ))
    {
        printk("prot is invalid\n");
        return -EINVAL;
    }

    if (length % 4096 != 0)
    {
        length = (length / 4096 + 1) * 4096;
    }
    // addr is page aligned
    if (addr % 4096 != 0)
    {
        printk("addr is not page aligned\n");
        return -EINVAL;
    }

    u64 start = addr;
    u64 end = addr + length;

    struct vm_area *prev = current->vm_area;
    struct vm_area *curr = current->vm_area->vm_next;

    while (curr)
    {
        if (start <= curr->vm_start && end >= curr->vm_end)
        {
            printk("case 1\n");

            printk("access flags changed in case 1\n");
            u64 *pgd = (u64 *)osmap(current->pgd);
            u64 trav = curr->vm_start;
            for (trav = curr->vm_start; trav < curr->vm_end; trav += (4 * 1024))
            {
                if (change_flags_page(trav, pgd, prot) == -1)
                {
                    continue;
                }
                else
                {
                    printk("page flags changed\n");
                }
            }
            curr->access_flags = prot;
            prev = curr;
            curr = curr->vm_next;
        }
        else if (start <= curr->vm_start && end < curr->vm_end && end > curr->vm_start)
        {
            printk("case 2\n");
            // make a new vm_area and change the start of curr
            u64 *pgd = (u64 *)osmap(current->pgd);
            u64 trav = curr->vm_start;
            for (trav = curr->vm_start; trav < end; trav += 4 * 1024)
            {
                if (change_flags_page(trav, pgd, prot) == -1)
                {
                    continue;
                }
                else
                {
                    printk("page flags changed\n");
                }
            }
            struct vm_area *new_vm_area = os_alloc(sizeof(struct vm_area));
            new_vm_area->vm_start = end;
            new_vm_area->vm_end = curr->vm_end;
            new_vm_area->access_flags = curr->access_flags;
            new_vm_area->vm_next = curr->vm_next;
            curr->vm_next = new_vm_area;
            curr->vm_end = end;
            curr->access_flags = prot;
            prev = curr;
            curr = curr->vm_next;
            stats->num_vm_area++;

            break;
        }
        else if (start > curr->vm_start && end >= curr->vm_end && start < curr->vm_end)
        {
            printk("case 3\n");
            // make a new vm_area and change the end of curr
            u64 *pgd = (u64 *)osmap(current->pgd);
            u64 trav = start;
            for (trav = start; trav < curr->vm_end; trav += 4 * 1024)
            {
                if (change_flags_page(trav, pgd, prot) == -1)
                {
                    continue;
                }
                else
                {
                    printk("page flags changed\n");
                }
            }
            struct vm_area *new_vm_area = os_alloc(sizeof(struct vm_area));
            new_vm_area->vm_start = start;
            new_vm_area->vm_end = curr->vm_end;
            new_vm_area->access_flags = prot;
            new_vm_area->vm_next = curr->vm_next;
            curr->vm_next = new_vm_area;
            curr->vm_end = start;
            prev = curr;
            curr = curr->vm_next;
            stats->num_vm_area++;
        }
        else if (start > curr->vm_start && end < curr->vm_end)
        {
            printk("case 4\n");
            // make 2 new vm_area and change the start and end of curr
            u64 *pgd = (u64 *)osmap(current->pgd);
            u64 trav = start;
            for (trav = start; trav < end; trav += 4 * 1024)
            {
                if (change_flags_page(trav, pgd, prot) == -1)
                {
                    continue;
                }
                else
                {
                    printk("page flags changed\n");
                }
            }
            struct vm_area *new_vm_area_1 = os_alloc(sizeof(struct vm_area));
            struct vm_area *new_vm_area_2 = os_alloc(sizeof(struct vm_area));
            new_vm_area_1->vm_start = start;
            new_vm_area_1->vm_end = end;
            new_vm_area_1->access_flags = prot;
            new_vm_area_1->vm_next = new_vm_area_2;
            new_vm_area_2->vm_start = end;
            new_vm_area_2->vm_end = curr->vm_end;
            new_vm_area_2->access_flags = curr->access_flags;
            new_vm_area_2->vm_next = curr->vm_next;
            curr->vm_next = new_vm_area_1;
            curr->vm_end = start;
            prev = curr;
            curr = curr->vm_next;
            stats->num_vm_area += 2;

            break;
        }
        else if (start < curr->vm_start && end <= curr->vm_start)
        {
            printk("case 5\n");
            prev = curr;
            curr = curr->vm_next;
            break;
        }
        else if (start >= curr->vm_end && end > curr->vm_end)
        {
            printk("case 6\n");
            prev = curr;
            curr = curr->vm_next;
        }
    }

    // check if new vm_area can be merged with the next vm_area
    struct vm_area *temp = current->vm_area->vm_next;
    while (temp->vm_next != NULL)
    {
        printk("inside while loop of merging in mprotect\n");
        if (temp->vm_end == temp->vm_next->vm_start && temp->access_flags == temp->vm_next->access_flags)
        {
            // merge the vm_area
            temp->vm_end = temp->vm_next->vm_end;
            struct vm_area *new = temp->vm_next;
            temp->vm_next = temp->vm_next->vm_next;
            os_free(new, sizeof(struct vm_area));
            stats->num_vm_area--;
        }
        else
        {
            temp = temp->vm_next;
        }
    }
    return 0; // Success
}

/**
 * mmap system call implementation.
 */

long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{

    printk("inside vm_area_map\n");
    // check if length is valid
    if (length <= 0 || length > MMAP_AREA_END - MMAP_AREA_START || length > 2 * 1024 * 1024)
    {
        printk("length is invalid\n");
        return -EINVAL;
    }

    // check if addr is invalid
    if (addr < MMAP_AREA_START || addr > MMAP_AREA_END)
    {
        if (addr != 0)
        {
            printk("addr is invalid\n");
            return -EINVAL;
        }
    }

    // check if addr is page aligned, if not return -EINVAL
    if (addr % 4096 != 0)
    {
        printk("addr is not page aligned\n");
        return -EINVAL;
    }

    // if prot is not valid return -EINVAL
    if ((prot != (PROT_READ | PROT_WRITE)) && (prot != PROT_READ))
    {
        printk("prot is invalid\n");
        return -EINVAL;
    }

    // if flags is not valid return -EINVAL
    if ((flags != MAP_FIXED) && (flags != 0))
    {
        printk("flags is invalid\n");
        return -EINVAL;
    }
    if (length % 4096 != 0)
    {
        length = (length / 4096 + 1) * 4096;
    }

    u64 start, end;
    // case when addr is not zero
    if (addr != 0)
    {
        start = addr;
        end = addr + length;
        if (is_valid(current, start, end) == -EINVAL)
        {
            if (flags == MAP_FIXED)
            {
                printk("address is already mapped and mapping is fixed\n");
                return -EINVAL;
            }
        }
    }

    if (addr == 0 && flags == MAP_FIXED)
    {
        return -EINVAL;
    }
    else if ((addr == 0 && flags == 0) || (addr != 0 && flags == 0 && (is_valid(current, start, end) == -EINVAL)))
    {
        struct vm_area *temp = current->vm_area;
        if (temp == NULL)
        {
            temp = os_alloc(sizeof(struct vm_area));
            // if  temp is null return -EINVAL
            if (temp == NULL)
            {
                printk("temp is null\n");
                return -EINVAL;
            }
            temp->vm_start = MMAP_AREA_START;
            temp->vm_end = MMAP_AREA_START + (4 * 1024);
            temp->access_flags = 0x0;
            current->vm_area = temp;
            stats->num_vm_area = 1;
        }
        while (temp->vm_next != NULL)
        {
            if (temp->vm_next->vm_start - temp->vm_end >= length)
            {
                break;
            }
            temp = temp->vm_next;
        }

        start = temp->vm_end;
        end = start + length;
    }

    // now we have the start and end address of the new vm_area now we just have create it and insert it in the list
    struct vm_area *new_vm_area = os_alloc(sizeof(struct vm_area));

    // if new_vm_area is null return -EINVAL
    if (new_vm_area == NULL)
    {
        printk("new vm area is null\n");
        return -EINVAL;
    }

    new_vm_area->vm_end = end;
    new_vm_area->vm_start = start;
    new_vm_area->access_flags = prot;
    new_vm_area->vm_next = NULL;
    stats->num_vm_area++;
    printk("new vm area created\n");

    // insert it into vm_ares list by traversing and finding vm area just before start address
    struct vm_area *temp = current->vm_area;
    while (temp->vm_next != NULL)
    {
        if (temp->vm_next->vm_start > start)
        {
            break;
        }
        temp = temp->vm_next;
    }

    new_vm_area->vm_next = temp->vm_next;
    temp->vm_next = new_vm_area;

    // now we have to check the merging conditions and merge the vm_area if possible

    // check if new vm_area can be merged with temp defined above
    if (temp->vm_end == new_vm_area->vm_start && temp->access_flags == new_vm_area->access_flags)
    {
        // merge the vm_area
        temp->vm_end = new_vm_area->vm_end;
        temp->vm_next = new_vm_area->vm_next;
        os_free(new_vm_area, sizeof(struct vm_area));
        new_vm_area = temp;
        stats->num_vm_area--;
    }

    // check if new vm_area can be merged with the next vm_area
    if (new_vm_area->vm_next != NULL)
    {
        if (new_vm_area->vm_end == new_vm_area->vm_next->vm_start && new_vm_area->access_flags == new_vm_area->vm_next->access_flags)
        {
            // merge the vm_area
            new_vm_area->vm_end = new_vm_area->vm_next->vm_end;
            new_vm_area->vm_next = new_vm_area->vm_next->vm_next;
            os_free(new_vm_area->vm_next, sizeof(struct vm_area));
            stats->num_vm_area--;
        }
    }

    // return the start address
    printk("start address is %x\n", start);
    return start;
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    printk("inside vm_area_unmap\n");
    // Validate input parameters
    if (length <= 0 || length > 2 * 1024 * 1024 || length > MMAP_AREA_END - MMAP_AREA_START)
    {
        printk("length is invalid\n");
        return -EINVAL; // Invalid length
    }

    // check addr is page aligned
    printk("addr for unmap is : %x \n", addr);
    printk("addr modulo 4096 for unmap is : %x \n", addr % 4096);
    if (addr % 4096 != 0)
    {
        printk("addr is not page aligned\n");
        return -EINVAL;
    }

    // addr not between MMAP_AREA_START and MMAP_AREA_END
    if (addr < MMAP_AREA_START || addr > MMAP_AREA_END)
    {
        printk("addr is not between MMAP_AREA_START and MMAP_AREA_END\n");
        return -EINVAL;
    }
    if (length % 4096 != 0)
    {
        length = (length / 4096 + 1) * 4096;
    }

    u64 start = addr;
    u64 end = addr + length;

    // we have to find where the start addr lies in which vm_area
    struct vm_area *prev = current->vm_area;
    struct vm_area *curr = current->vm_area;
    if (curr == NULL)
    {
        curr = os_alloc(sizeof(struct vm_area));
        curr->access_flags = 0x0;
        curr->vm_start = MMAP_AREA_START;
        curr->vm_end = MMAP_AREA_START + (4 * 1024);
        curr->vm_next = NULL;
        current->vm_area = curr;
        stats->num_vm_area = 1;

        return 0;
    }
    while (curr)
    {
        // make 6 cases and handle them
        if (start <= curr->vm_start && end >= curr->vm_end)
        {
            printk("case 1\n");
            // free the physiscal address by removing the pfn from the page table
            u64 *pgd = (u64 *)osmap(current->pgd);
            u64 trav = curr->vm_start;
            for (trav = curr->vm_start; trav < curr->vm_end; trav += 4 * 1024)
            {
                if (remove_page(trav, pgd) == -1)
                {
                    continue;
                }
                else
                {
                    printk("page removed\n");
                }

                asm volatile("invlpg (%0)" ::"r"(trav)
                             : "memory");
            }

            prev->vm_next = curr->vm_next;
            os_free(curr, sizeof(struct vm_area));

            stats->num_vm_area--;
            curr = prev->vm_next;
        }
        else if (start <= curr->vm_start && end < curr->vm_end && end > curr->vm_start)
        {
            // make a new vm_area and change the start of curr
            printk("case 2\n");
            u64 *pgd = (u64 *)osmap(current->pgd);
            u64 trav = curr->vm_start;
            for (trav = curr->vm_start; trav < end; trav += 4 * 1024)
            {
                if (remove_page(trav, pgd) == -1)
                {
                    continue;
                }
                else
                {
                    printk("page removed\n");
                }

                asm volatile("invlpg (%0)" ::"r"(trav)
                             : "memory");
            }
            curr->vm_start = end;

            prev = curr;
            curr = curr->vm_next;

            return 0;
        }
        else if (start > curr->vm_start && end >= curr->vm_end && start < curr->vm_end)
        {
            // make a new vm_area and change the end of curr
            printk("case 3\n");
            u64 *pgd = (u64 *)osmap(current->pgd);
            u64 trav = start;
            for (trav = start; trav < curr->vm_end; trav += 4 * 1024)
            {
                if (remove_page(trav, pgd) == -1)
                {
                    continue;
                }
                else
                {
                    printk("page removed\n");
                }

                asm volatile("invlpg (%0)" ::"r"(trav)
                             : "memory");
            }
            curr->vm_end = start;

            prev = curr;
            curr = curr->vm_next;
        }
        else if (start > curr->vm_start && end < curr->vm_end)
        {
            // make 2 new vm_area and change the start and end of curr
            printk("case 4\n");
            u64 *pgd = (u64 *)osmap(current->pgd);
            u64 trav = start;

            for (trav = start; trav < end; trav += 4 * 1024)
            {
                if (remove_page(trav, pgd) == -1)
                {
                    continue;
                }
                else
                {
                    printk("page removed\n");
                }

                asm volatile("invlpg (%0)" ::"r"(trav)
                             : "memory");
            }
            struct vm_area *new_vm_area = os_alloc(sizeof(struct vm_area));
            new_vm_area->vm_start = end;
            new_vm_area->vm_end = curr->vm_end;
            new_vm_area->access_flags = curr->access_flags;
            new_vm_area->vm_next = curr->vm_next;
            curr->vm_next = new_vm_area;
            curr->vm_end = start;

            prev = curr;
            curr = curr->vm_next;
            stats->num_vm_area++;

            return 0;
        }
        else if (start < curr->vm_start && end <= curr->vm_start)
        {
            printk("case 5\n");
            return 0;
        }
        else if (start >= curr->vm_end && end > curr->vm_end)
        {
            printk("case 6\n");
            prev = curr;
            curr = curr->vm_next;
        }
    }

    return 0; // Success
}

/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{

    printk("inside vm_area_pagefault\n");
    // addr is not between MMAP_AREA_START and MMAP_AREA_END
    if (addr < MMAP_AREA_START || addr > MMAP_AREA_END)
    {
        return -1;
    }

    printk("divided the address\n");

    u16 pte_offset = (addr >> 12) & 0x1FF; // Bits 20-12
    u16 pmd_offset = (addr >> 21) & 0x1FF; // Bits 30-21
    u16 pud_offset = (addr >> 30) & 0x1FF; // Bits 39-30
    u16 pgd_offset = (addr >> 39) & 0x1FF; // Bits 47-39

    // iterate through the vm_area list and check if addr is in any of the vm_area
    struct vm_area *temp = current->vm_area;

    int check = 0;

    printk("going into the loop to find where addr is  \n");
    while (temp)
    {
        if (addr >= temp->vm_start && addr < temp->vm_end)
        {
            // check if the access flags are valid
            check = 1;
            break;
        }
        temp = temp->vm_next;
    }
    printk("the access flag of temp is: %x \n", temp->access_flags);

    if (check == 0)
    {
        return -EINVAL;
    }
    if ((error_code != 0x4) && (error_code != 0x6) && (error_code != 0x7))
    {
        return -1;
    }

    printk("checking the error code present\n");
    if (error_code == 0x4)
    {
        printk("in error code 4\n");
        // no physical page mapped to the virtual address so we have to map it by allocating a new pfn and mapping it to the virtual address
        // get the pgd entry of the virtual address using osmap
        u64 *pgd = (u64 *)osmap(current->pgd);
        if (pgd == NULL)
        {
            return -1;
        }

        u64 pgd_t = *(pgd + pgd_offset);

        if ((pgd_t & 1) == 0)
        {
            u64 pfn_new = (u64)os_pfn_alloc(OS_PT_REG);
            if (pfn_new == 0)
            {
                return -EINVAL;
            }

            // change the left 52 bit of pgd_t with *pfn bits
            pgd_t = (pfn_new << 12) | (pgd_t & 0xFFF);
            pgd_t = pgd_t | 1 | 8 | 16;
            *(pgd + pgd_offset) = pgd_t;
        }

        // get the pud entry of the virtual address using pud_offset and pgd_t
        u64 *pud = (u64 *)osmap(pgd_t >> 12);
        if (pud == NULL)
        {
            return -1;
        }

        u64 pud_t = *(pud + pud_offset);

        if ((pud_t & 1) == 0)
        {
            u64 pfn_new = (u64)os_pfn_alloc(OS_PT_REG);
            if (pfn_new == 0)
            {
                return -EINVAL;
            }

            // change the left 52 bit of pud_t with *pfn bits
            pud_t = (pfn_new << 12) | (pud_t & 0xFFF);
            pud_t = pud_t | 1 | 8 | 16;
            *(pud + pud_offset) = pud_t;
        }

        // get the pmd entry of the virtual address using pmd_offset and pud_t
        u64 *pmd = (u64 *)osmap(pud_t >> 12);
        if (pmd == NULL)
        {
            return -1;
        }

        u64 pmd_t = *(pmd + pmd_offset);

        if ((pmd_t & 1) == 0)
        {
            u64 pfn_new = (u64)os_pfn_alloc(OS_PT_REG);
            if (pfn_new == 0)
            {
                return -EINVAL;
            }

            // change the left 52 bit of pmd_t with *pfn bits
            pmd_t = (pfn_new << 12) | (pmd_t & 0xFFF);
            pmd_t = pmd_t | 1 | 8 | 16;
            *(pmd + pmd_offset) = pmd_t;
        }

        // get the pte entry of the virtual address using pte_offset and pmd_t
        u64 *pte = (u64 *)osmap(pmd_t >> 12);
        if (pte == NULL)
        {
            return -1;
        }

        u64 pte_t = *(pte + pte_offset);

        if ((pte_t & 1) == 0)
        {
            u64 pfn_new = (u64)os_pfn_alloc(USER_REG);
            if (pfn_new == 0)
            {
                return -EINVAL;
            }

            // change the left 52 bit of pte_t with *pfn bits
            pte_t = (pfn_new << 12) | (pte_t & 0xFFF);
            pte_t = pte_t | 1 | 16;
            *(pte + pte_offset) = pte_t;
        }

        return 1;
    }
    else if (error_code == 0x6)
    {
        printk("in error code 6\n");
        // no physical page mapped to the virtual address so we have to map it by allocating a new pfn and mapping it to the virtual address
        // get the pgd entry of the virtual address using osmap
        u64 *pgd = (u64 *)osmap(current->pgd);
        if (pgd == NULL)
        {
            return -1;
        }

        u64 pgd_t = *(pgd + pgd_offset);

        if ((pgd_t & 1) == 0)
        {
            printk("pgd_t present bit is 0\n");
            u64 pfn_new = (u64)os_pfn_alloc(OS_PT_REG);
            if (pfn_new == 0)
            {
                return -EINVAL;
            }

            // change the left 52 bit of pgd_t with *pfn bits
            pgd_t = (pfn_new << 12) | (pgd_t & 0xFFF);
            pgd_t = pgd_t | 1 | 8 | 16;
            *(pgd + pgd_offset) = pgd_t;
        }

        // get the pud entry of the virtual address using pud_offset and pgd_t
        u64 *pud = (u64 *)osmap(pgd_t >> 12);
        if (pud == NULL)
        {
            return -1;
        }

        u64 pud_t = *(pud + pud_offset);

        if ((pud_t & 1) == 0)
        {
            printk("pud_t present bit is 0\n");
            u64 pfn_new = (u64)os_pfn_alloc(OS_PT_REG);
            if (pfn_new == 0)
            {
                return -EINVAL;
            }

            // change the left 52 bit of pud_t with *pfn bits
            pud_t = (pfn_new << 12) | (pud_t & 0xFFF);
            pud_t = pud_t | 1 | 8 | 16;
            *(pud + pud_offset) = pud_t;
        }

        // get the pmd entry of the virtual address using pmd_offset and pud_t
        u64 *pmd = (u64 *)osmap(pud_t >> 12);
        if (pmd == NULL)
        {
            return -1;
        }

        u64 pmd_t = *(pmd + pmd_offset);

        if ((pmd_t & 1) == 0)
        {
            printk("the value of pmd_t is %x\n", pmd_t);
            printk("pmd_t present bit is 0\n");
            u64 pfn_new = (u64)os_pfn_alloc(OS_PT_REG);
            if (pfn_new == 0)
            {
                return -EINVAL;
            }
            printk("the value of pfn_new is %x\n", pfn_new);
            pmd_t = (pfn_new << 12) | (pmd_t & 0xFFF);
            pmd_t = pmd_t | 1 | 8 | 16;
            *(pmd + pmd_offset) = pmd_t;

            printk("the value of pmd_t after changing is %x\n", pmd_t);
        }

        // get the pte entry of the virtual address using pte_offset and pmd_t

        u64 *pte = (u64 *)osmap(pmd_t >> 12);
        printk("the value of pte is %x\n", pte);
        if (pte == NULL)
        {
            return -1;
        }

        u64 pte_t = *(pte + pte_offset);

        if ((pte_t & 1) == 0)
        {
            printk("pte_t present bit is 0\n");
            u64 pfn_new = (u64)os_pfn_alloc(USER_REG);
            if (pfn_new == 0)
            {
                return -EINVAL;
            }

            // change the left 52 bit of pte_t with *pfn bits
            pte_t = (pfn_new << 12) | (pte_t & 0xFFF);
            pte_t = pte_t | 1 | 8 | 16;
            *(pte + pte_offset) = pte_t;
        }

        return 1;
    }
    else if (error_code == 0x7)
    {
        printk("in error code 7\n");
        printk("the access flag of temp is: %x \n", temp->access_flags);
        if (temp->access_flags == PROT_READ)
        {
            return -1;
        }
        handle_cow_fault(current, addr, temp->access_flags);

        return 1;
    }
    else
    {
        return -1;
    }
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork()
{
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
    /* Do not modify above lines
     *
     * */
    /*--------------------- Your code [start]---------------*/
    new_ctx->type = ctx->type;
    new_ctx->used_mem = ctx->used_mem;
    new_ctx->state = ctx->state;
    new_ctx->os_rsp = ctx->os_rsp;
    new_ctx->regs = ctx->regs;
    new_ctx->pending_signal_bitmap = ctx->pending_signal_bitmap;
    new_ctx->ticks_to_alarm = ctx->ticks_to_alarm;
    new_ctx->ticks_to_sleep = ctx->ticks_to_sleep;
    new_ctx->alarm_config_time = ctx->alarm_config_time;
    new_ctx->ctx_threads = ctx->ctx_threads;

    for (int i = 0; i < MAX_MM_SEGS; i++)
    {
        new_ctx->mms[i].start = ctx->mms[i].start;
        new_ctx->mms[i].next_free = ctx->mms[i].next_free;
        new_ctx->mms[i].access_flags = ctx->mms[i].access_flags;
        new_ctx->mms[i].end = ctx->mms[i].end;
        
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        new_ctx->files[i] = ctx->files[i];
    }

    for (int i = 0; i < CNAME_MAX; i++)
    {
        new_ctx->name[i] = ctx->name[i];
    }

    for (int i = 0; i < MAX_SIGNALS; i++)
    {
        new_ctx->sighandlers[i] = ctx->sighandlers[i];
    }

    // copying the vm_area linked list using os_alloc and loop
    struct vm_area *temp = ctx->vm_area;
    struct vm_area *new_vm_area = os_alloc(sizeof(struct vm_area));
    new_ctx->vm_area = new_vm_area;
    while (temp->vm_next != NULL)
    {
        new_vm_area->vm_start = temp->vm_start;
        new_vm_area->vm_end = temp->vm_end;
        new_vm_area->access_flags = temp->access_flags;
        new_vm_area->vm_next = os_alloc(sizeof(struct vm_area));
        new_vm_area = new_vm_area->vm_next;
        temp = temp->vm_next;
    }

    new_ctx->ppid = ctx->pid;

    pid = new_ctx->pid;

    

    u64 pfn = os_pfn_alloc(OS_PT_REG);
    if (pfn == 0)
    {
        return -1;
    }
    new_ctx->pgd = pfn;

   

    for(int i=0;i<MAX_MM_SEGS;i++){
        u64 start, end;
        start = new_ctx->mms[i].start;
        if(i=MM_SEG_STACK){
            end = new_ctx->mms[i].end;
        }
        else{
            end = new_ctx->mms[i].next_free;
        }


        for(int j=start;j <end;j+=(4*1024)){
            
            u64* pte_t_parent= table_walk(j,ctx);
            
            if(pte_t_parent==NULL){
                continue;
            }
            else{

                if((*pte_t_parent & 1)==0){
                    continue;
                }

                //making write bit of pte_t_parent =0
                *pte_t_parent=*pte_t_parent -(*pte_t_parent & 8);
                u64* pte_t_child= table_walk(j,new_ctx);
                *pte_t_child=*pte_t_parent;

                //increasing referance count of pte of parent using get_pfn
                get_pfn(*pte_t_parent>>12);


            }
        }

    }



    // do the same as above but for vm_area linked list that is check if that vm_area is physicaly mapped in ctx if yes map it in new_ctx otherwise continue
    struct vm_area *temp_1 = ctx->vm_area;
    while (temp_1->vm_next != NULL)
    {   
        u64 start, end;
        start = temp_1->vm_start;
        end = temp_1->vm_end;
        for (int i = start; i < end; i += (4 * 1024))
        {
            u64 *pte_t_parent = table_walk(i, ctx);
            if (pte_t_parent == NULL)
            {
                continue;
            }
            else
            {
                if ((*pte_t_parent & 1) == 0)
                {
                    continue;
                }

                //making write bit of pte_t_parent =0
                *pte_t_parent = *pte_t_parent - (*pte_t_parent & 8);
                u64 *pte_t_child = table_walk(i, new_ctx);
                *pte_t_child = *pte_t_parent;

                //increasing referance count of pte of parent using get_pfn
                get_pfn(*pte_t_parent >> 12);
            }
        }
        temp_1 = temp_1->vm_next;
    }
    

    

    /*--------------------- Your code [end] ----------------*/

    /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}

/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data)
 * it is called when there is a CoW violation in these areas.
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{
    printk("inside cow_fault\n");
    if(access_flags==PROT_READ){
        return -1;
    }
    if(vaddr<MMAP_AREA_START || vaddr>MMAP_AREA_END){
        return -1;
    }

    u64* pte_t=table_walk(vaddr,current);
    if(get_pfn_refcount(*pte_t>>12)==1){
        *pte_t=*pte_t | 8;
        
    }
    else{
        if(*pte_t & 0x1 == 0){
            return 1;
        }
        u64 pfn_new = (u64)os_pfn_alloc(USER_REG);
        if (pfn_new == 0)
        {
            return -EINVAL;
        }
        memcpy((char *)(osmap(pfn_new)), (char *)(osmap(*pte_t >> 12)), 4096);
        put_pfn(*pte_t >> 12);
        // change the left 52 bit of pte_t with *pfn bits
        *pte_t = (pfn_new << 12) | (*pte_t & 0xFFF);
        *pte_t = *pte_t | 1 | 8 | 16;
        
        
    }

    asm volatile("invlpg (%0)" ::"r"(vaddr): "memory");
    
    return -1;
}