#include "nlocker.h"


net_err_t nlocker_init(nlocker_t *locker, nlocker_type_t type){
    locker->type = type;
    sys_mutex_t mutex = 0;
    switch (type) {
        case NLOCKER_THREAD:
            mutex =  sys_mutex_create();
            if (mutex == SYS_MUTEX_INVALID) {
                return NET_ERR_SYS;
            }
            break;
        default:
            break;
    }
    locker->mutex = mutex;
    return NET_ERR_OK;
}

void nlocker_destory(nlocker_t *locker){
    switch (locker->type) {
        case NLOCKER_THREAD:
            sys_mutex_free(locker->mutex);
            break;
        default:
            break;
    }
}

void nlocker_lock(nlocker_t *locker){
    switch (locker->type) {
        case NLOCKER_THREAD:
            sys_mutex_lock(locker->mutex);
            break;
        default:
            break;
    }
}

void nlocker_unlock(nlocker_t *locker){
    switch (locker->type) {
        case NLOCKER_THREAD:
            sys_mutex_unlock(locker->mutex);
            break;
        default:
            break;
    }
}