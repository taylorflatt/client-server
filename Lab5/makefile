client:
	gcc -Wall -o client client.c

server:
	gcc -Wall -pthread -lrt -o server server.c

clientD:
	gcc -Wall -DDEBUG -o client client.c

serverD:
	gcc -Wall -pthread -DDEBUG -std=gnu99 -lrt  -o server server.c tpool.c
