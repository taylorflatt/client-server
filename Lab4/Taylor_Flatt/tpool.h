/* Makes sure the header file hasn't been included yet (maybe by other files).
 * This prevents double declaration of any identifies such as types, enums, and
 * static variables.
*/
#ifndef _FLATT_POOL_
#define _FLATT_POOL_

int tpool_init(void (*task)(int));
int tpool_add_task(int newtask);

#endif
