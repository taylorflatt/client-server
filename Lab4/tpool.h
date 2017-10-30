#ifndef _FLATT_POOL_
#define _FLATT_POOL_

int tpool_init(void (*task)(int));
int tpool_add_task(int newtask);

#endif
