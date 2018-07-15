#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <tgmath.h>


//  ######   PRIVATE FUNCTIONS DECLARATION   ######

void get_address_and_remainder(uint64_t index, int index_bit_size, uint64_t &address,
                               uint64_t &remainder);

uint64_t pad_with_offset(uint64_t addr);

bool check_invalid_input(uint64_t virtualAddress);

int build_route_for_read_and_write(uint64_t virtualAddress, word_t *value, word_t &final_addr);

void dfs(
        bool &return_address_flag,
        unsigned int PM_node_arr_len,
        int PM_root_flag,
        int frame_idx,
        int height,
        int &max_frames,
        int &evict_frame,
        int &father_evict,
        int cur_father_evict,
        int &frame_evict_idx,
        int cur_frame_evict_idx,
        uint64_t PM_node_addr,
        uint64_t &min_cyclic_page,
        uint64_t cur_tree_addr,
        uint64_t target_page,
        uint64_t &found_addr,
        word_t father_frame,
        word_t &address_return
);


//  ######   IMPLEMENTATION   ######
//           ==============


/**
 * receives the index and the index's width, and 2 pointers.
 * The function puts the address (new index) and remainder into the pointers
 */
void get_address_and_remainder(uint64_t index, int index_bit_size, uint64_t &address,
                               uint64_t &remainder)
{
    // calculate address mask size, according to if the index bits size
    // divides without a remainder
    int to_subtract_address_mask;
    if (index_bit_size % OFFSET_WIDTH != 0)
    {
        to_subtract_address_mask = index_bit_size % OFFSET_WIDTH;
    }
    else
    {
        to_subtract_address_mask = OFFSET_WIDTH;
    }
    int address_mask = index_bit_size - to_subtract_address_mask;
    // same for remainder - calculate mask and remainder
    int remainder_mask_power = OFFSET_WIDTH;
    if (index_bit_size % OFFSET_WIDTH != 0)
    {
        remainder_mask_power = index_bit_size % OFFSET_WIDTH;
    }
    auto remainder_mask = static_cast<uint64_t>(pow(2, index_bit_size - remainder_mask_power) - 1);
    address = index >> address_mask;
    remainder = (uint64_t) index & remainder_mask;
}


/**
 * Function "pads" the address with 0's, as much as the offset.
 * This is done so that the next level address can be put in.
 */
uint64_t pad_with_offset(uint64_t addr)
{
    return addr << OFFSET_WIDTH;
}

/**
 * Verify the input is valid
 */
bool check_invalid_input(uint64_t virtualAddress)
{
    return (VIRTUAL_ADDRESS_WIDTH < log2(virtualAddress));
}

/**
 * A given function
 */
