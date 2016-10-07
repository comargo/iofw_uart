#ifndef CIRC_H
#define CIRC_H

#include <stddef.h>

struct circ_buf {
    unsigned char *buf;
    int head;
    int tail;
    int size;
};

/* Return count in buffer.  */
#define _CIRC_CNT(head,tail,size) (((head) - (tail)) & ((size)-1))
#define CIRC_CNT(buf) _CIRC_CNT((buf).head,(buf).tail,(buf).size)

/* Return space available, 0..size-1.  We always leave one free char
   as a completely full buffer has head == tail, which is the same as
   empty.  */
#define _CIRC_SPACE(head,tail,size) _CIRC_CNT((tail),((head)+1),(size))
#define CIRC_SPACE(buf) _CIRC_SPACE((buf).head,(buf).tail,(buf).size)

/* Return count up to the end of the buffer.  Carefully avoid
   accessing head and tail more than once, so they can change
   underneath us without returning inconsistent results.  */
#define _CIRC_CNT_TO_END(head,tail,size) \
        ({int end = (size) - (tail); \
          int n = ((head) + end) & ((size)-1); \
          n < end ? n : end;})
#define CIRC_CNT_TO_END(buf) _CIRC_CNT_TO_END((buf).head,(buf).tail,(buf).size)

/* Return space available up to the end of the buffer.  */
#define _CIRC_SPACE_TO_END(head,tail,size) \
        ({int end = (size) - 1 - (head); \
          int n = (end + (tail)) & ((size)-1); \
          n <= end ? n : end+1;})
#define CIRC_SPACE_TO_END(buf) _CIRC_SPACE_TO_END((buf).head,(buf).tail,(buf).size)

#define CIRC_INCREMENT(buf, field, inc) (buf).field = ((buf).field+inc)&((buf).size-1)

static inline unsigned char *circ_alloc_buffer(struct circ_buf* buf, int size)
{
    buf->buf = calloc(size, sizeof(*buf->buf));
    if(!buf->buf)
        return NULL;
    buf->size = size;
    buf->head = 0;
    buf->tail = 0;
    return buf->buf;
}

#endif//CIRC_H
