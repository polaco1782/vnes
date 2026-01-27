# VNES - Minimal NES Emulator
# C++11 Makefile

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -Werror -Wunused-function -O0 -g
LDFLAGS = -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TARGET = $(BIN_DIR)/vnes

.PHONY: all clean dirs analyze

all: dirs $(TARGET)

analyze:
	@echo "Running cppcheck for unused functions..."
	@cppcheck --enable=unusedFunction --quiet $(SRC_DIR)/ 2>&1 || true

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Dependencies
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp $(SRC_DIR)/bus.h $(SRC_DIR)/debugger.h $(SRC_DIR)/display.h $(SRC_DIR)/input.h $(SRC_DIR)/sound.h
$(BUILD_DIR)/cartridge.o: $(SRC_DIR)/cartridge.cpp $(SRC_DIR)/cartridge.h
$(BUILD_DIR)/cpu.o: $(SRC_DIR)/cpu.cpp $(SRC_DIR)/cpu.h $(SRC_DIR)/bus.h
$(BUILD_DIR)/ppu.o: $(SRC_DIR)/ppu.cpp $(SRC_DIR)/ppu.h $(SRC_DIR)/bus.h
$(BUILD_DIR)/apu.o: $(SRC_DIR)/apu.cpp $(SRC_DIR)/apu.h
$(BUILD_DIR)/bus.o: $(SRC_DIR)/bus.cpp $(SRC_DIR)/bus.h $(SRC_DIR)/input.h
$(BUILD_DIR)/debugger.o: $(SRC_DIR)/debugger.cpp $(SRC_DIR)/debugger.h $(SRC_DIR)/bus.h
$(BUILD_DIR)/display.o: $(SRC_DIR)/display.cpp $(SRC_DIR)/display.h
$(BUILD_DIR)/input.o: $(SRC_DIR)/input.cpp $(SRC_DIR)/input.h
$(BUILD_DIR)/sound.o: $(SRC_DIR)/sound.cpp $(SRC_DIR)/sound.h $(SRC_DIR)/apu.h
