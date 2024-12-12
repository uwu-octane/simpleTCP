#include "fixedqueue.h"
#include "dbg.h"

net_err_t fixedqueue_init(fixedqueue_t *queue, void **data, int size, nlocker_type_t locker_type){
    queue->data = data;
    queue->size = size;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;

    net_err_t err = nlocker_init(&queue->locker, locker_type);
    if( err < 0){
        dbg_error(DBG_QUEUE, "queue init: nlocker_init failed\n");
        return err;
    }


    queue->receive_sem = sys_sem_create(0);
    queue->send_sem = sys_sem_create(size);
    if (queue->receive_sem == SYS_SEM_INVALID || queue->send_sem == SYS_SEM_INVALID) {
        dbg_error(DBG_QUEUE, "queue init: sys_sem_create failed\n");
        err = NET_ERR_SYS;
        goto init_failed;
    }

    return NET_ERR_OK;
init_failed:
    if(queue->receive_sem != SYS_SEM_INVALID){
        sys_sem_free(queue->receive_sem);
    } else if(queue->send_sem != SYS_SEM_INVALID){
        sys_sem_free(queue->send_sem);
    }
    nlocker_destory(&queue->locker);
    return err;
}

//向一个固定大小的队列 fixedqueue 中发送消息
//参数：
//queue：指向 fixedqueue_t 结构体的指针，表示要发送消息的队列。
//msg：指向要发送的消息的指针。
//timeout：等待时间，单位为毫秒。如果 timeout < 0 表示不想等待队列变为可用（非阻塞发送），
net_err_t fixedqueue_send(fixedqueue_t *queue, void *msg, int timeout) {
    nlocker_lock(&queue->locker);
    /*如果 timeout < 0 表示不想等待队列变为可用（非阻塞发送），
    且当前 queue->count >= queue->size 表明队列已满，这时立即报错并返回 NET_ERR_INVALID。
    非阻塞发送：当队列已满时，发送函数会立即返回一个错误（或特定状态值）告诉你“此时不能发送”，
    而不会在这里等待。这意味着你的代码可以立即知道发送失败，可继续执行其他逻辑，而不是卡住不动。*/
    if ((timeout < 0) && (queue->count >= queue->size)) {
        dbg_warning(DBG_QUEUE, "fixedqueue_send: queue is full\n");
        nlocker_unlock(&queue->locker);
        return NET_ERR_INVALID;
    }
    //in case queue is full and waiting for receive, have to release the lock
    nlocker_unlock(&queue->locker);

    /*//wait for available memory block
    //queue->send_sem 的信号量数量与队列可用槽位有关（
    //队列不满时 send_sem 会有正数，可立即通过；队列满时 send_sem 为0，需要等待别的线程接收消息使队列空出空间）。
    //timeout 用来表示等待时间，若超时仍无空闲槽位则返回 NET_ERR_TIMEOUT。*/
    if(sys_sem_wait(queue->send_sem, timeout) < 0) {
        dbg_warning(DBG_QUEUE, "fixedqueue_send: sys_sem_wait failed\n");
        return NET_ERR_TIMEOUT;
    }

    nlocker_lock(&queue->locker);
    queue->data[queue->head++] = msg;
    if (queue->head >= queue->size) {
        queue->head = 0;
    }
    queue->count++;
    nlocker_unlock(&queue->locker);

    //notify receive thread
    //通过 queue->receive_sem 信号量通知可能正在等待接收的线程，现在队列中有新数据可读了。
    sys_sem_notify(queue->receive_sem);
    return NET_ERR_OK;
}

void* fixedqueue_receive(fixedqueue_t *queue,  int timeout) {
    //等待接收信号量
    nlocker_lock(&queue->locker);
    if (!queue->count && timeout < 0) {
        nlocker_unlock(&queue->locker);
        dbg_warning(DBG_QUEUE, "fixedqueue_receive: queue is empty\n");
        return (void *)0;
    }
    nlocker_unlock(&queue->locker);


    if(sys_sem_wait(queue->receive_sem, timeout) < 0) {
        dbg_warning(DBG_QUEUE, "fixedqueue_receive: sys_sem_wait failed\n");
        return (void *)0;
    }

    nlocker_lock(&queue->locker);
    void *msg = queue->data[queue->tail++];
    if (queue->tail >= queue->size) {
        queue->tail = 0;
    }
    queue->count--;
    nlocker_unlock(&queue->locker);

    //notify send thread
    sys_sem_notify(queue->send_sem);
    return msg;
}

void fixedqueue_destory(fixedqueue_t *queue) {
    sys_sem_free(queue->receive_sem);
    sys_sem_free(queue->send_sem);
    nlocker_destory(&queue->locker);
}



int fixedqueue_count(fixedqueue_t *queue) {

    nlocker_lock(&queue->locker);
    int cnt = queue->count;
    nlocker_unlock(&queue->locker);

    return cnt;
}