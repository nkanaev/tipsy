CFLAGS = -Wall -Wextra
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)

ifeq ($(OS),Windows_NT)
	LDFLAGS = -lopengl32 -lgdi32 -lm
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		LDFLAGS = -framework OpenGL -framework Cocoa -lm
	else ifeq ($(UNAME_S),Linux)
		LDFLAGS = -lGLU -lGL -lX11 -lm
	endif
endif

default: tipsy

%.o: %.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(LDFLAGS)

tipsy: $(OBJS)
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

clean:
	rm -f $(OBJS)
	rm -f tipsy
