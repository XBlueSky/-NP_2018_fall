all: simple_npServer.cpp single_npServer.cpp multi_npServer.cpp semaphore.h
	g++ simple_npServer.cpp -o np_simple
	g++ single_npServer.cpp -o np_single_proc
	g++ multi_npServer.cpp -o np_multi_proc
clean: 
	rm -f np_single
	rm -f np_single_proc
	rm -f np_multi_proc