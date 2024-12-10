#include "exmsg.h"

#include <dbg.h>

#include "sys_plat.h"
#include "fixedqueue.h"
#include "mblock.h"

static void *msg_tbl[2048];
static fixedqueue_t message_queue;
static exmsg_t msg_buffer[50];
static mblock_t msg_mblock;

net_err_t exmsg_init(void){
  dbg_info(DBG_MSG,"exmsg init\n");
  net_err_t err = fixedqueue_init(&message_queue, msg_tbl, EXMSG_MSG_CNT, EXMSG_LOCKER);
  if(err < 0){
    dbg_error(DBG_MSG, "exmsg init: fixedqueue_init failed\n");
    return err;
  }

  err = mblock_init(&msg_mblock, msg_buffer, sizeof(exmsg_t), EXMSG_MSG_CNT, EXMSG_LOCKER);
  if (err < 0) {
    dbg_error(DBG_MSG, "exmsg init: mblock_init failed\n");
    return err;
  }

  dbg_info(DBG_MSG, "exmsg init success\n");
  return NET_ERR_OK;
}


net_err_t exmsg_netif_in(void) {
    exmsg_t *msg = (exmsg_t *)mblock_alloc(&msg_mblock, -1);
    if (!msg) {
      dbg_warning(DBG_MSG, "exmsg_netif_in: mblock_alloc failed, no free block\n");
      return NET_ERR_MEM;
    }
    static int id = 0;
    msg->type = NET_EXMSG_TYPE_NETIF_IN;
    msg->id = id++;
    net_err_t err = fixedqueue_send(&message_queue, msg, -1);
    plat_printf("send msg: %d\n", msg->id);
    if (err < 0) {
      dbg_warning(DBG_MSG, "exmsg_netif_in: fixedqueue_send failed\n");
      mblock_free(&msg_mblock, msg);
      return err;
    }
    return NET_ERR_OK;
}

static void work_thread(void * arg){
  plat_printf("work thread start\n");
  while(1){
    exmsg_t *msg = (exmsg_t *)fixedqueue_receive(&message_queue, 0);
    plat_printf("work thread receive msg: %d\n", msg->id);
    //after process, free the message block
    mblock_free(&msg_mblock, msg);
    sys_sleep(1);
  }
}

net_err_t exmsg_start(void){
    sys_thread_t thread = sys_thread_create(work_thread, (void *)0);
    if(thread == SYS_THREAD_INVALID){
      return NET_ERR_SYS;
    }
    return NET_ERR_OK;
}