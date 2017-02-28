
ELF = web_client_test
ELDFLAGS = -L$(LIB_DIR) -lbacnet $(LDFLAGS)

CSRC = $(shell find -name '*.c')
CPPSRC = $(shell find -name '*.cpp')
OBJ = $(CSRC:%.c=%.o) $(CPPSRC:%.cpp=%.o)

.cpp.o:
	$(CPP) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

all: $(ELF)
.PHONY : all clean

$(ELF): $(OBJ) $(LIB_DIR)/libbacnet.a
	$(CPP) -o $(ELF) $(OBJ) $(ELDFLAGS) 

clean:
	-rm -rf $(OBJ) $(ELF)
