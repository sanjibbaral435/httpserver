#Makefile

all: server

server: web_server.c
	gcc web_server.c -Wall -o web_server

clean: rm -f server
