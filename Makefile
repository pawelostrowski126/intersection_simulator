# Compiler and flags
CC       := gcc
CFLAGS   := -Wall -Wextra -pedantic -std=c17 -g
CPPFLAGS := -Iinclude -MMD -MP
LDFLAGS  := 
LDLIBS   := -lm -lncurses

# Project layout
TARGET   := traffic_sim
SRC_DIR  := src
OBJ_DIR  := build

# Automatically gather all 6 .c files and map them to build/*.o
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS     := $(OBJS:.o=.d)

# Default target
all: $(TARGET)

# Link all object files into the traffic_sim binary
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Compile each src/*.c into build/*.o and generate .d header dependency files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Create the build/ folder automatically
$(OBJ_DIR):
	mkdir -p $@

# Convenience target to build and launch with the sample scenario
run: $(TARGET)
	./$(TARGET) data/sample_scenario.txt

# Remove compiled binaries, object files, and temporary test saves
clean:
	rm -rf $(OBJ_DIR) $(TARGET) data/save_state.txt

# Automatically track changes to any header in include/
-include $(DEPS)

.PHONY: all run clean