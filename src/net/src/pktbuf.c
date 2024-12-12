#include "pktbuf.h"
#include "dbg.h"
#include "mblock.h"

static pktblk_t pktblk_buffer[PKTBUF_BLK_COUNT];
static pktbuf_t pktbuf_buffer[PKTBUF_BUF_COUNT];
//list to store the allocated pktblk
static mblock_t pktblk_list_mblock;
//list to store the allocated pktbuf
static mblock_t pktbuf_list_mblock;
static nlocker_t pktbuf_locker;

static inline int calculate_free_space(pktblk_t* pktblk) {
    return (int)(pktblk->payload + PKTBUF_BLK_SIZE - pktblk->head - pktblk->used_blk_size);
}


#if DEBUG_DISP_ENABLED(DBG_PKTBUF)
static void display_check_buf(pktbuf_t *pktbuf) {
    if (!pktbuf) {
        dbg_error(DBG_PKTBUF, "pktbuf is null\n");
        return;
    }

    plat_printf("pktbuf: %p, total_size: %d\n", pktbuf, pktbuf->total_size);
    nlist_t *blk_list = &pktbuf->blk_list;
    nlist_iterator_t iterator = nlist_iterator(blk_list);
    int alloced_blk_size = 0;
    while (nlist_iterator_has_next(&iterator)) {
        pktblk_t *pktblk = (pktblk_t *)nlist_iterator_next(&iterator);

        if (pktblk->head < pktblk->payload || pktblk->head > pktblk->payload + PKTBUF_BLK_SIZE) {
            dbg_error(DBG_PKTBUF, "bad block head, pktblk: %p, head: %p, payload: %p, PKTBUF_BLK_SIZE: %d\n", pktblk, pktblk->head, pktblk->payload, PKTBUF_BLK_SIZE);
        }

        plat_printf("pktblk: %p,  blk head: %p\n", pktblk, pktblk->head);

        int blk_head_to_payload = (int)(pktblk->head - pktblk->payload);
        plat_printf("blk_head_to_payload: %d\n", blk_head_to_payload);

        int used_blk_size = pktblk->used_blk_size;
        plat_printf("used_blk_size: %d\n", used_blk_size);

        int free_space = calculate_free_space(pktblk);
        plat_printf("free_space: %d\n", free_space);
        plat_printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
        int blk_total_size = pktblk->used_blk_size + blk_head_to_payload + free_space;
        if (blk_total_size != PKTBUF_BLK_SIZE) {
            dbg_error(DBG_PKTBUF, "bad block size, pktblk: %p, blk_total_size: %d, PKTBUF_BLK_SIZE: %d\n", pktblk, blk_total_size, PKTBUF_BLK_SIZE);
        }
        alloced_blk_size += pktblk->used_blk_size;
    }
    if (alloced_blk_size != pktbuf->total_size) {
        dbg_error(DBG_PKTBUF, "bad total size, pktbuf: %p, alloced_blk_size: %d, pktbuf->total_size: %d\n", pktbuf, alloced_blk_size, pktbuf->total_size);
    }

}
#else
#define display_check_buf(pktbuf)
#endif

net_err_t pktbuf_init(void) {
    dbg_info(DBG_PKTBUF, "pktbuf_init\n");

    nlocker_init(&pktbuf_locker, NLOCKER_THREAD);
    mblock_init(&pktbuf_list_mblock, pktbuf_buffer, sizeof(pktbuf_t), PKTBUF_BUF_COUNT, NLOCKER_THREAD);
    //assign num of PKTBUF_BLK_COUNT pktblk_t to fill a list maintained by mblock
    mblock_init(&pktblk_list_mblock, pktblk_buffer, sizeof(pktblk_t), PKTBUF_BLK_COUNT, NLOCKER_THREAD);
    dbg_info(DBG_PKTBUF, "pktbuf_init end\n");
    return NET_ERR_OK;
}

static pktblk_t *pktblk_alloc_internal(void) {
    pktblk_t *pktblk = (pktblk_t *)mblock_alloc(&pktblk_list_mblock, -1);
    if (pktblk) {
        pktblk->used_blk_size = 0;
        pktblk->head = (uint8_t *)0;
        nlist_node_init(&pktblk->node);
    }
    return pktblk;
}
static void pktblock_list_free(nlist_t * blk_list) {
    nlist_iterator_t iterator = nlist_iterator(blk_list);
    while (nlist_iterator_has_next(&iterator)) {
        pktblk_t *pktblk = (pktblk_t *)nlist_iterator_next(&iterator);
        mblock_free(&pktblk_list_mblock, pktblk);
    }
}
static void pktblock_list_free_from_head(nlist_node_t *head) {
    nlist_node_t *node = head;
    while (node) {
        pktblk_t *pktblk = container_of(node, pktblk_t, node);
        nlist_node_t *next = nlist_node_next(node);
        mblock_free(&pktblk_list_mblock, pktblk);
        node = next;
    }
}

