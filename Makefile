#
# Cross Platform Makefile
# Compatible with MSYS2/MINGW, Ubuntu 14.04.1 and Mac OS X
#
#
# You will need SDL2 (http://www.libsdl.org)
#
#   apt-get install libsdl2-dev  # Linux
#   brew install sdl2            # Mac OS X
#   pacman -S mingw-w64-i686-SDL # MSYS2
#

CXX = clang++
CC = clang

# Supply make with DEBUG=1 to switch to debug build
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	DEBUG_FLAGS = -O0 -ggdb -D_DEBUG
	BUILD_DIR = build-debug
else
	DEBUG_FLAGS =
	BUILD_DIR = build
endif

EXE = $(BUILD_DIR)/imgui_test.out

CXXSRCS = $(shell find -type f -name '*.cpp')
CXXOBJS = $(CXXSRCS:./%.cpp=$(BUILD_DIR)/%.o)

CSRCS += $(shell find -type f -name '*.c')
COBJS = $(CSRCS:./%.c=$(BUILD_DIR)/%.o)

UNAME_S := $(shell uname -s)


ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS = -lGL -ldl `sdl2-config --libs`

	CXXFLAGS = -I. -Ilibs/gl3w `sdl2-config --cflags`
	CXXFLAGS += -Werror -Wformat -std=c++14
	CXXFLAGS += $(DEBUG_FLAGS)
	CFLAGS = -I. -Ilibs/gl3w `sdl2-config --cflags`
	CFLAGS += $(DEBUG_FLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo `sdl2-config --libs`

	CXXFLAGS = -I. -Ilibs/gl3w -I/usr/local/include `sdl2-config --cflags`
	CXXFLAGS += -Werror -Wformat -std=c++14
	CXXFLAGS += $(DEBUG_FLAGS)
	CFLAGS = -I. -Ilibs/gl3w `sdl2-config --cflags`
	CFLAGS += $(DEBUG_FLAGS)
endif

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
  ECHO_MESSAGE = "Windows"
  LIBS = -lgdi32 -lopengl32 -limm32 `pkg-config --static --libs sdl2`

  CXXFLAGS = -Ilibs/gl3w `pkg-config --cflags sdl2`
  CXXFLAGS += -Werror -Wformat -std=c++14
	CXXFLAGS += $(DEBUG_FLAGS)
	CFLAGS = -I. -Ilibs/gl3w `sdl2-config --cflags`
	CFLAGS += $(DEBUG_FLAGS)
endif


$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

all: $(EXE)
	@echo Build complete for $(ECHO_MESSAGE)

$(EXE): $(CXXOBJS) $(COBJS)
	$(warning $(CXXOBJS) $(COBJS))
	$(CXX) -o $(EXE) $(CXXOBJS) $(COBJS) $(CXXFLAGS) $(LIBS)

-include $(COBJS:.o=.d)

-include $(CXXOBJS:.o=.d)

clean:
	rm $(BUILD_DIR) -r
