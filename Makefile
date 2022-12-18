LIBRARIES = -lX11 -lGL -lpthread -lpng -lstdc++fs -std=c++20

EXE = lin

all:
	g++ lin.cpp $(LIBRARIES) -o $(EXE)
clean:
	-rm $(EXE)
