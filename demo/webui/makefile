ELF = webui
ELDFLAGS = -L$(LIB_DIR) -lbacnet $(LDFLAGS) -lz

CSRC = $(shell find -name '*.c')
CPPSRC = $(shell find -name '*.cpp')
OBJ = $(CSRC:%.c=%.o) $(CPPSRC:%.cpp=%.o)

depend: .depend_c .depend_cpp

.depend_c: $(CSRC)
	rm -f $@
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -MM $^ > $@;
	sed -i 's/^\(.*\).o:\( \(.*\)\1.c .*\)$$/\3\1.o:\2/' $@

.depend_cpp: $(CPPSRC)
	rm -f $@
	$(CPP) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) -MM $^ > $@;
	sed -i 's/^\(.*\).o:\( \(.*\)\1.cpp .*\)$$/\3\1.o:\2/' $@

-include .depend_c .depend_cpp

.cpp.o:
	$(CPP) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

all: $(ELF) bacnetrouter factory_test slave_demo
.PHONY : all clean

WEBUIOBJ = $(filter-out %bacnetrouter.o %factory_test.o %slave_demo.o, $(OBJ))

$(ELF): $(WEBUIOBJ) $(LIB_DIR)/libbacnet.a
	$(CPP) -o $(ELF) $(WEBUIOBJ) $(ELDFLAGS)

bacnetrouter: bacnetrouter.o $(LIB_DIR)/libbacnet.a
	$(CPP) -o bacnetrouter bacnetrouter.o $(ELDFLAGS)

factory_test: factory_test.o $(LIB_DIR)/libbacnet.a
	$(CPP) -o factory_test factory_test.o $(ELDFLAGS)

slave_demo: slave_demo.o $(LIB_DIR)/libbacnet.a
	$(CPP) -o slave_demo slave_demo.o $(ELDFLAGS)

clean:
	-rm -rf $(OBJ) $(ELF) bacnetrouter factory_test
