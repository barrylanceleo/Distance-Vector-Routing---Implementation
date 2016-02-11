all:
	gcc -o dv_implementation main.c Server.c Server.h main.h SocketUtils.c SocketUtils.h list.c list.h user_interface.c user_interface.h