//size: the size of coming pktbuf, alloc a list with fixed block size to store the incoming pktbuf
//append_way: 1: append to head, 0: append to tail
pktblk_t *pktblk_list_alloc(int size, int append_way) {
    pktblk_t *pktblk_chain_head = (pktblk_t*)0;
    pktblk_t *pktblk_chain_tail = (pktblk_t*)0;
    while (size) {
        pktblk_t *pktblk = pktblk_alloc_internal();
        if (!pktblk) {
            dbg_error(DBG_PKTBUF, "pktbuf_alloc_list: pktblk_alloc_internal failed, used:%d, current block buffer capacity:%d\n", pktblk_list_mblock.used,pktblk_list_mblock.size);

            //todo: free all allocated pktblk
            pktblock_list_free_from_head(&pktblk_chain_head->node);
            return (pktblk_t *)0;
        }

        int current_size = 0;
        //1: append to head, 0: append to tail
        if (append_way) {
            if (!pktblk_chain_head && !pktblk_chain_tail) {
                //pktblk_chain_tail = pktblk;
                pktblk_chain_head = pktblk;
            }
            current_size = size > PKTBUF_BLK_SIZE ? PKTBUF_BLK_SIZE : size;
            pktblk->used_blk_size = current_size;
            pktblk->head = pktblk->payload + PKTBUF_BLK_SIZE - current_size;
            if (pktblk_chain_tail) {
                nlist_node_link(&pktblk->node, &pktblk_chain_tail->node);
            }
            pktblk_chain_tail = pktblk;
        } else {
            if (!pktblk_chain_head && !pktblk_chain_tail) {
                pktblk_chain_head = pktblk;
                //pktblk_chain_tail = pktblk;
            }
            current_size = size > PKTBUF_BLK_SIZE ? PKTBUF_BLK_SIZE : size;
            pktblk->used_blk_size = current_size;
            pktblk->head = pktblk->payload;
            if (pktblk_chain_tail) {
                nlist_node_link(&pktblk_chain_tail->node, &pktblk->node);

            }
            pktblk_chain_tail = pktblk;
        }
        size -= current_size;
        dbg_info(DBG_PKTBUF,"alloc one pktblk, pkt waiting to be alloced:%d, block list used:%d, block buffer capacity:%d\n", size, pktblk_list_mblock.used, pktblk_list_mblock.size);
    }
    return pktblk_chain_head;
}

static void pktbuf_insert_blk_list(pktbuf_t *pktbuf, pktblk_t *pktblk, int append_way) {
    nlist_node_t *pktblk_node_current = &pktblk->node;
    if (!append_way) {
        while (pktblk_node_current) {
            //pktblk_t *next = (pktblk_t *)nlist_node_next(pktblk_node_current);
            nlist_node_t *next = nlist_node_next(pktblk_node_current);
            nlist_append(&pktbuf->blk_list, pktblk_node_current);
            pktbuf->total_size += container_of(pktblk_node_current, pktblk_t, node)->used_blk_size;
            dbg_info(DBG_MBLOCK,"current pktbuf total size: %d\n", pktbuf->total_size);
            pktblk_node_current = next;
        }
    } else {
        while (pktblk_node_current) {
            //pktblk_t *next = (pktblk_t *)nlist_node_next(pktblk_node_current);
            nlist_node_t *pre = nlist_node_pre(pktblk_node_current);
            nlist_append_head(&pktbuf->blk_list, pktblk_node_current);
            pktbuf->total_size += container_of(pktblk_node_current, pktblk_t, node)->used_blk_size;
            dbg_info(DBG_MBLOCK,"current pktbuf total size: %d\n", pktbuf->total_size);
            pktblk_node_current = pre;
        }
    }
}

pktbuf_t *pktbuf_alloc(int size, int append_way) {
    pktbuf_t *pktbuf = (pktbuf_t *)mblock_alloc(&pktbuf_list_mblock, -1);
    if (!pktbuf) {
        dbg_error(DBG_PKTBUF, "pktbuf_alloc: mblock_alloc failed, used:%d, current capacity:%d\n", pktbuf_list_mblock.used, pktbuf_list_mblock.size);
        return (pktbuf_t *)0;
    }
    pktbuf->total_size = 0;
    nlist_init(&pktbuf->blk_list);
    nlist_node_init(&pktbuf->node);

    if (size) {
        pktblk_t *pktblk = pktblk_list_alloc(size, append_way);
        if (!pktblk) {
            mblock_free(&pktbuf_list_mblock, pktbuf);
            return (pktbuf_t *)0;
        }
        pktbuf_insert_blk_list(pktbuf, pktblk, append_way);
    }
    display_check_buf(pktbuf);
    return pktbuf;
}



void pktbuf_free(pktbuf_t *pktbuf) {
    pktblock_list_free(&pktbuf->blk_list);
    mblock_free(&pktbuf_list_mblock, pktbuf);

}
