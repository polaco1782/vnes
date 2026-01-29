# VNES - Minimal NES Emulator
# C++11 Makefile

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -Werror -Wunused-function -O0 -g -MMD -MP
LDFLAGS = -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio

# Release build flags with security hardening
RELEASE_CXXFLAGS = -std=c++11 -O2 -D_FORTIFY_SOURCE=2 \
    -fstack-protector-strong \
    -fstack-clash-protection \
    -fcf-protection=full \
    -fPIE \
    -Wall -Wextra -Werror=format-security -MMD -MP
RELEASE_LDFLAGS = -pie \
    -Wl,-z,relro,-z,now \
    -Wl,-z,noexecstack \
    -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
RELEASE_OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.release.o)
DEPS = $(OBJECTS:.o=.d)
RELEASE_DEPS = $(RELEASE_OBJECTS:.o=.d)
DEBUG_TARGET = $(BIN_DIR)/vnes-debug
RELEASE_TARGET = $(BIN_DIR)/vnes-release

.PHONY: all clean dirs analyze debug release

all: debug

debug: dirs $(DEBUG_TARGET)

release: dirs $(RELEASE_TARGET)

analyze:
	@echo "Running cppcheck for unused functions..."
	@cppcheck --enable=unusedFunction --quiet $(SRC_DIR)/ 2>&1 || true

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

$(DEBUG_TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(RELEASE_TARGET): $(RELEASE_OBJECTS)
	$(CXX) $(RELEASE_OBJECTS) -o $@ $(RELEASE_LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.release.o: $(SRC_DIR)/%.cpp
	$(CXX) $(RELEASE_CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Include auto-generated dependencies
-include $(DEPS)
-include $(RELEASE_DEPS)
