#ifndef PKTBUF_H
#define PKTBUF_H

#include "nlist.h"
#include "cfg.h"
#include "sys.h"
#include "nlocker.h"

typedef struct _pktblk_t{
    nlist_node_t node;
    uint8_t *head;
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
#endif // PKTBUF_H
