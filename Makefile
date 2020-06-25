xx = g++

CFLAGS = -Wall -O -g -std=c++14
TARGET=sig_server
%.o:%.cpp
	$(xx) $(CFLAGS) -c $< -o $@

SOURCES=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp,%.o,$(SOURCES))
$(TARGET): $(OBJS)
	$(xx) $(OBJS) -o $(TARGET)

cleanObj:
	rm -r *.o
clean:
	rm -r *.o sig_server

.PHONY:$(TARGET) clean
