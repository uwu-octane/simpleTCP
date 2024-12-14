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

//free space at the tail of the pktblk
static int calculate_free_space(const pktblk_t* pktblk) {
    return (int)(pktblk->payload + PKTBUF_BLK_SIZE - pktblk->data_head - pktblk->used_blk_size);
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
    int index = 1;
    while (nlist_iterator_has_next(&iterator)) {
        plat_printf(">>>>>>>>>>>>>>>>>>>>Block Index:%d>>>>>>>>>>>>>>>>>>>>>>>>>\n", index);
        pktblk_t *pktblk = (pktblk_t *)nlist_iterator_next(&iterator);

        if (pktblk->data_head < pktblk->payload || pktblk->data_head > pktblk->payload + PKTBUF_BLK_SIZE) {
            dbg_error(DBG_PKTBUF, "bad block head, pktblk: %p, head: %p, payload: %p, PKTBUF_BLK_SIZE: %d\n", pktblk, pktblk->data_head, pktblk->payload, PKTBUF_BLK_SIZE);
        }

        plat_printf("pktblk: %p,  blk data head: %p\n", pktblk, pktblk->data_head);

        int blk_head_to_payload = (int)(pktblk->data_head - pktblk->payload);
        plat_printf("blk_head_to_payload: %d\n", blk_head_to_payload);

        int used_blk_size = pktblk->used_blk_size;
        plat_printf("used_blk_size: %d\n", used_blk_size);

        int free_space = calculate_free_space(pktblk);
        plat_printf("free_space: %d\n", free_space);

        int blk_total_size = pktblk->used_blk_size + blk_head_to_payload + free_space;
        if (blk_total_size != PKTBUF_BLK_SIZE) {
            dbg_error(DBG_PKTBUF, "bad block size, pktblk: %p, blk_total_size: %d, PKTBUF_BLK_SIZE: %d\n", pktblk, blk_total_size, PKTBUF_BLK_SIZE);
        }
        alloced_blk_size += pktblk->used_blk_size;
        plat_printf(">>>>>>>>>>>>>>>>>>>>Block Index:%d>>>>>>>>>>>>>>>>>>>>>>>>>\n", index++);
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
        pktblk->data_head = (uint8_t *)0;
        nlist_node_init(&pktblk->node);
    }
    return pktblk;
}

void pktblk_free(pktbuf_t *pktbuf, pktblk_t *pktblk) {
    nlist_node_t *node = &pktblk->node;
    nlist_t *blk_list = &pktbuf->blk_list;
    nlist_remove(blk_list, node);
    mblock_free(&pktblk_list_mblock, pktblk);
}

static void pktblock_list_free(nlist_t *blk_list) {
    // 使用移除函数将节点从链表中移出，然后再释放对应的内存
    while (!nlist_is_empty(blk_list)) {
        // 从链表头部移除一个节点
        nlist_node_t *node = nlist_remove_head(blk_list);
        pktblk_t *pktblk = container_of(node, pktblk_t, node);

        // 释放该pktblk
        mblock_free(&pktblk_list_mblock, pktblk);
    }
}
static void pktblock_list_free_from_node(nlist_node_t *head) {
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
    pktblk_t *pktblk_head = (pktblk_t*)0;
    pktblk_t *pktblk_tail = (pktblk_t*)0;
    while (size) {
        pktblk_t *pktblk = pktblk_alloc_internal();
        if (!pktblk) {
            dbg_error(DBG_PKTBUF, "pktbuf_alloc_list: pktblk_alloc_internal failed, used:%d, current block buffer capacity:%d\n", pktblk_list_mblock.used,pktblk_list_mblock.size);

            pktblock_list_free_from_node(&pktblk_head->node);
            return (pktblk_t *)0;
        }

        int current_size = (size > PKTBUF_BLK_SIZE) ? PKTBUF_BLK_SIZE : size;
        pktblk->used_blk_size = current_size;
        //1: append to head, 0: append to tail
        // 头插法：数据从块尾部开始写
        // 根据append_way决定data_head位置
        if (append_way == 1) {
            // 头插方式 => 数据从尾部开始写
            pktblk->data_head = pktblk->payload + (PKTBUF_BLK_SIZE - current_size);
        } else {
            // 尾插方式 => 数据从头部开始写
            pktblk->data_head = pktblk->payload;
        }

        // 将新的pktblk链接到链表的尾部（这里仅构造链表，不决定最终插入pktbuf的方式）
        if (!pktblk_head) {
            pktblk_head = pktblk_tail = pktblk;
        } else {
            nlist_node_link(&pktblk_tail->node, &pktblk->node);
            pktblk_tail = pktblk;
        }

        /*if (append_way) {
            pktblk->data_head = pktblk->payload + PKTBUF_BLK_SIZE - current_size;
            if (!pktblk_head && !pktblk_tail) {
                pktblk_head = pktblk_tail = pktblk;
            } else {
                // 非空链表，头插：新块在前，原head在后
                nlist_node_link(&pktblk->node, &pktblk_head->node);
                pktblk_head = pktblk;
            }

            /*if (!pktblk_head && !pktblk_tail) {
                //pktblk_chain_tail = pktblk;
                pktblk_head = pktblk;
            }
            pktblk->head = pktblk->payload + PKTBUF_BLK_SIZE - current_size;
            if (pktblk_tail) {
                nlist_node_link(&pktblk->node, &pktblk_tail->node);
            }
            pktblk_tail = pktblk;
        } else {
            // 尾插法：数据从块头部开始写
            pktblk->data_head = pktblk->payload;
            if (!pktblk_head && !pktblk_tail) {
                // 空链表
                pktblk_head = pktblk_tail = pktblk;
            } else {
                // 非空链表，尾插：原tail在前，新块在后
                nlist_node_link(&pktblk_tail->node, &pktblk->node);
                pktblk_tail = pktblk;
            }
            /*if (!pktblk_head && !pktblk_tail) {
                pktblk_head = pktblk;
                //pktblk_chain_tail = pktblk;
            }

            pktblk->head = pktblk->payload;
            if (pktblk_tail) {
                nlist_node_link(&pktblk_tail->node, &pktblk->node);

            }
            pktblk_tail = pktblk;
        }*/
        size -= current_size;
        dbg_info(DBG_PKTBUF,"alloc one pktblk, pkt waiting to be alloced:%d, block list used:%d, block buffer capacity:%d\n", size, pktblk_list_mblock.used, pktblk_list_mblock.size);
    }
    return pktblk_head;
}

