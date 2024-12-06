#include <stdio.h>
#include "sys_plat.h"
#include "echo/tcp_echo_client.h"
#include "echo/tcp_echo_server.h"
#include "net.h"
#include "netinterface_pcap.h"

//network driver init
net_err_t netdriver_init(void) {
    return netif_pcap_open();
}

int main(void) {
    //protocol stack init and start
    net_init();
    net_start();

    netdriver_init();

    while (1) {
        sys_sleep(10);
    }
    return 0;
}