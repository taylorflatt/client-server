client:
	gcc -Wall -o client client.c

server:
	gcc -Wall -pthread -std=gnu99 -o server server.c tpool.c -lrt

clientD:
	gcc -Wall -DDEBUG -o client client.c

serverD:
	gcc -Wall -pthread -DDEBUG -std=gnu99  -o server server.c tpool.c -lrt
