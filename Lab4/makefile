pool:
	gcc -Wall -std=gnu99 -otest test.c tpool.c -pthread
	gcc -c -std=gnu99 tpool.c && ar -cr libtpool.a tpool.o
	gcc -Wall -std=gnu99 -otesta test.c libtpool.a -pthread
	gcc -Wall -std=gnu99 -fpic -c tpool.c && gcc -shared -olibtpool.so tpool.o
	gcc -Wall -std=gnu99 -otestso test.c libtpool.so -pthread

poolD:
	gcc -Wall -DDEBUG -std=gnu99 -otest test.c tpool.c -pthread
	gcc -c -std=gnu99 tpool.c && ar -cr libtpool.a tpool.o
	gcc -Wall -DDEBUG -std=gnu99 -otesta test.c libtpool.a -pthread
	gcc -Wall -DDEBUG -std=gnu99 -fpic -c tpool.c && gcc -shared -olibtpool.so tpool.o
	gcc -Wall -DDEBUG -std=gnu99 -otestso test.c libtpool.so -pthread