void clearTable(uint64_t frameIndex)
{
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

/**
 * A given function
 */
void VMinitialize()
{
    clearTable(0);
}


/**
 * Function reads from the virtual memory, and creates the path for the exact cell if not existing.
 * @param virtualAddress: The address to read from
 * @param value: a pointer to the value
 * @return 1 iff success
 */
int VMread(uint64_t virtualAddress, word_t *value)
{
    word_t final_addr;
    uint64_t offset = virtualAddress & (uint64_t) (std::pow(2, OFFSET_WIDTH) - 1);
    int route_success = build_route_for_read_and_write(virtualAddress, value, final_addr);

    if (!route_success)
    {
        return 0;
    }
    PMread(final_addr * PAGE_SIZE + offset, value);
    return 1;
}


/**
 * Function writes to the virtual memory, and creates the path for the exact cell if not existing.
 * @param virtualAddress: The address to write to
 * @param value: the value to write in memory
 * @return 1 iff success
 */
int VMwrite(uint64_t virtualAddress, word_t value)
{
    word_t final_addr;
    uint64_t offset = virtualAddress & (uint64_t) (std::pow(2, OFFSET_WIDTH) - 1);
    int route_success = build_route_for_read_and_write(virtualAddress, &value, final_addr);
    if (!route_success)
    {
        return 0;
    }
    PMwrite(final_addr * PAGE_SIZE + offset, value);
    return 1;
}


/**
 * This is the "heart" of the exercise. This function receives a virtual address, and iterates over
 * the tree that the frames and tables create. The function goes to the relevant leaf. If on the
 * way the path does not exist, the function finds a frame to evict (using DFS) or finds an empty
 * frame in the physical memory.
 * @return 0 upon failure, 1 upon success
 */
int build_route_for_read_and_write(uint64_t virtualAddress, word_t *value, word_t &final_addr)
{
    // verify validity
    if (check_invalid_input(virtualAddress))
    {
        return 0;
    }
    word_t cur_phy_addr = 0;
    word_t next_phy_addr;
    int index_width_size = VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH;
    bool is_root_of_offset_width = (VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH == 0);
    // the MSB is not always the size of the offset. Find the width of the root:
    unsigned int root_width_size;
    if (is_root_of_offset_width)
    {
        root_width_size = OFFSET_WIDTH;
    }
    else
    {
        root_width_size = VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH;
    }

    // define initial offset and remainder:
    auto mask = (uint64_t) (std::pow(2, OFFSET_WIDTH) - 1);
    uint64_t current_index = (virtualAddress & ~mask) >> (OFFSET_WIDTH);
    uint64_t frame_index = (virtualAddress & ~mask) >> (OFFSET_WIDTH);;

    // get the next address and remainder
    uint64_t address_within_frame, remainder;
    get_address_and_remainder(current_index, index_width_size, address_within_frame, remainder);
    current_index = remainder;
    // reduce the remainder index size
    index_width_size -= root_width_size;
    int last_iteration = TABLES_DEPTH - 1;
    // iterate down the tree towards the wanted address:
    for (int i = 0; i < TABLES_DEPTH; i++)
    {

        PMread(cur_phy_addr * PAGE_SIZE + address_within_frame, &next_phy_addr);
        // if found a non existing point throughout the route:
        if (next_phy_addr == 0)
        {
            bool return_addr_flag = false;
            int max_frame = 0;
            int frame_evict = 0;
            int father_evicted = 0;
            int father = 0;
            int inx_evicted = 0;
            int index_updated = 0;
            uint64_t min_page_num = 0;
            uint64_t add_tree = 0;
            uint64_t add_final = 0;

            dfs(
                    return_addr_flag,
                    root_width_size,
                    OFFSET_WIDTH - root_width_size,
                    0,
                    TABLES_DEPTH,
                    max_frame,
                    frame_evict,
                    father_evicted,
                    father,
                    inx_evicted,
                    index_updated,
                    0,
                    min_page_num,
                    add_tree,
                    current_index,
                    add_final,
                    cur_phy_addr,
                    next_phy_addr
            );

            //case 1: if no zeroed frame found:
            if (return_addr_flag == 0)
            {
                // case 2: evict a frame
                if (max_frame >= NUM_FRAMES - 1)
                {

                    PMevict((uint64_t) frame_evict, add_final);
                    next_phy_addr = frame_evict;
                    PMwrite((uint64_t) father_evicted * PAGE_SIZE + inx_evicted, 0);

                }
                // case 3: found an empty frame - initialize it and use it
                else
                {

                    next_phy_addr = max_frame + 1;
                }
            }
            auto cast_next_phy_addr = (uint64_t) next_phy_addr;
            clearTable(cast_next_phy_addr);
            if (i == last_iteration) // last iteration
            {
                PMrestore(cast_next_phy_addr, frame_index);
            }
            PMwrite(cur_phy_addr * PAGE_SIZE + address_within_frame, next_phy_addr);
        }
        cur_phy_addr = next_phy_addr;
        // get ready for next iteration
        get_address_and_remainder(current_index, index_width_size, address_within_frame,
                                  current_index);
        index_width_size = index_width_size - OFFSET_WIDTH;
    }

    final_addr = cur_phy_addr;
    return 1;

}


/**
 * DFS algorithm goes through the tree. If finds an initiated frame that is all zeros - returns it.
 * If finds that the max frame is not the max possible - returns the max found +1 for use.
 * If none of the above - finds the leaf with the biggest cyclic distance from the destined
 * address, and returns it for eviction.
 */
void dfs(
        bool &return_address_flag,
        unsigned int PM_node_arr_len,
        int PM_root_flag,
        int frame_idx,
        int height,
        int &max_frames,
        int &evict_frame,
        int &father_evict,
        int cur_father_evict,
        int &frame_evict_idx,
        int cur_frame_evict_idx,
        uint64_t PM_node_addr,
        uint64_t &min_cyclic_page,
        uint64_t cur_tree_addr,
        uint64_t target_page,
        uint64_t &found_addr,
        word_t father_frame,
        word_t &address_return
)
{
    address_return = -1;
    uint64_t new_add;
    uint64_t cyclic_dist;
    word_t value;
    int count_blank = 0;
    int dest_count = PAGE_SIZE;

    if (PM_root_flag)
    {
        dest_count = PM_root_flag;
    }
    // update max:
    max_frames = std::max(max_frames, frame_idx);
    // if in a leaf, calculate cyclic distance
    if (!height)
    {
        cyclic_dist = (uint64_t) std::min(abs((int) (target_page - cur_tree_addr)),
                                          (int) (NUM_PAGES) -
                                          abs((int) (target_page - cur_tree_addr)));
        if (cyclic_dist > min_cyclic_page)
        {
            found_addr = cur_tree_addr;
            evict_frame = frame_idx;
            father_evict = cur_father_evict;
            frame_evict_idx = cur_frame_evict_idx;
            min_cyclic_page = cyclic_dist;
        }
        return;
    }
    uint64_t new_add_old = pad_with_offset(cur_tree_addr);
    bool break_flag = false;

    // iterate over all sons, and go deeper into the recursion:
    for (int idx = 0; idx < dest_count; idx++)
    {
        new_add = new_add_old + idx;
        PMread(PM_node_addr + idx, &value);
        if (!value)
        {
            count_blank++;
        }

        else
        {
            dfs(
                    return_address_flag,
                    PM_node_arr_len,
                    0,
                    value,
                    height - 1,
                    max_frames,
                    evict_frame,
                    father_evict,
                    frame_idx,
                    frame_evict_idx,
                    idx,
                    value * (uint64_t) PAGE_SIZE,
                    min_cyclic_page,
                    new_add,
                    target_page,
                    found_addr,
                    father_frame,
                    address_return
            );
            if (return_address_flag)
            {
                if (address_return == value)
                {
                    PMwrite(PM_node_addr + idx, 0);
                }
                break_flag = true;
                break;
            }
        }
    }
    // determine which case 1 2 or 3 is relevant (i.e which pointers to use when the function returns)
    if (!break_flag && (count_blank == dest_count))
    {
        return_address_flag = (frame_idx < NUM_FRAMES) && (frame_idx != father_frame);
        address_return = return_address_flag * frame_idx + (return_address_flag - 1);
    }

}
