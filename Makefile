DEBUG = 0
CXX_STD = c++17

CXX = g++
CFLAGS += -std=$(CXX_STD) -Wall -march=native

ifeq ($(DEBUG), 1)
	CFLAGS += -g -Og
else
	CFLAGS += -DNDEBUG -O3
endif

LINKER = $(CXX)
LFLAGS += -Wall $(LIBS)

SRC_DIR = sifsim
OBJ_DIR = obj
BIN_DIR = bin

DEPS = $(wildcard $(SRC_DIR)/*.h) $(wildcard $(SRC_DIR)/pcg/*.hpp) $(wildcard $(SRC_DIR)/rapidjson/*.h) $(wildcard $(SRC_DIR)/rapidjson/error/*.h) $(wildcard $(SRC_DIR)/rapidjson/internal/*.h)

OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.cpp))

LIBS = -lm -lpthread

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(DEPS)
	mkdir -p $(OBJ_DIR)
	$(CXX) -c -o $@ $< $(CFLAGS)

$(BIN_DIR)/sifsim: $(OBJS)
	mkdir -p $(BIN_DIR)
	$(LINKER) -o $@ $^ $(LFLAGS)

.PHONY: all
all: $(BIN_DIR)/sifsim

.PHONY: clean
clean:
	rm $(OBJ_DIR)/*.o
