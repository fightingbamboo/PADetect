# PADetect C++ Makefile
# 用于编译C++源文件的Makefile

# 编译器设置
CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# 架构设置 - 支持多架构编译
# 可以通过 ARCHS 参数指定架构，例如：
# make ARCHS="x86_64 arm64"  # 编译通用二进制
# make ARCHS="arm64"         # 仅编译arm64
# make ARCHS="x86_64"        # 仅编译x86_64
ARCHS ?= $(shell uname -m)

# 添加架构标志到编译选项
ifneq ($(ARCHS),)
    ARCH_FLAGS = $(addprefix -arch ,$(ARCHS))
    CXXFLAGS += $(ARCH_FLAGS)
endif

# C++17 filesystem 支持
CXXFLAGS += -DHAS_FILESYSTEM=1 -DSTD_FILESYSTEM_AVAILABLE=1

# 检查是否需要链接 stdc++fs (主要针对较老的 GCC 版本)
COMPILER_VERSION := $(shell $(CXX) --version 2>/dev/null | head -n1)
ifneq ($(findstring gcc,$(COMPILER_VERSION)),)
    GCC_VERSION := $(shell $(CXX) -dumpversion 2>/dev/null | cut -d. -f1)
    ifeq ($(shell test $(GCC_VERSION) -lt 9; echo $$?),0)
        LIBS += -lstdc++fs
    endif
endif

# 平台检测和宏定义
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	CXXFLAGS += -DPLATFORM_MACOS
else ifeq ($(UNAME_S),Linux)
	CXXFLAGS += -DPLATFORM_LINUX
else
	CXXFLAGS += -DPLATFORM_WINDOWS
endif
INCLUDES = -I. -I/usr/local/include -I/opt/homebrew/include
LIBS = -L/usr/local/lib -L/opt/homebrew/lib

# 检测Homebrew安装路径
HOMEBREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo "/usr/local")
INCLUDES += -I$(HOMEBREW_PREFIX)/include
LIBS += -L$(HOMEBREW_PREFIX)/lib

