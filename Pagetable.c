#include "os.h"
#include "math.h"

/*
 * There are (64-7-12)/9 = 45/9 = 5 chuncks/blocks that an address is being splitted into, therefore 5 levels.
 *
 * Everytime we alloc new page frame, to get its pointer we need to send the page number * 4096 (page size)
 * I forgot and wasted time :(
*/



void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn) {

    uint64_t ONES = pow(2,9)-1;
    uint64_t* ptPointer = phys_to_virt(pt * 4096); //Free it later

    if(ppn == NO_MAPPING) {
        for(int i = 4; i >= 0; i--) {
            uint64_t chunck = vpn&(ONES << (9*i)); //we start at bit 0 as we get the PAGE NUMBER so no offset here/
            chunck >>= 9*i;

            if(i && !(ptPointer[chunck] & 1)) { //i.e. no continuation
                return;
            }

            if(i>0) {
                uint64_t address = (ptPointer[chunck] >> 12) << 12; //zeroing the smallest 12 bits.
                ptPointer = phys_to_virt(address);
            }
            else { //Only in last level we nullify the pointer
                ptPointer[chunck] = NO_MAPPING;
            }
        }
    }
    else {
        for(int i = 4; i >= 0; i--) {
            uint64_t chunck = vpn&(ONES << (9*i));
            chunck >>= 9*i;

            if(!(ptPointer[chunck] & 1)) { //i.e. no continuation
                uint64_t address = alloc_page_frame(); //New physical address for new pagetable
                ptPointer[chunck] = (address * 4096) | 1; //| 1 for valid bit
            }

            if(i>0) {
                uint64_t address = (ptPointer[chunck] >> 12) << 12; //zeroing the smallest 12 bits.
                ptPointer = phys_to_virt(address);
            }
            else { //Only in last level we define the pointer to be the ppn
                ptPointer[chunck] = ppn;
            }
        }
    }
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn) {
    uint64_t ONES = pow(2,9)-1;
    uint64_t* ptPointer = phys_to_virt(pt*4096); //Free it later
    uint64_t result = NO_MAPPING;
    for(int i = 4; i >= 0; i--) {
        uint64_t chunck = vpn&(ONES << (9*i));
        chunck >>= 9*i;

        if(i && !(ptPointer[chunck] & 1)) { //i.e. no continuation
            break;
        }

        if(i>0) {
            uint64_t address = (ptPointer[chunck] >> 12) << 12; //zeroing the smallest 12 bits.
            ptPointer = phys_to_virt(address);
        }
        else {
            result = ptPointer[chunck];
        }
    }
    return result;
}
