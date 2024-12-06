#include "tcp_echo_server.h"
#include <arpa/inet.h>
#include "sys_plat.h"
#include <sys/socket.h>


void tcp_echo_server_start(int port) {
    plat_printf("tcp echo server, port: %d\n", port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        plat_printf("create socket failed\n");
        goto end;
    }

    struct sockaddr_in server_addr;
    plat_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
          plat_printf("bind failed\n");
          goto end;
    }
    listen(sockfd, 5);
    while(1){
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client = accept(sockfd, (struct sockaddr *)&client_addr, &len);
        if (client < 0) {
            plat_printf("accept failed\n");
            goto end;
        }
        plat_printf("accept client: %s\n, port: %d\n", inet_ntoa(client_addr.sin_addr),
                    ntohs(client_addr.sin_port));
        char buf[128];
        ssize_t size;
        while((size = recv(client, buf, sizeof(buf), 0)) > 0) {
            plat_printf("recvied size : %d\n", (int)size);
            send(client, buf, size, 0);
        }
        close(client);
        }

end:
    if (sockfd >= 0) {
        close(sockfd);
    }
    }
