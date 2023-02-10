#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "page.h"
#include "threads/vaddr.h"

void vm_init(struct hash *vm)
{
    /* hash_init()으로 해시테이블 초기화 */
    /* 인자로 해시 테이블과 vm_hash_func과 vm_less_func 사용 */
    ASSERT(vm != NULL);
    hash_init(vm, vm_hash_func, vm_less_func, 0);
}

static unsigned vm_hash_func(const struct hash_elem *e, void *aux)
{
    /* hash_entry()로 element에 대한 vm_entry 구조체 검색 */
    /* hash_int()를 이용해서 vm_entry의 멤버 vaddr에 대한 해시값을 구하고 반환 */
    ASSERT(e != NULL);
    int temp = hash_entry(e, struct vm_entry, elem)->vaddr;
    return hash_int(temp);
}

static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b)
{
    /* hash_entry()로 각각의 element에 대한 vm_entry 구조체를 얻은 후 vaddr 비교 (b가 크다면 true, a가 크다면 false */
    ASSERT(a != NULL);
    ASSERT(b != NULL);

    int temp_a = hash_entry(a, struct vm_entry, elem)->vaddr;
    int temp_b = hash_entry(b, struct vm_entry, elem)->vaddr;
    return temp_a < temp_b;
}

bool insert_vme(struct hash *vm, struct vm_entry *vme)
{
    ASSERT(vm != NULL);
    ASSERT(vme != NULL);

    if (hash_insert(vm, vme) == NULL)
        return true;
    return false;
}

bool delete_vme(struct hash *vm, struct vm_entry *vme)
{
    ASSERT(vm != NULL);
    ASSERT(vme != NULL);

    if (hash_delete(vm, vme) != NULL)
        return true;
    return false;
}