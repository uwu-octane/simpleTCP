#include "mblock.h"
#include "dbg.h"
#include "cfg.h"

net_err_t mblock_init(mblock_t *mblock, void * mem, int blk_size, int blk_count, nlocker_type_t type){
    nlist_init(&mblock->free_list);
    uint8_t *buf = (uint8_t *)mem;
    //todo: have to modify to adapt the size
    if (blk_size < sizeof(nlist_node_t)) {
        dbg_warning(DBG_MBLOCK, "mblock_init: block size is too small");
        blk_size = sizeof(nlist_node_t);
        //return NET_ERR_INVALID;
    }

    for (int i = 0; i < blk_count; i++) {
        nlist_node_t *block_node = (nlist_node_t *)(buf + i * blk_size);
        nlist_node_init(block_node);
        nlist_append(&mblock->free_list, block_node);
    }

    nlocker_init(&mblock->locker, type);
    if (mblock->locker.type != NLOCKER_NONE) {
        mblock->alloc_sem = sys_sem_create(blk_count);
        if (mblock->alloc_sem == SYS_SEM_INVALID) {
            dbg_error(DBG_MBLOCK, "mblock_init: sys_sem_create failed");
            nlocker_destory(&mblock->locker);
            return NET_ERR_SYS;
        }
    }
    mblock->start = mem;
    mblock->size = blk_count;
    mblock->used = 0;
    //mblock->buf = (char *)mem;

    return NET_ERR_OK;
}

// 辅助函数：尝试从 free_list 分配块
void *mblock_try_allocate(mblock_t *mblock) {
    if (nlist_count(&mblock->free_list) == 0) {
        return (void *)0; // 没有可用块
    }

    // 获取并移除 free_list 的头节点
    nlist_node_t *block = nlist_head(&mblock->free_list);
    nlist_remove(&mblock->free_list, block);
    mblock->used++; // 增加已使用块的计数

    return (void *)block;
}


void *mblock_alloc(mblock_t *mblock, int wait_time) {
    // 如果没有等待时间或无锁保护
    if (wait_time < 0 || mblock->locker.type == NLOCKER_NONE) {
        nlocker_lock(&mblock->locker);

        // 尝试从 free_list 中获取可用块
        void *block = mblock_try_allocate(mblock);

        nlocker_unlock(&mblock->locker);
        return block;
    }

    // 使用信号量等待可用块
    if (sys_sem_wait(mblock->alloc_sem, wait_time) < 0) {
        return (void *)0; // 等待超时
    }

    nlocker_lock(&mblock->locker);
    void *block = mblock_try_allocate(mblock);
    nlocker_unlock(&mblock->locker);

    return block;
}

int mblock_freeblock_cnt(mblock_t *mblock) {
    nlocker_lock(&mblock->locker);
    int cnt = nlist_count(&mblock->free_list);
    //int cnt = mblock->used;
    nlocker_unlock(&mblock->locker);
    return cnt;
}

void mblock_free(mblock_t *mblock, void *block) {
    nlocker_lock(&mblock->locker);
    nlist_append(&mblock->free_list, (nlist_node_t *)block);
    mblock->used--;
    nlocker_unlock(&mblock->locker);

    if (mblock->locker.type != NLOCKER_NONE) {
        //通知等待的线程有可用块
        sys_sem_notify(mblock->alloc_sem);
    }
}

void mblock_destroy(mblock_t *mblock) {
    if (mblock->locker.type != NLOCKER_NONE) {
        sys_sem_free(mblock->alloc_sem);
        nlocker_destory(&mblock->locker);
    }

    if (mblock->used > 0) {
        dbg_warning(DBG_MBLOCK, "mblock_destroy: %d blocks are still in use", mblock->used);
    }

}