//append_way: 1: append to head, 0: append to tail
static void pktbuf_insert_blk_list(pktbuf_t *pktbuf, pktblk_t *pktblk, int append_way) {
    nlist_node_t *pktblk_node_current = &pktblk->node;
    while (pktblk_node_current) {
        nlist_node_t *next = nlist_node_next(pktblk_node_current);

        // 根据append_way选择头插法或尾插法
        if (append_way == 1) {
            nlist_append_head(&pktbuf->blk_list, pktblk_node_current);
        } else {
            nlist_append(&pktbuf->blk_list, pktblk_node_current);
        }

        pktbuf->total_size += container_of(pktblk_node_current, pktblk_t, node)->used_blk_size;
        dbg_info(DBG_MBLOCK,"current pktbuf total size: %d\n", pktbuf->total_size);

        pktblk_node_current = next;
    }
}

pktbuf_t *pktbuf_alloc(int size, int append_way) {
    pktbuf_t *pktbuf = (pktbuf_t *)mblock_alloc(&pktbuf_list_mblock, -1);
    if (!pktbuf) {
        dbg_error(DBG_PKTBUF, "pktbuf_alloc: mblock_alloc failed, used:%d, current capacity:%d\n", pktbuf_list_mblock.used, pktbuf_list_mblock.size);
        return (pktbuf_t *)0;
    }
    pktbuf->total_size = 0;
    //pktbuf->append_way = append_way;
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

net_err_t pktbuf_add_header(pktbuf_t *pktbuf, int header_size, int continous) {
    pktblk_t * first_blk = pktbuf_first_block(pktbuf);

    int remaing_hader_space = (int)(first_blk->data_head - first_blk->payload);
    //if header_size is less than the remaining space in the first block, add the header to the first block
    if (header_size <= remaing_hader_space) {
        first_blk->data_head -= header_size;
        first_blk->used_blk_size += header_size;
        pktbuf->total_size += header_size;
        dbg_info(DBG_PKTBUF, "add header without new block");
        //display_check_buf(pktbuf);
        return NET_ERR_OK;
    }

    if (continous) {
        if (header_size >PKTBUF_BLK_SIZE) {
            dbg_error(DBG_PKTBUF, "header size is larger than block size, current block size: %d\n", PKTBUF_BLK_SIZE);
            return NET_ERR_INVALID;
        }
        //if header_size is larger than the remaining space in the first block,
        //allocate a new block to store the header by adding header at the end of the first block
        first_blk = pktblk_list_alloc(header_size, 1);
        if (!first_blk) {
            dbg_error(DBG_PKTBUF, "pktbuf_add_header: pktblk_list_alloc failed, used:%d, current capacity:%d\n",
                pktblk_list_mblock.used, pktblk_list_mblock.size);
            return NET_ERR_MEM;
        }

    } else {
        first_blk->data_head = first_blk->payload;
        first_blk->used_blk_size += remaing_hader_space;
        pktbuf->total_size += remaing_hader_space;
        header_size -= remaing_hader_space;
        first_blk = pktblk_list_alloc(header_size, 1);
        if (!first_blk) {
            dbg_error(DBG_PKTBUF, "pktbuf_add_header: pktblk_list_alloc failed, used:%d, current capacity:%d\n",
                pktblk_list_mblock.used, pktblk_list_mblock.size);
            return NET_ERR_MEM;
        }
    }
    pktbuf_insert_blk_list(pktbuf, first_blk, 1);
    dbg_info(DBG_PKTBUF, "add header with new block");
    display_check_buf(pktbuf);
    return NET_ERR_OK;
}

net_err_t pktbuf_remove_header(pktbuf_t *pktbuf, int header_size) {
    pktblk_t *first_blk = pktbuf_first_block(pktbuf);

    while (header_size) {
        if (header_size<=first_blk->used_blk_size) {
            first_blk->data_head += header_size;
            first_blk->used_blk_size -= header_size;
            pktbuf->total_size -= header_size;
            dbg_info(DBG_PKTBUF, "remove header without new block");
            break;
        }
        int current_size = first_blk->used_blk_size;
        pktblk_free(pktbuf, first_blk);
        header_size -= current_size;
        pktbuf->total_size -= current_size;
        first_blk = pktbuf_first_block(pktbuf);


    }
    display_check_buf(pktbuf);
    return NET_ERR_OK;
}


net_err_t pktbuf_resize(pktbuf_t *pktbuf, unsigned int new_size) {
    if (new_size == pktbuf->total_size) {
        return NET_ERR_OK;
    }
    if (new_size == 0) {
        pktblock_list_free(&pktbuf->blk_list);
        pktbuf->total_size = 0;
        display_check_buf(pktbuf);
        return NET_ERR_OK;
    }
    if (pktbuf->total_size == 0) {
        pktblk_t *pktblk = pktblk_list_alloc(new_size, 0);
        if (!pktblk) {
            dbg_error(DBG_PKTBUF, "pktbuf_resize: pktblk_list_alloc failed, used:%d, current capacity:%d\n",
                pktblk_list_mblock.used, pktblk_list_mblock.size);
            return NET_ERR_MEM;
        }
        pktbuf_insert_blk_list(pktbuf, pktblk, 0);

    } else if (new_size > pktbuf->total_size) {
        pktblk_t *last_blk = pktbuf_last_block(pktbuf);
        int size_to_add = new_size - pktbuf->total_size;
        int remaining_space = calculate_free_space(last_blk);

        if (remaining_space >= size_to_add) {
            last_blk->used_blk_size += size_to_add;
            pktbuf->total_size += size_to_add;
            dbg_info(DBG_PKTBUF, "resize without new block, current total size:%d\n", pktbuf->total_size);
        } else {
            pktblk_t *new_blk = pktblk_list_alloc(size_to_add - remaining_space, 0);
            if (!new_blk) {
                dbg_error(DBG_PKTBUF, "pktbuf_resize: pktblk_list_alloc failed, used:%d, current capacity:%d\n",
                    pktblk_list_mblock.used, pktblk_list_mblock.size);
                return NET_ERR_MEM;
            }
            last_blk->used_blk_size += remaining_space;
            pktbuf->total_size += remaining_space;
            pktbuf_insert_blk_list(pktbuf, new_blk, 0);
            dbg_info(DBG_PKTBUF, "resize with new block, current total size:%d\n", pktbuf->total_size);
        }
    } else {
        int size_sum = 0;

        pktblk_t *pktblk = pktbuf_first_block(pktbuf);
        while (pktblk) {
            size_sum += pktblk->used_blk_size;
            if (size_sum >= new_size) {
                break;
            }
            pktblk = container_of(nlist_node_next(&pktblk->node), pktblk_t, node);
        }
        if (pktblk == NULL) {
            dbg_error(DBG_PKTBUF, "resize failed at size_sum:%d, new_size:%d\n", size_sum, new_size);
            return NET_ERR_INVALID;
        }
        size_sum = 0;
        pktblk_t *current = container_of(nlist_node_next(&pktblk->node), pktblk_t, node);
        while (current) {
            pktblk_t *next = container_of(nlist_node_next(&current->node), pktblk_t, node);
            size_sum += current->used_blk_size;
            pktblk_free(pktbuf, current);
            current = next;
        }
        pktblk->used_blk_size -= pktbuf->total_size - size_sum - new_size;
        pktbuf->total_size = new_size;
    }
    display_check_buf(pktbuf);
    return NET_ERR_OK;
}