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
static int pktblk_remain_tail_space(const pktblk_t* pktblk) {
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

        int free_space = pktblk_remain_tail_space(pktblk);
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
        memset(pktblk->payload, 0, PKTBUF_BLK_SIZE);
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

            if (!pktblk_head && !pktblk_tail) {
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
            if (!pktblk_head && !pktblk_tail) {
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
    pktbuf->append_way = append_way;
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
    pktbuf_reset_acc(pktbuf);
    display_check_buf(pktbuf);
    return pktbuf;
}


void pktbuf_free(pktbuf_t *pktbuf) {
    pktblock_list_free(&pktbuf->blk_list);
    mblock_free(&pktbuf_list_mblock, pktbuf);
    plat_printf("current pktbuf_list_mblock used:%d\n", pktbuf_list_mblock.used);
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
        int remaining_space = pktblk_remain_tail_space(last_blk);

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


// 辅助函数：为pktbuf在尾部追加指定长度的数据块链表
// append_way=0表示从块头开始写入
static pktblk_t *pktblk_alloc_chain(int size_needed) {
    // 分配一个或多个块来容纳size_needed数据
    // 具体可与pktblk_list_alloc类似
    return pktblk_list_alloc(size_needed, 0);
}

// 辅助函数：将src的所有数据拷贝到dst的尾部
static net_err_t pktbuf_copy_data_at_tail(pktbuf_t *dst, pktbuf_t *src) {
    nlist_t *src_list = &src->blk_list;
    nlist_iterator_t it = nlist_iterator(src_list);

    // 遍历src所有块
    while (nlist_iterator_has_next(&it)) {
        pktblk_t *src_blk = (pktblk_t *)nlist_iterator_next(&it);
        uint8_t *src_data = src_blk->data_head;
        int src_len = src_blk->used_blk_size;

        // 将src_blk数据全部拷入dst尾部
        while (src_len > 0) {
            pktblk_t *last_blk = pktbuf_last_block(dst);
            if (!last_blk) {
                // dst还没有块，需要分配新的块
                last_blk = pktblk_alloc_chain(src_len);
                if (!last_blk) {
                    dbg_error(DBG_PKTBUF, "pktbuf_copy_data_at_tail: alloc chain failed\n");
                    return NET_ERR_MEM;
                }
                pktbuf_insert_blk_list(dst, last_blk, 0); // 尾插入
            }

            int remain = pktblk_remain_tail_space(last_blk);
            int to_copy = 0;
            if (remain > 0) {
                to_copy = remain < src_len ? remain : src_len;
                uint8_t *dst_ptr = last_blk->data_head + last_blk->used_blk_size;
                memcpy(dst_ptr, src_data, to_copy);
                last_blk->used_blk_size += to_copy;
                dst->total_size += to_copy;
            } else {
                to_copy = src_len < PKTBUF_BLK_SIZE ? src_len : PKTBUF_BLK_SIZE;
                pktblk_t *new_blk = pktblk_alloc_chain(to_copy);
                if (!new_blk) {
                    dbg_error(DBG_PKTBUF, "pktbuf_copy_data_at_tail: alloc chain failed\n");
                    return NET_ERR_MEM;
                }
                pktbuf_insert_blk_list(dst, new_blk, 0);
                last_blk = new_blk;
                memcpy(last_blk->data_head, src_data, to_copy);
            }
            src_len -= to_copy;
            src_data += to_copy;  // 更新src数据位置
            // 更新dst块信息
            //last_blk->used_blk_size += to_copy;
        }
    }
    //display_check_buf(dst);
    return NET_ERR_OK;
}


net_err_t pktbuf_join(pktbuf_t *pktbuf, pktbuf_t *pktbuf_to_join) {
    if (!pktbuf_to_join) {
        return NET_ERR_INVALID;
    }

    int size_to_add = pktbuf_to_join->total_size;
    if (size_to_add == 0) {
        // 待合并的pktbuf空，直接释放
        pktbuf_free(pktbuf_to_join);
        return NET_ERR_OK;
    }

    // 实际拷贝数据到pktbuf的尾部
    net_err_t err = pktbuf_copy_data_at_tail(pktbuf, pktbuf_to_join);
    if (err < 0) {
        dbg_error(DBG_PKTBUF, "pktbuf_join: copy data failed\n");
        return err;
    }

    // 所有数据已拷贝到pktbuf中，可以安全释放pktbuf_to_join
    pktbuf_free(pktbuf_to_join);

    display_check_buf(pktbuf);
    return NET_ERR_OK;
}

//not cheacked yet !!
net_err_t pktbuf_sort_segments(pktbuf_t *pktbuf) {
    // 如果没有数据或只有一个块，不需要合并
    if (pktbuf->total_size == 0) {
        return NET_ERR_OK;
    }

    // 从第一个块开始
    pktblk_t *current = pktbuf_first_block(pktbuf);
    while (current) {
        // 根据append_way对齐当前块的数据
        uint8_t *desired_pos = NULL;
        if (pktbuf->append_way) {
            // 头插法：数据应当位于块尾部
            desired_pos = current->payload + (PKTBUF_BLK_SIZE - current->used_blk_size);
        } else {
            // 尾插法：数据应位于块头部
            desired_pos = current->payload;
        }

        // 若当前data_head与desired_pos不一致，则移动数据
        if (current->data_head != desired_pos && current->used_blk_size > 0) {
            // 将数据移动到目标位置
            memmove(desired_pos, current->data_head, current->used_blk_size);
            current->data_head = desired_pos;
        }

        // 不断从后面的块中搬数据进来直到这个块填满或后面没有数据
        while (1) {
            pktblk_t *next = NULL;
            nlist_node_t *next_node = nlist_node_next(&current->node);
            if (next_node) {
                next = container_of(next_node, pktblk_t, node);
            } else {
                break; // 没有后续块了
            }

            int remain = pktblk_remain_tail_space(current);
            if (remain <= 0) {
                // 当前块已满，转到下一个块进行整理
                break;
            }

            if (next->used_blk_size == 0) {
                // 下一块无数据，释放掉
                pktblk_free(pktbuf, next);
                continue; // 再次检查后续块
            }

            // 将能从next块挪动的数据量
            int to_copy = remain < next->used_blk_size ? remain : next->used_blk_size;
            uint8_t *dst_ptr = current->data_head + current->used_blk_size;
            // 从next块复制数据到current块尾部
            memmove(dst_ptr, next->data_head, to_copy);

            current->used_blk_size += to_copy;
            next->data_head += to_copy;
            next->used_blk_size -= to_copy;
            pktbuf->total_size = pktbuf->total_size; // total_size不变，因为未增减数据

            if (next->used_blk_size == 0) {
                // next块数据已搬空，释放next块
                pktblk_free(pktbuf, next);
                // 继续尝试从下一个块中搬数据进来
            } else {
                // next块仍有数据，但当前块已无更多空间或已尽力拷贝
                // 再看一下是否还能继续拷贝，如果remain还有空间会继续下一轮循环
                // 如果remain不够或next数据仍有剩余都没问题，下一轮循环会再次尝试
                if (pktblk_remain_tail_space(current) == 0) {
                    // 当前块满了，退出内部合并循环
                    break;
                }
            }
        }

        // current块已尽力从后续块搬数据，现在转到下一个块整理
        nlist_node_t *next_node = nlist_node_next(&current->node);
        if (!next_node) {
            // 没有下一个块了，排序结束
            break;
        }
        current = container_of(next_node, pktblk_t, node);
    }
    display_check_buf(pktbuf);
    return NET_ERR_OK;
}

//while reading data from pktbuf, the pos should be reset to 0
void pktbuf_reset_acc(pktbuf_t *pktbuf) {
    if (pktbuf) {
        pktbuf->pos = 0;
        pktbuf->current_blk = pktbuf_first_block(pktbuf);
        pktbuf->blk_offset = pktbuf->current_blk ? pktbuf->current_blk->data_head : (uint8_t *)0;
    }
}

static inline int total_blk_remain_space(pktbuf_t *pktbuf) {
    return pktbuf->total_size - pktbuf->pos;
}

static inline int plk_remain_space(pktbuf_t *pktbuf) {
    pktblk_t *pktblk = pktbuf->current_blk;
    if (!pktblk) {
        return 0;
    }
    return (int)(pktblk->data_head + pktblk->used_blk_size - pktbuf->blk_offset);
}

// 辅助函数：向前移动指定长度的读写指针
static void write_move_forward(pktbuf_t *pktbuf, int len) {
    pktbuf->pos += len;
    pktbuf->blk_offset += len;

    pktblk_t *pktblk = pktbuf->current_blk;
    // 如果当前块已写满，转到下一个块
    if (pktbuf->blk_offset >= pktblk->data_head + pktblk->used_blk_size) {
        pktbuf->current_blk = pktbuf_next_block(pktbuf->current_blk);
        pktbuf->blk_offset = pktbuf->current_blk ? pktbuf->current_blk->data_head : (uint8_t *)0;
    }
}

net_err_t pktbuf_write(pktbuf_t *pktbuf, uint8_t *data_src, int len) {
    if (!pktbuf || !data_src || len <= 0) {
        dbg_error(DBG_PKTBUF, "pktbuf_write: invalid param\n");
        return NET_ERR_PARAM;
    }

    int remain_size = total_blk_remain_space(pktbuf);
    if (remain_size < len) {
        dbg_error(DBG_PKTBUF, "pktbuf_write: no enough space\n");
        return NET_ERR_SIZE;
    }

    while (len) {
        int remain_blk_space = plk_remain_space(pktbuf);
        int to_copy = remain_blk_space < len ? remain_blk_space : len;
        plat_memcpy(pktbuf->blk_offset, data_src, to_copy);
        data_src += to_copy;
        len -= to_copy;
        dbg_info(DBG_PKTBUF, "pktbuf_write: write %d bytes, remain %d to write\n", to_copy, len);
        write_move_forward(pktbuf, to_copy);
    }

    return NET_ERR_OK;
}