# spdlog 库设置
SPDLOG_INCLUDE := $(shell find $(HOMEBREW_PREFIX)/include $(HOMEBREW_PREFIX)/Cellar/spdlog/*/include -name "spdlog" -type d 2>/dev/null | head -1)
ifeq ($(SPDLOG_INCLUDE),)
	SPDLOG_INCLUDE := $(shell find /usr/local/include /opt/homebrew/include -name "spdlog" -type d 2>/dev/null | head -1)
endif
ifneq ($(SPDLOG_INCLUDE),)
    INCLUDES += -I$(dir $(SPDLOG_INCLUDE))
endif

# OpenCV 库设置 (可选)
OPENCV_PREFIX := $(shell pkg-config --variable=prefix opencv4 2>/dev/null || echo "")
ifneq ($(OPENCV_PREFIX),)
    OPENCV_FLAGS := $(shell pkg-config --cflags opencv4 2>/dev/null)
    OPENCV_LIBS := $(shell pkg-config --libs opencv4 2>/dev/null)
    CXXFLAGS += $(OPENCV_FLAGS) -DHAVE_OPENCV=1 -DHAS_OPENCV=1
    LIBS += $(OPENCV_LIBS)
else
    # 如果没有OpenCV，添加一个宏定义
    CXXFLAGS += -DNO_OPENCV=1 -DHAS_OPENCV=0
endif

# 平台检测
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS 特定设置
    CXXFLAGS += -DPLATFORM_MACOS=1 -DPLATFORM_WINDOWS=0 -DPLATFORM_LINUX=0
    FRAMEWORKS = -framework Foundation -framework CoreFoundation -framework CoreGraphics -framework AppKit -framework AVFoundation
    LIBS += $(FRAMEWORKS)
else ifeq ($(UNAME_S),Linux)
    # Linux 特定设置
    CXXFLAGS += -DPLATFORM_LINUX=1 -DPLATFORM_WINDOWS=0 -DPLATFORM_MACOS=0
    LIBS += -lX11 -lgtk-3 -lgdk-3
else
    # Windows 特定设置 (如果使用MinGW)
    CXXFLAGS += -DPLATFORM_WINDOWS=1 -DPLATFORM_MACOS=0 -DPLATFORM_LINUX=0
    LIBS += -lgdi32 -luser32 -lkernel32 -lwtsapi32 -lshcore
endif

# OpenSSL 库
LIBS += -lssl -lcrypto

# curl 库
LIBS += -lcurl

# jsoncpp 库
LIBS += $(shell pkg-config --libs jsoncpp)


# MNN 框架设置
MNN_ROOT = ./mnn_3.2.0
MNN_FRAMEWORK_PATH = $(MNN_ROOT)/Static/MNN.framework
MNN_HEADERS = $(MNN_FRAMEWORK_PATH)/Versions/A/Headers
MNN_LIBRARY = $(MNN_FRAMEWORK_PATH)/Versions/A/MNN

# 添加MNN头文件路径
INCLUDES += -I$(MNN_HEADERS)

# 添加MNN库链接
ifeq ($(UNAME_S),Darwin)
    # macOS使用framework方式链接
    LIBS += -F$(MNN_ROOT)/Static -framework MNN
else
    # Linux使用静态库方式链接
    LIBS += $(MNN_LIBRARY)
endif

# spdlog 库
LIBS += $(shell pkg-config --libs spdlog)

# OpenCV 库 (如果需要)
# OPENCV_FLAGS = $(shell pkg-config --cflags opencv4)
# OPENCV_LIBS = $(shell pkg-config --libs opencv4)
# CXXFLAGS += $(OPENCV_FLAGS)
# LIBS += $(OPENCV_LIBS)

# 源文件
SOURCES = \
    CommonUtils.cpp \
    ConfigParser.cpp \
    HttpClient.cpp \
    KeyVerifier.cpp \
    MyWindMsgBox.cpp \
    PADetectCore.cpp \
    PicFileUploader.cpp \
    MNNDetector.cpp \
    DeviceInfo.cpp \
    LogPathUtils.cpp

# Objective-C++ 源文件 (仅macOS)
ifeq ($(UNAME_S),Darwin)
    MM_SOURCES = PlatformCompat.mm MyWindMsgBox_macOS.mm ImageProcessor.mm
    MM_OBJECTS = $(MM_SOURCES:.mm=.o)
else
    MM_SOURCES = PlatformCompat.cpp ImageProcessor.cpp.windows
    MM_OBJECTS = PlatformCompat.o ImageProcessor.o
endif

# 构建目录
BUILD_DIR = build

# 目标文件（放在构建目录中）
ifeq ($(UNAME_S),Darwin)
    # macOS平台：使用.mm文件，不包含ImageProcessor.cpp
    OBJECTS = $(addprefix $(BUILD_DIR)/,$(SOURCES:.cpp=.o)) $(addprefix $(BUILD_DIR)/,$(MM_SOURCES:.mm=.o))
else
    # 其他平台：使用所有.cpp文件包括ImageProcessor.cpp
    OBJECTS = $(addprefix $(BUILD_DIR)/,$(SOURCES:.cpp=.o)) $(addprefix $(BUILD_DIR)/,$(MM_SOURCES:.cpp=.o))
endif

# 目标静态库
TARGET = libPADetectCore.a

# 通用二进制目标（同时编译x86_64和arm64）
universal: ARCHS = x86_64 arm64
universal: $(TARGET)

# 单独架构目标
x86_64: ARCHS = x86_64
x86_64: $(TARGET)

arm64: ARCHS = arm64
arm64: $(TARGET)

# 默认目标
all: $(TARGET)

# 创建静态库
$(TARGET): $(OBJECTS)
	@echo "Creating static library $(TARGET)..."
	ar rcs $(TARGET) $(OBJECTS)
	@echo "Build completed: $(TARGET)"

# 创建构建目录
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# 编译规则
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Objective-C++ 编译规则
$(BUILD_DIR)/%.o: %.mm | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 清理
clean:
	@echo "Cleaning..."
	rm -rf $(BUILD_DIR)
	@if [ -f "$(TARGET)" ]; then rm -f "$(TARGET)"; fi
	@echo "Clean completed"

# 重新构建
rebuild: clean all

# 安装依赖 (macOS)
install-deps-macos:
	@echo "Installing dependencies for macOS..."
	brew install openssl spdlog
	@echo "Optional: Install OpenCV with 'brew install opencv'"
	@echo "Dependencies installation completed"

# 安装依赖 (Ubuntu/Debian)
install-deps-linux:
	@echo "Installing dependencies for Linux..."
	sudo apt-get update
	sudo apt-get install libssl-dev libx11-dev libgtk-3-dev libspdlog-dev
	@echo "Optional: Install OpenCV with 'sudo apt-get install libopencv-dev'"
	@echo "Dependencies installation completed"

# 检查依赖
check-deps:
	@echo "Checking dependencies..."
	@echo "Homebrew prefix: $(HOMEBREW_PREFIX)"
	@echo "spdlog include: $(SPDLOG_INCLUDE)"
	@echo "OpenCV prefix: $(OPENCV_PREFIX)"
	@if [ -z "$(SPDLOG_INCLUDE)" ]; then \
		echo "Warning: spdlog not found. Run 'make install-deps-macos' to install."; \
	else \
		echo "spdlog: Found"; \
	fi
	@if [ -z "$(OPENCV_PREFIX)" ]; then \
		echo "Warning: OpenCV not found (optional)"; \
	else \
		echo "OpenCV: Found"; \
	fi

# 调试版本
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# 发布版本
release: CXXFLAGS += -DNDEBUG
release: $(TARGET)

# 显示帮助
help:
	@echo "Available targets:"
	@echo "  all        - Build the project (default)"
	@echo "  universal  - Build universal binary (x86_64 + arm64)"
	@echo "  x86_64     - Build for x86_64 architecture only"
	@echo "  arm64      - Build for arm64 architecture only"
	@echo "  clean      - Remove object files and executable"
	@echo "  rebuild    - Clean and build"
	@echo "  debug      - Build debug version"
	@echo "  release    - Build release version"
	@echo "  check-deps - Check if dependencies are installed"
	@echo "  install-deps-macos - Install dependencies on macOS"
	@echo "  install-deps-linux - Install dependencies on Linux"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Architecture options:"
	@echo "  ARCHS=\"x86_64 arm64\" - Compile for multiple architectures"
	@echo "  ARCHS=\"arm64\"        - Compile for arm64 only"
	@echo "  ARCHS=\"x86_64\"       - Compile for x86_64 only"
	@echo ""
	@echo "Examples:"
	@echo "  make universal          # Build universal binary"
	@echo "  make ARCHS=\"x86_64 arm64\" # Same as universal"
	@echo "  make ARCHS=\"arm64\"      # Build for Apple Silicon only"

# 声明伪目标
.PHONY: all universal x86_64 arm64 clean rebuild debug release check-deps install-deps-macos install-deps-linux help

# 依赖关系 (可选，用于头文件变化时重新编译)
# 只为实际编译的源文件生成依赖
CPP_SOURCES = $(SOURCES)
ifeq ($(UNAME_S),Darwin)
    # macOS平台：不包含ImageProcessor.cpp
    ACTUAL_SOURCES = $(CPP_SOURCES) $(MM_SOURCES)
else
    # 其他平台：包含ImageProcessor.cpp
    ACTUAL_SOURCES = $(CPP_SOURCES) $(MM_SOURCES)
endif

ACTUAL_DEPS = $(addprefix $(BUILD_DIR)/,$(ACTUAL_SOURCES:.cpp=.d))
ACTUAL_DEPS := $(ACTUAL_DEPS:.mm=.d)

-include $(ACTUAL_DEPS)

$(BUILD_DIR)/%.d: %.cpp | $(BUILD_DIR)
	@set -e; rm -f $@; \
	$(CXX) -MM $(CXXFLAGS) $(INCLUDES) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(BUILD_DIR)/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(BUILD_DIR)/%.d: %.mm | $(BUILD_DIR)
	@set -e; rm -f $@; \
	$(CXX) -MM $(CXXFLAGS) $(INCLUDES) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(BUILD_DIR)/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$