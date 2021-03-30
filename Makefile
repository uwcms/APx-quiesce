CFLAGS := -std=c++11

all: build

build: quiesced

quiesced: quiesced.o
	$(CXX) $(CFLAGS) -o $@ $^ -ldl

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $^

clean:
	rm -f *.o quiesced *.rpm
