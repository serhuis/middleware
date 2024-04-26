#include "ringbuf.h"

/** \def CC_ACCESS_NOW(x)
 * This macro ensures that the access to a non-volatile variable can
 * not be reordered or optimized by the compiler.
 * See also https://lwn.net/Articles/508991/ - In Linux the macro is
 * called ACCESS_ONCE
 * The type must be passed, because the typeof-operator is a gcc
 * extension
 */

#define CC_ACCESS_NOW(type, variable) (*(volatile type *)&(variable))

/*---------------------------------------------------------------------------*/

void RingBuf_Init(struct RingBuf *r, uint8_t *dataptr, uint8_t size)
{
  r->data = dataptr;
  r->mask = size - 1;
  r->put_ptr = 0;
  r->get_ptr = 0;
}

/*---------------------------------------------------------------------------*/

int RingBuf_Put(struct RingBuf *r, uint8_t c)
{
  /* Check if buffer is full. If it is full, return 0 to indicate that
     the element was not inserted into the buffer.

     XXX: there is a potential risk for a race condition here, because
     the ->get_ptr field may be written concurrently by the
     RingBuf_get() function. To avoid this, access to ->get_ptr must
     be atomic. We use an uint8_t type, which makes access atomic on
     most platforms, but C does not guarantee this.
  */
  if(((r->put_ptr - r->get_ptr) & r->mask) == r->mask) 
	{
    return 0;
  }
  /*
   * CC_ACCESS_NOW is used because the compiler is allowed to reorder
   * the access to non-volatile variables.
   * In this case a reader might read from the moved index/ptr before
   * its value (c) is written. Reordering makes little sense, but
   * better safe than sorry.
   */
  CC_ACCESS_NOW(uint8_t, r->data[r->put_ptr]) = c;
  CC_ACCESS_NOW(uint8_t, r->put_ptr) = (r->put_ptr + 1) & r->mask;
  return 1;
}

/*---------------------------------------------------------------------------*/

int RingBuf_Get(struct RingBuf *r)
{
  uint8_t c;
  
  /* Check if there are bytes in the buffer. If so, we return the
     first one and increase the pointer. If there are no bytes left, we
     return -1.

     XXX: there is a potential risk for a race condition here, because
     the ->put_ptr field may be written concurrently by the
     RingBuf_put() function. To avoid this, access to ->get_ptr must
     be atomic. We use an uint8_t type, which makes access atomic on
     most platforms, but C does not guarantee this.
  */
  if(((r->put_ptr - r->get_ptr) & r->mask) > 0) 
	{
    /*
     * CC_ACCESS_NOW is used because the compiler is allowed to reorder
     * the access to non-volatile variables.
     * In this case the memory might be freed and overwritten by
     * increasing get_ptr before the value was copied to c.
     * Opposed to the put-operation this would even make sense,
     * because the register used for mask can be reused to save c
     * (on some architectures).
     */
    c = CC_ACCESS_NOW(uint8_t, r->data[r->get_ptr]);
    CC_ACCESS_NOW(uint8_t, r->get_ptr) = (r->get_ptr + 1) & r->mask;
    return c;
  } else 
	{
    return -1;
  }
}

/*---------------------------------------------------------------------------*/

int RingBuf_Size(struct RingBuf *r)
{
  return r->mask + 1;
}

/*---------------------------------------------------------------------------*/

int RingBuf_elements(struct RingBuf *r)
{
  return (r->put_ptr - r->get_ptr) & r->mask;
}

/*---------------------------------------------------------------------------*/
