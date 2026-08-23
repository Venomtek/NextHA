// Main bank code starts at 0x8400; 1K stack below it at 0x8000-0x83FF,
// both inside the dotn-allocated mmu4 page (see z88dk ls/128 example).
#pragma output CRT_ORG_MAIN = 0x8400
#pragma output REGISTER_SP = 0x8400

// Keep printf small: only these conversions are linked.
#pragma printf = "%s %c %u %d"

// One atexit slot (used to restore turbo / paging on exit).
#pragma output CLIB_EXIT_STACK_SIZE = 1

// Logical->physical page table must cover mmu4..mmu7.
#pragma output DOTN_LAST_PAGE = 11
#pragma output DOTN_NUM_EXTRA = 0
