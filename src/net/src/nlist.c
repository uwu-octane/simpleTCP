#include "nlist.h"

#include <dbg.h>
#include <stdio.h>

void nlist_init(nlist_t *list){
    list->head = list->tail = (nlist_node_t *)0;
    list->count = 0;
}

void nlist_append(nlist_t *list, nlist_node_t *node){
    //node->next = (nlist_node_t *)0;
    //node->pre = list->tail;
    //printf("list count before append: %d\n", list->count);
    if (nlist_is_empty(list)) {
        list->head = list->tail = node;
        node->pre = node->next = (nlist_node_t *)0;
    } else {
        list->tail->next = node;
        node->pre = list->tail;
        node->next = (nlist_node_t *)0;
        list->tail = node;
    }
    list->count++;
   // printf("list count after append: %d\n", list->count);
}

void nlist_append_head(nlist_t *list, nlist_node_t *node){
    if (nlist_is_empty(list)) {
        // 空链表时，新节点即为头又为尾
        list->head = list->tail = node;
        node->pre = node->next = (nlist_node_t *)0;
    } else {
        // 非空链表在头部插入
        node->pre = (nlist_node_t *)0;
        node->next = list->head;
        list->head->pre = node;
        list->head = node;
    }
    list->count++;
}

void nlist_insert(nlist_t *list, nlist_node_t *node, nlist_node_t *new_node){
    new_node->next = node->next;
    new_node->pre = node;
    node->next = new_node;
    if (new_node->next) {
        new_node->next->pre = new_node;
    } else {
        // 如果new_node是插在尾节点后面，则new_node成为新的尾节点
        list->tail = new_node;
    }
    list->count++;
}

void nlist_remove(nlist_t *list, nlist_node_t *node){
    if (!list || !node) {
        dbg_error(DBG_LIST, "nlist_remove: list or node is null");
        return;
    }
    if (node->pre == (nlist_node_t *)0) {
        // 如果是链表的头节点
        list->head = node->next;
    } else {
        // 更新前驱节点的 next 指针
        node->pre->next = node->next;
    }

    if (node->next == (nlist_node_t *)0) {
        list->tail = node->pre;
    } else {
        node->next->pre = node->pre;
    }
    list->count--;
    node->pre = node->next = (nlist_node_t *)0;
}

nlist_iterator_t nlist_iterator(nlist_t *list){
    nlist_iterator_t it;
    it.list = list;
    it.node = list->head;
    return it;
}
int nlist_iterator_has_next(nlist_iterator_t *it) {
    return it->node != (nlist_node_t *)0;
}

//从头部移除一个节点并返回
nlist_node_t *nlist_remove_head(nlist_t *list) {
    if (nlist_is_empty(list)) {
        return NULL;
    }

    nlist_node_t *node = list->head;
    nlist_node_t *next = node->next;

    if (next) {
        next->pre = NULL;
    } else {
        // 没有下一个节点了，说明移除后链表为空
        list->tail = NULL;
    }
    list->head = next;
    list->count--;

    node->pre = node->next = NULL;
    return node;
}

void nlist_remove_list(nlist_t *list) {
    while (!nlist_is_empty(list)) {
        nlist_remove_head(list);
    }
}