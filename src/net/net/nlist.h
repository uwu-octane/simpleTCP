#ifndef NLIST_H
#define NLIST_H

typedef struct _nlist_node_t{
  struct _nlist_node_t *pre;
  struct _nlist_node_t *next;
  }nlist_node_t;

static inline void nlist_node_init(nlist_node_t *node){
  node->next = node->pre = (nlist_node_t *)0;
}

static inline nlist_node_t * nlist_node_next(nlist_node_t *node){
  return node->next;
}

static inline nlist_node_t * nlist_node_pre(nlist_node_t *node){
  return node->pre;
}

static inline void nlist_node_link(nlist_node_t *node, nlist_node_t *next){
  node->next = next;
  next->pre = node;
}




typedef struct _nlist_t{
  nlist_node_t *head;
  nlist_node_t *tail;
  int count;
}nlist_t;

void nlist_init(nlist_t *list);

static inline int nlist_count(nlist_t *list){
  return list->count;
}

static inline int nlist_is_empty(nlist_t *list){
  return list->count == 0;
}

static inline nlist_node_t * nlist_head(nlist_t *list){
  return list->head;
}

static inline nlist_node_t * nlist_tail(nlist_t *list){
  return list->tail;
}

void nlist_append(nlist_t *list, nlist_node_t *node);

void nlist_append_head(nlist_t *list, nlist_node_t *node);

void nlist_remove(nlist_t *list, nlist_node_t *node);

void nlist_insert(nlist_t *list, nlist_node_t *node, nlist_node_t *new_node);


typedef struct iterator_t {
  nlist_t *list;
  nlist_node_t *node;
}nlist_iterator_t;

nlist_iterator_t nlist_iterator(nlist_t *list);

static inline nlist_node_t * nlist_iterator_next(nlist_iterator_t *it){
  nlist_node_t *node = it->node;
  //set next node
  if (node != (nlist_node_t *)0) {
    it->node = node->next;
  }
  //it->node = nlist_node_next(it->node);
  return node;
}
int nlist_iterator_has_next(nlist_iterator_t *it);

#endif // NLIST_H