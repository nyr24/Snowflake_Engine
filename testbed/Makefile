# Configuration
CC := clang++
SRC_DIR := src
INCLUDE_DIR := include
SHADER_SRC_DIR := shaders
SHADER_COMPILER := $(VULKAN_SDK)/bin/slangc
BUILD_MODE ?= debug
OBJ_BUILD_ROOT_DIR := build
EXE_NAME ?= testbed
OBJ_BUILD_DIR := $(OBJ_BUILD_ROOT_DIR)/$(BUILD_MODE)
EXE_BUILD_DIR := ../build/$(BUILD_MODE)
EXE_EXTENSION :=
SF_INCLUDE_DIR := ../engine/include
SF_LIB_PATH := ../build/$(BUILD_MODE)
SF_LIB := snowflake

ifeq ($PLATFORM, win32)
	EXE_EXTENSION := .exe
endif

EXE_TARGET := $(EXE_BUILD_DIR)/$(EXE_NAME)$(EXE_EXTENSION)
SHADER_BUILD_DIR := $(EXE_BUILD_DIR)/shaders

# Files
SRCS := $(shell find $(SRC_DIR) -name "*.cpp")
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_BUILD_DIR)/%.o,$(SRCS))
DEPS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_BUILD_DIR)/%.d,$(SRCS))
HEADERS := $(shell find $(INCLUDE_DIR) -name "*.hpp")
SHADER_SRCS := $(shell find $(SHADER_SRC_DIR) -name "*.slang")

# Compiler flags
CFLAGS := -std=c++20 -MMD

ifeq ($(BUILD_MODE),debug)
	CFLAGS += -g -O0 -DSF_DEBUG -DSF_ASSERTS_ENABLED
else
	CFLAGS += -O3 -DSF_RELEASE
endif

# Linker flags
LDFLAGS := -L$(SF_LIB_PATH) -l$(SF_LIB)

.PHONY: all clean shaders

all: $(EXE_TARGET) shaders

$(EXE_TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo $(BUILD_MODE) build for testbed executed successfully!

$(OBJ_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -I$(SF_INCLUDE_DIR) -c $< -o $@

shaders: $(SPIRV_FILES)

# Shader compilation rules
SPIRV_FILES := $(patsubst $(SHADER_SRC_DIR)/%.slang,$(SHADER_BUILD_DIR)/%.spv,$(SHADER_SRCS))

$(SHADER_BUILD_DIR)/%.spv: $(SHADER_SRC_DIR)/%.slang
	@mkdir -p $(dir $@)
	$(SHADER_COMPILER) $< -target spirv -profile spirv_1_4 -emit-spirv-directly \
	-fvk-use-entrypoint-name -entry vertMain -entry fragMain -o $@
	@echo shaders compiled for testbed successfully!

clean:
	rm -rf $(OBJ_BUILD_ROOT_DIR)/$(BUILD_MODE)
	@echo cleanup performed for testbed successfully!

.PRECIOUS: $(SHADER_BUILD_DIR)/%.spv

-include $(DEPS)
