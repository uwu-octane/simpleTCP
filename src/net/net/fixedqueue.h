#ifndef FIXEDQUEUE_H
#define FIXEDQUEUE_H

#include "nlocker.h"
#include "sys.h"
typedef struct _fixedqueue_t{
    void **data;
    int size;
    int count;
    int head;
    int tail;
    nlocker_t locker;
    sys_sem_t receive_sem;
    sys_sem_t send_sem;
} fixedqueue_t;

net_err_t fixedqueue_init(fixedqueue_t *queue, void **data, int size, nlocker_type_t locker_type);

//向一个固定大小的队列 fixedqueue 中发送消息
//参数：
//queue：指向 fixedqueue_t 结构体的指针，表示要发送消息的队列。
//msg：指向要发送的消息的指针。
//timeout：等待时间，单位为毫秒。如果 timeout < 0 表示不想等待队列变为可用（非阻塞发送），
net_err_t fixedqueue_send(fixedqueue_t *queue, void *msg, int timeout);

//从一个固定大小的队列 fixedqueue 中接收消息
//参数：
//queue：指向 fixedqueue_t 结构体的指针，表示要接收消息的队列。
//timeout：等待时间，单位为毫秒。如果 timeout < 0 表示不想等待队列变为可用（非阻塞接收），
void* fixedqueue_receive(fixedqueue_t *queue,  int timeout);

#endif // FIXEDQUEUE_H