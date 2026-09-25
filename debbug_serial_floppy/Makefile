CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lrt

# Leer versión desde archivo VERSION
VERSION := $(shell cat ../VERSION 2>/dev/null || echo "0.0.0")
CXXFLAGS += -DVERSION=\"$(VERSION)\"

TARGET = pico_serial_debug
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

all: $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

install: $(BIN_DIR)/$(TARGET)
	sudo cp $(BIN_DIR)/$(TARGET) /usr/local/bin/

run: $(BIN_DIR)/$(TARGET)
	./$(BIN_DIR)/$(TARGET) /dev/ttyACM1

version:
	@echo $(VERSION)

.PHONY: all clean install run version