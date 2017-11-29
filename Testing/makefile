lab1client:
	gcc -Wall -o ~/Tests/client1 ../Lab1/client.c

lab1server:
	gcc -Wall -o ~/Tests/server1 ../Lab1/server.c

lab2client:
	gcc -Wall -DDEBUG -o ~/Tests/client2 ../Lab2/client.c

lab2server:
	gcc -Wall -DDEBUG -o ~/Tests/server2 ../Lab2/server.c

lab3client:
	gcc -Wall -DDEBUG -o ~/Tests/client3 ../Lab3/client.c

lab3server:
	gcc -Wall -pthread -DDEBUG  -o ~/Tests/server3 ../Lab3/server.c -lrt

lab4tpool:
	gcc -c -std=gnu99 ../Lab4/tpool.c -o ../Lab4/tpool.o && ar -cr libtpool.a ../Lab4/tpool.o
	gcc -Wall -DDEBUG -std=gnu99 -fpic -c ../Lab4/tpool.c && gcc -shared -o ~/Tests/libtpool.so ../Lab4/tpool.o

lab5client:
	gcc -Wall -DDEBUG -o ~/Tests/client5 ../Lab5/client.c

lab5server:
	gcc -Wall -pthread -DDEBUG -std=gnu99  -o ~/Tests/server5 ../Lab5/server.c ../Lab5/tpool.c -lrt

brokenclients:
	gcc -Wall -DDEBUG -o ~/Tests/client-child-sigint BrokenClients/client-child-sigint.c
	gcc -Wall -DDEBUG -o ~/Tests/client-child-sigquit BrokenClients/client-child-sigquit.c
	gcc -Wall -DDEBUG -o ~/Tests/client-failed-challenge BrokenClients/client-failed-challenge.c
	gcc -Wall -DDEBUG -o ~/Tests/client-failed-port BrokenClients/client-failed-port.c
	gcc -Wall -DDEBUG -o ~/Tests/client-failed-proceed BrokenClients/client-failed-proceed.c
	gcc -Wall -DDEBUG -o ~/Tests/client-failed-secret BrokenClients/client-failed-secret.c
	gcc -Wall -DDEBUG -o ~/Tests/client-parent-sigint BrokenClients/client-parent-sigint.c
	gcc -Wall -DDEBUG -o ~/Tests/client-parent-sigquit BrokenClients/client-parent-sigquit.c
	gcc -Wall -DDEBUG -o ~/Tests/client-wait-on-connect BrokenClients/client-wait-on-connect.c
	gcc -Wall -DDEBUG -o ~/Tests/client-wait-on-handshake BrokenClients/client-wait-on-handshake.c
	gcc -Wall -DDEBUG -o ~/Tests/client-no-tty-tester BrokenClients/client-no-tty-tester.c
