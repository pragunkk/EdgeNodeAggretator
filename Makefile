CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -g
LDFLAGS = -lpthread -lrt

all: server client_node admin_client guest_client dashboard

server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS) -lm

client_node: client_node.c common.h
	$(CC) $(CFLAGS) -o client_node client_node.c $(LDFLAGS) -lm

admin_client: admin_client.c common.h
	$(CC) $(CFLAGS) -o admin_client admin_client.c $(LDFLAGS)

guest_client: guest_client.c common.h
	$(CC) $(CFLAGS) -o guest_client guest_client.c $(LDFLAGS)

dashboard: dashboard.c
	$(CC) $(CFLAGS) -o dashboard dashboard.c

clean:
	rm -f server client_node admin_client guest_client dashboard model_checkpoint.bin
