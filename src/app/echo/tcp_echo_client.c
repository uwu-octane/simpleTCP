#include "tcp_echo_client.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include "sys_plat.h"

int tcp_echo_client_start(const char* ip, int port){
      plat_printf("tcp echo client, ip: %s, port: %d\n", ip, port);

      // 创建socket AF_INET:IPV4, SOCK_STREAM:TCP
      int sockfd = socket(AF_INET, SOCK_STREAM,0);
      if (sockfd < 0) {
          plat_printf("create socket failed\n");
          goto end;
      }

      //sockaddr_in结构体是专门用来处理网络通信的地址结构
      struct sockaddr_in server_addr;
      plat_memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(port);
      server_addr.sin_addr.s_addr = inet_addr(ip);
      //连接服务器
      if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
            plat_printf("connect failed\n");
            goto end;
      }

      //发送数据
      char buf[128];
      plat_printf(">>");
      while (fgets(buf, sizeof(buf),stdin) != NULL){
          if(send(sockfd,buf,strlen(buf),0) <= 0 )  {
                plat_printf("send failed\n");
                goto end;
            }

            plat_memset(buf,0,sizeof(buf));
            int len = recv(sockfd, buf, sizeof(buf) - 1, 0);
            if (len < 0) {
                plat_printf("recv failed\n");
                goto end;
            }

            plat_printf("%s",buf);
            plat_printf(">>");
      }
      close(sockfd);

end:

  if (sockfd >= 0) {
      close(sockfd);
  }

  return -1;

}