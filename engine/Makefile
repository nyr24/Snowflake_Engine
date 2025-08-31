# Configuration
CC := clang++
SRC_DIR := src
INCLUDE_DIR := include
GLM_INCLUDE_DIR := lib/glm
SHADER_SRC_DIR := shaders
SHADER_COMPILER := $(VULKAN_SDK)/bin/slangc
BUILD_MODE ?= debug
OBJ_BUILD_ROOT_DIR := build
LIB_NAME ?= snowflake
OBJ_BUILD_DIR := $(OBJ_BUILD_ROOT_DIR)/$(BUILD_MODE)
LIB_BUILD_DIR := ../build/$(BUILD_MODE)
LIB_TARGET := $(LIB_BUILD_DIR)/lib$(LIB_NAME).so
SHADER_BUILD_DIR := $(LIB_BUILD_DIR)/shaders

# Files
SRCS := $(shell find $(SRC_DIR) -name "*.cpp")
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_BUILD_DIR)/%.o,$(SRCS))
DEPS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_BUILD_DIR)/%.d,$(SRCS))
HEADERS := $(shell find $(INCLUDE_DIR) -name "*.hpp")
SHADER_SRCS := $(shell find $(SHADER_SRC_DIR) -name "*.slang")

# Compiler flags
CFLAGS := -std=c++20 -MMD -fPIC

ifeq ($(BUILD_MODE),debug)
	CFLAGS += -g -O0 -DSF_DEBUG -DSF_ASSERTS_ENABLED
else
	CFLAGS += -O3 -DSF_RELEASE
endif

# Linker flags
LDFLAGS := -lvulkan

ifeq ($(PLATFORM),wl)
	CFLAGS += -DSF_BUILD_WAYLAND
	LDFLAGS += -lwayland-client -lxkbcommon
else ifeq ($(PLATFORM),x11)
	CFLAGS += -DSF_BUILD_X11
	LDFLAGS += -lxcb -lxcb-xkb -lX11 -lX11-xcb
else
	CFLAGS += -DSF_BUILD_WINDOWS
endif

# Shader compilation rules
SPIRV_FILES := $(patsubst $(SHADER_SRC_DIR)/%.slang,$(SHADER_BUILD_DIR)/%.spv,$(SHADER_SRCS))

.PHONY: all clean shaders

all: $(LIB_TARGET) shaders

$(LIB_TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) -shared $(LDFLAGS) -o $@ $^
	@echo $(BUILD_MODE) build executed for engine successfully!

$(OBJ_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -I$(GLM_INCLUDE_DIR) -I$(VULKAN_SDK)/include -c $< -o $@

$(SHADER_BUILD_DIR)/%.spv: $(SHADER_SRC_DIR)/%.slang
	@mkdir -p $(dir $@)
	$(SHADER_COMPILER) $< -target spirv -profile spirv_1_4 -emit-spirv-directly \
	-fvk-use-entrypoint-name -entry vertMain -entry fragMain -o $@
	@echo shaders compiled for engine successfully!

shaders: $(SPIRV_FILES)

clean:
	@echo performing cleanup for engine!
	rm -rf $(OBJ_BUILD_ROOT_DIR)/$(BUILD_MODE)

.PRECIOUS: $(SHADER_BUILD_DIR)/%.spv

-include $(DEPS)
