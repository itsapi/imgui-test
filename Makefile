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


# Supply make with DEBUG=1 to switch to debug build
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	DEBUG_FLAGS = -O0 -ggdb -D_DEBUG
else
	DEBUG_FLAGS =
endif

EXE = imgui_test.out
OBJS = main.o imgui_impl_sdl_gl3.o
OBJS += imgui.o imgui_demo.o imgui_draw.o
OBJS += main-loop.o shader.o
OBJS += libs/gl3w/GL/gl3w.o

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


.cpp.o:
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

all: $(EXE)
	@echo Build complete for $(ECHO_MESSAGE)

$(EXE): $(OBJS)
	$(CXX) -o $(EXE) $(OBJS) $(CXXFLAGS) $(LIBS)

-include $(OBJS:.o=.d)

clean:
	rm $(EXE) $(OBJS) $(OBJS:.o=.d)
