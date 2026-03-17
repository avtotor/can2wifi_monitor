CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -Isrc
LDFLAGS = -lws2_32
TARGET = can_monitor.exe

SRCS = src/main.cpp src/connection.cpp src/gvret_parser.cpp src/frame_store.cpp src/display.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
