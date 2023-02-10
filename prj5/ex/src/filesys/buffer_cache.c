#include "filesys/buffer_cache.h"
#include "threads/palloc.h"
#include <string.h>
#include <debug.h>

#define BUFFER_CACHE_ENTRIES 64

static struct buffer_cache_entry buffer_head[BUFFER_CACHE_ENTRIES];
static struct buffer_cache_entry *clock_hand;
static struct lock buffer_cache_lock;

void bc_init(void)
{
    //lock_init(&buffer_cache_lock);
    for (int i = 0; i != BUFFER_CACHE_ENTRIES; i++)
    {
        memset(&buffer_head[i], 0, sizeof(struct buffer_cache_entry));
        lock_init(&(&buffer_head[i])->lock);
    }
    clock_hand = buffer_head;
}

void bc_term(void)
{
    bc_flush_all_entries();
}

bool bc_read(block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunk_size, int sector_ofs)
{
    /* sector_idx를 buffer_head에서 검색 (bc_lookup 함수 이용) */
    /* 검색 결과가 없을 경우, 디스크 블록을 캐싱 할 buffer entry의 buffer_head를 구함 (bc_select_victim 함수 이용) */
    /* block_read 함수를 이용해, 디스크 블록 데이터를 buffer cache 로 read */
    /* memcpy 함수를 통해, buffer에 디스크 블록 데이터를 복사 */
    /* buffer_head의 clock bit을 setting */

    struct buffer_cache_entry *cur = bc_lookup(sector_idx);

    if (cur == NULL)
    {
        cur = bc_select_victim();
        bc_flush_entry(cur);
        cur->valid_bit = true;
        cur->dirty_bit = false;
        cur->disk_sector = sector_idx;
        block_read(fs_device, sector_idx, cur->buffer);
    }

    cur->reference_bit = true;
    memcpy(buffer + bytes_read, cur->buffer + sector_ofs, chunk_size);
    lock_release(&cur->lock);
    return true;
}

bool bc_write(block_sector_t sector_idx, void *buffer, off_t offset, int chunk_size, int sector_ofs)
{
    /* sector_idx를 buffer_head에서 검색하여 buffer에 복사(구현)*/
    /* update buffer_head (구현) */

    struct buffer_cache_entry *cur = bc_lookup(sector_idx);

    if (cur == NULL)
    {
        cur = bc_select_victim();
        bc_flush_entry(cur);
        cur->valid_bit = true;
        cur->disk_sector = sector_idx;
        block_read(fs_device, sector_idx, cur->buffer);
    }

    cur->reference_bit = true;
    cur->dirty_bit = true;
    memcpy(cur->buffer + sector_ofs, buffer + offset, chunk_size);
    lock_release(&cur->lock);
    return true;
}

struct buffer_cache_entry *bc_lookup(block_sector_t sector)
{
    /* buffe_head를 순회하며, 전달받은 sector 값과 동일한 sector 값을 갖는 buffer cache entry가 있는지 확인 */
    /* 성공 : 찾은 buffer_head 반환, 실패 : NULL */

    for (int i = 0; i < BUFFER_CACHE_ENTRIES; i++)
    {

        if (buffer_head[i].valid_bit && buffer_head[i].disk_sector == sector)
        {
            lock_acquire(&(&buffer_head[i])->lock);
            return &buffer_head[i];
        }
    }
    return NULL;
}

struct buffer_cache_entry *bc_select_victim(void)
{
    /* clock 알고리즘을 사용하여 victim entry를 선택 */
    /* buffer_head 전역변수를 순회하며 clock_bit 변수를 검사 */
    /* 선택된 victim entry가 dirty일 경우, 디스크로 flush */
    /* victim entry에 해당하는 buffer_head 값 update */
    /* victim entry를 return */
    int count = 0;
    int idx = (clock_hand - buffer_head) / sizeof(struct buffer_cache_entry);

    if (idx == BUFFER_CACHE_ENTRIES)
        idx = 0;

    for (; idx < BUFFER_CACHE_ENTRIES; idx++)
    {
        lock_acquire(&(buffer_head[idx].lock));
        if (!buffer_head[idx].valid_bit)
        {
            clock_hand = &buffer_head[idx + 1];
            return &buffer_head[idx];
        }
        if (!buffer_head[idx].reference_bit)
        {
            clock_hand = &buffer_head[idx + 1];
            return &buffer_head[idx];
        }
        buffer_head[idx].reference_bit = false;
        lock_release(&(buffer_head[idx].lock));

        if (idx == BUFFER_CACHE_ENTRIES - 1)
            idx = 0;
    }
}

void bc_flush_entry(struct buffer_cache_entry *p_flush_entry)
{
    if (!p_flush_entry->valid_bit)
        return;
    if (!p_flush_entry->dirty_bit)
        return;
    p_flush_entry->dirty_bit = false;
    block_write(fs_device, p_flush_entry->disk_sector, p_flush_entry->buffer);
}

void bc_flush_all_entries(void)
{
    for (int i = 0; i < BUFFER_CACHE_ENTRIES; i++)
    {
        lock_acquire(&(buffer_head[i].lock));
        bc_flush_entry(&buffer_head[i]);
        lock_release(&(buffer_head[i].lock));
    }
}
