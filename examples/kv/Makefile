default: kv-local kv-client kv-server

.SUFFIXES:

%.o: %.cpp
	g++ -Wall -g -std=c++11 -c $< -o $@

%: %.o
	g++ -o $@ $^

kv-local: kv-client-common.o
kv-client: kv-client-common.o
