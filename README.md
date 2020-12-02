A simple multiplayer memory game, where several clients can connect to a server and play on the same game. The project was developed in the C language in the context of Systems Programming IST Course.

Compilation instructions:
	gcc -c `sdl2-config --cflags` board_library.c
	gcc -c `sdl2-config --cflags` event_library.c
	gcc -c `sdl2-config --cflags` UI_library.c
	gcc -c `sdl2-config --cflags` server.c
	gcc -c `sdl2-config --cflags` client.c
	gcc -c `sdl2-config --cflags` bot.c
	gcc -o server server.o UI_library.o board_library.o event_library.o `sdl2-config --libs` -lSDL2_ttf -lpthread -lm
	gcc -o client client.o UI_library.o board_library.o event_library.o `sdl2-config --libs` -lSDL2_ttf -lpthread -lm
	gcc -o bot bot.o board_library.o event_library.o `sdl2-config --libs` -lSDL2_ttf -lpthread -lm
