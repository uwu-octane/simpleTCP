#include "netinterface_pcap.h"
#include "sys_plat.h"
#include "exmsg.h"


void recv_thread(void *arg) {
    plat_printf("recv thread start\n");
  //netif_pcap_open();
    while(1){
        exmsg_netif_in();
        sys_sleep(1000);
    }
}

void transmit_thread(void *arg) {
    plat_printf("send thread start\n");
    //netif_pcap_open();
    while(1){
        sys_sleep(1000);
    }
}

net_err_t netif_pcap_open(void){
    sys_thread_t thread = sys_thread_create(recv_thread, (void *)0);
    sys_thread_t thread2 = sys_thread_create(transmit_thread, (void *)0);
    if(thread == SYS_THREAD_INVALID || thread2 == SYS_THREAD_INVALID){
        return NET_ERR_SYS;
    }
    return NET_ERR_OK;
}
