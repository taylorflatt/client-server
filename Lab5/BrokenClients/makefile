allclients:
	gcc -Wall -o client-child-sigint client-child-sigint.c
	gcc -Wall -o client-child-sigquit client-child-sigquit.c
	gcc -Wall -o client-failed-challenge client-failed-challenge.c
	gcc -Wall -o client-failed-port client-failed-port.c
	gcc -Wall -o client-failed-proceed client-failed-proceed.c
	gcc -Wall -o client-failed-secret client-failed-secret.c
	gcc -Wall -o client-parent-sigint client-parent-sigint.c
	gcc -Wall -o client-parent-sigquit client-parent-sigquit.c
	gcc -Wall -o client-wait-on-connect client-wait-on-connect.c
	gcc -Wall -o client-wait-on-handshake client-wait-on-handshake.c

allclientsD:
	gcc -Wall -DDEBUG -o client-child-sigint client-child-sigint.c
	gcc -Wall -DDEBUG -o client-child-sigquit client-child-sigquit.c
	gcc -Wall -DDEBUG -o client-failed-challenge client-failed-challenge.c
	gcc -Wall -DDEBUG -o client-failed-port client-failed-port.c
	gcc -Wall -DDEBUG -o client-failed-proceed client-failed-proceed.c
	gcc -Wall -DDEBUG -o client-failed-secret client-failed-secret.c
	gcc -Wall -DDEBUG -o client-parent-sigint client-parent-sigint.c
	gcc -Wall -DDEBUG -o client-parent-sigquit client-parent-sigquit.c
	gcc -Wall -DDEBUG -o client-wait-on-connect client-wait-on-connect.c
	gcc -Wall -DDEBUG -o client-wait-on-handshake client-wait-on-handshake.c

server:
	gcc -Wall -pthread -lrt -o server ../server.c

serverD:
	gcc -Wall -pthread -lrt -DDEBUG -o server ../server.c