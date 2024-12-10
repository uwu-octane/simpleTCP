#include <stdio.h>
#include "sys_plat.h"
#include "echo/tcp_echo_client.h"
#include "echo/tcp_echo_server.h"
#include "net.h"
#include "netinterface_pcap.h"
#include "dbg.h"
#include "nlist.h"
#include "mblock.h"
//network driver init

net_err_t netdriver_init(void) {
    return netif_pcap_open();
}

void mblock_test(void) {
    mblock_t block_list;
    int blk_size = 10 * sizeof(nlist_node_t);
    uint8_t buffer[5][blk_size];
    mblock_init(&block_list, buffer, blk_size, 5, NLOCKER_THREAD);
    printf("block_list size:%d\n", mblock_freeblock_cnt(&block_list));

    /*nlist_t list = block_list.free_list;
    nlist_iterator_t iter = nlist_iterator(&list);
    while (nlist_iterator_next(&iter) && iter.node != (nlist_node_t *)0) {
        printf("list element: %p\n", iter.node);
    }*/
    void* temp[5];
    for (int i = 0; i< 5; i++) {
        temp[i] = mblock_alloc(&block_list, 0);
        printf("block: %p, block_list size:%d\n",temp[i] ,mblock_freeblock_cnt(&block_list));
    }
    for (int j = 0; j< 5; j++) {
        mblock_free(&block_list, temp[j]);
        printf("block available: %p ,block_list size:%d\n", temp[j],mblock_freeblock_cnt(&block_list));
    }
}

void nlist_test() {

    nlist_t list;
    nlist_init(&list);

    static uint8_t buffer[2][3 * sizeof(nlist_node_t)];
    uint8_t *buf = (uint8_t *)buffer;
    //printf("buf: %p\n", buf);
    //printf("buf.next: %p\n", buf+=sizeof(buffer[0]));
    /*int blk_size = sizeof(nlist_node_t);
    int blk_count = sizeof(buffer) / blk_size;
    printf("blk_size: %d, blk_count: %d\n", blk_size, blk_count);*/
    int blk_size = sizeof(buffer[0]);
    int blk_count = sizeof(buffer) / blk_size;
    printf("blk_size: %d, blk_count: %d\n", blk_size, blk_count);
    for (int i = 0; i < blk_count; i++) {
        printf("i: %d\n", i);
        nlist_node_t *block_node = (nlist_node_t *)(buf + i * blk_size);
        nlist_node_init(block_node);
        nlist_append(&list, block_node);
        printf("list element: %p\n", block_node);
        printf("list head: %p\n", nlist_head(&list));
        printf("list tail: %p\n", nlist_tail(&list));
        printf("head.next: %p\n", nlist_node_next(nlist_head(&list)));
    }
}

int main(void) {
    //protocol stack init and start
    //dbg_info(3,"dbg_infor");
    //dbg_assert(1==2, "test");
    //mblock_test();
    net_init();
    net_start();

    netdriver_init();

    while (1) {
        sys_sleep(10);
    }
    return 0;
}
