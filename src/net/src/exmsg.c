#include "exmsg.h"
#include "sys_plat.h"

net_err_t exmsg_init(void){
  return NET_ERR_OK;
}

static void work_thread(void * arg){
  plat_printf("work thread start\n");
  while(1){
    sys_sleep(1000);
  }
}

net_err_t exmsg_start(void){
    sys_thread_t thread = sys_thread_create(work_thread, (void *)0);
    if(thread == SYS_THREAD_INVALID){
      return NET_ERR_SYS;
    }
    return NET_ERR_OK;
}