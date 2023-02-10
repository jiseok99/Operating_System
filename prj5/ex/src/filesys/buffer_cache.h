#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/synch.h"

struct buffer_cache_entry
{
    bool dirty_bit;
    bool valid_bit;
    bool reference_bit;
    block_sector_t disk_sector;
    struct lock lock;
    uint8_t buffer[BLOCK_SECTOR_SIZE];
};

void bc_init(void);
void bc_term(void);
bool bc_read(block_sector_t, void *, off_t, int, int);
bool bc_write(block_sector_t, void *, off_t, int, int);
struct buffer_cache_entry *bc_lookup(block_sector_t);
struct buffer_cache_entry *bc_select_victim(void);
void bc_flush_entry(struct buffer_cache_entry *);
void bc_flush_all_entries(void);