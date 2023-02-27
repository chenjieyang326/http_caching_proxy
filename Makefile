all: main

main: client.h main.cpp parser_request.h parser_response.h proxy.cpp proxy.h response.h test_parser.cpp utils.cpp utils.h
	g++ -g  -o main main.cpp utils.cpp proxy.cpp -lpthread

.PHONY:
	clean
clean:
	rm -rf *.o main