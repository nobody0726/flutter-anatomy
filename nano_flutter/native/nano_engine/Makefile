CXX = clang++
CXXFLAGS = -std=c++20 -O2 -Wall -fPIC -Iinclude
LDFLAGS = -shared

TARGET = ../../lib/libnano_engine.dylib

SRCS = src/nano_engine.cc
OBJS = $(SRCS:.cc=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "Built $(TARGET) successfully."

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
