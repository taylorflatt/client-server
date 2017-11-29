lab1client:
	gcc -Wall -o client1 ../Lab1/client.c

lab1server:
	gcc -Wall -o server1 ../Lab1/server.c

lab2client:
	gcc -Wall -DDEBUG -o client2 ../Lab2/client.c

lab2server:
	gcc -Wall -DDEBUG -o server2 ../Lab2/server.c

lab3client:
	gcc -Wall -DDEBUG -o client3 ../Lab3/client.c

lab3server:
	gcc -Wall -pthread -DDEBUG  -o server3 ../Lab3/server.c -lrt

lab4tpool:
	gcc -c -std=gnu99 ../Lab4/tpool.c -o ../Lab4/tpool.o

lab5client:
	gcc -Wall -DDEBUG -o client5 ../Lab5/client.c

lab5server:
	gcc -Wall -pthread -DDEBUG -std=gnu99  -o server5 ../Lab5/server.c ../Lab5/tpool.c -lrt

brokenclients:
	gcc -Wall -DDEBUG -o client-child-sigint BrokenClients/client-child-sigint.c
	gcc -Wall -DDEBUG -o client-child-sigquit BrokenClients/client-child-sigquit.c
	gcc -Wall -DDEBUG -o client-failed-challenge BrokenClients/client-failed-challenge.c
	gcc -Wall -DDEBUG -o client-failed-port BrokenClients/client-failed-port.c
	gcc -Wall -DDEBUG -o client-failed-proceed BrokenClients/client-failed-proceed.c
	gcc -Wall -DDEBUG -o client-failed-secret BrokenClients/client-failed-secret.c
	gcc -Wall -DDEBUG -o client-parent-sigint BrokenClients/client-parent-sigint.c
	gcc -Wall -DDEBUG -o client-parent-sigquit BrokenClients/client-parent-sigquit.c
	gcc -Wall -DDEBUG -o client-wait-on-connect BrokenClients/client-wait-on-connect.c
	gcc -Wall -DDEBUG -o client-wait-on-handshake BrokenClients/client-wait-on-handshake.c
	gcc -Wall -DDEBUG -o client-no-tty-tester BrokenClients/client-no-tty-tester.c

notty:
	gcc -Wall -DDEBUG -o client-no-tty-tester BrokenClients/client-no-tty-tester.c
