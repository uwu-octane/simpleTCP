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
} pktblk_t;

typedef struct _pktbuf_t{
    nlist_t blk_list;
    nlist_node_t node;
    nlocker_t locker;
    int total_size;
    //int blk_offset;
    //int pos;
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

net_err_t pktbuf_add_header(pktbuf_t *pktbuf, int header_size, int continous);
net_err_t pktbuf_remove_header(pktbuf_t *pktbuf, int header_size);
net_err_t pktbuf_resize(pktbuf_t *pktbuf, unsigned int new_size);
#endif // PKTBUF_H
