CC := gcc
CFLAGS := -g -Wall -Wextra -std=c99 -Iinclude -Isrc/libs -I.-fsanitize=address -fsanitize=undefined -MMD -MP -Wno-unused-parameter -Wno-unused-function -Wno-format-truncation
LFLAGS :=

# --- Configuration ---
BUILD_DIR := build
BIN := app

# --- Application Source Discovery ---
SRC := $(shell find src -name '*.c')
OBJ := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(filter src/%.c,$(SRC)))

# --- Dependency Files ---
# Only includes dependencies for application objects
DEP := $(OBJ:.o=.d)

.PHONY: all run clean

all: $(BIN)

# ====================================================================
# APPLICATION BUILD RULES
# ====================================================================

$(BIN): $(OBJ)
	@mkdir -p $(@D)
	@echo "LINKING all object files into $@"
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(@D)
	@echo "COMPILING $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ====================================================================
# UTILITY TARGETS
# ====================================================================

run: $(BIN)
	./$(BIN)

clean:
	@echo "CLEANING $(BUILD_DIR) and $(BIN)"
	$(RM) -rf $(BUILD_DIR) $(BIN)

# Include automatically generated dependency files
-include $(DEP)


