#ifndef PKTBUF_H
#define PKTBUF_H

#include <dbg.h>

#include "nlist.h"
#include "cfg.h"
#include "sys.h"
#include "nlocker.h"

typedef struct _pktblk_t{
    nlist_node_t node;
    uint8_t *data_head;
    uint8_t payload[PKTBUF_BLK_SIZE];
    int used_blk_size;
    //int allocated_blk_size;
} pktblk_t;

typedef struct _pktbuf_t{
    nlist_t blk_list;
    nlist_node_t node;
    nlocker_t locker;
    int total_size;
    int append_way;

    //read control
    int pos;
    uint8_t *blk_offset;
    pktblk_t *current_blk;
} pktbuf_t;

net_err_t pktbuf_init(void);
pktbuf_t *pktbuf_alloc(int size, int append_way);
pktblk_t *pktblk_list_alloc(int size, int append_way);
void pktbuf_free(pktbuf_t *pktbuf);
void pktblk_free(pktbuf_t *pktbuf, pktblk_t *pktblk);

static inline pktblk_t* pktbuf_first_block(pktbuf_t *pktbuf) {
    return container_of(nlist_head(&pktbuf->blk_list), pktblk_t, node);
}

static inline  pktblk_t* pktbuf_last_block(pktbuf_t *pktbuf) {
    return container_of(nlist_tail(&pktbuf->blk_list), pktblk_t, node);
}

static inline pktblk_t* pktbuf_next_block(pktblk_t *pktblk) {
    return container_of(nlist_node_next(&pktblk->node), pktblk_t, node);
}

static inline int get_pktbuf_total_size(const pktbuf_t *pktbuf) {
    return pktbuf->total_size;
}

net_err_t pktbuf_add_header(pktbuf_t *pktbuf, int header_size, int continous);
net_err_t pktbuf_remove_header(pktbuf_t *pktbuf, int header_size);
net_err_t pktbuf_resize(pktbuf_t *pktbuf, unsigned int new_size);
net_err_t pktbuf_join(pktbuf_t *pktbuf, pktbuf_t *pktbuf_to_join);

//make sure the pktbuf is continuous
net_err_t pktbuf_sort_segments(pktbuf_t *pktbuf);

void pktbuf_reset_acc(pktbuf_t *pktbuf);

//return the length of the written data
net_err_t pktbuf_write(pktbuf_t *pktbuf, uint8_t *data_src, int len);
net_err_t pktbuf_read(pktbuf_t *pktbuf, uint8_t *data_dst, int len);
net_err_t pktbuf_seek(pktbuf_t *pktbuf, int offset);
net_err_t pktbuf_copy(pktbuf_t *dst, pktbuf_t *src, int len);
#endif // PKTBUF_H
