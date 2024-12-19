#ifndef NET_ERR_H
#define NET_ERR_H

typedef enum _net_err_t{
    NET_ERR_OK = 0,
    NET_ERR_SYS = -1,
    NET_ERR_MEM = -2,
    NET_ERR_INVALID = -3,
    NET_ERR_TIMEOUT = -4,
    NET_ERR_PARAM = -5,
    NET_ERR_SIZE = -6,
}net_err_t;
#endif // NET_ERR_H