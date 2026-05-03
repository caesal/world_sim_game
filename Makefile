CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -I.
LDFLAGS ?= -lgdi32 -luser32 -lmsimg32 -lgdiplus -mwindows
TARGET := world_sim.exe

SOURCES := \
	src/main.c \
	src/game.c \
	src/core/game_state.c \
	src/data/game_tables.c \
	src/world_gen.c \
	src/simulation.c \
	src/world/noise.c \
	src/world/ports.c \
	src/sim/expansion.c \
	src/sim/diplomacy.c \
	src/sim/war.c \
	src/render.c \
	src/ui.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
