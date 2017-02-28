#Makefile to build BACnet Library with GCC

TARGET = bacnet
LIBRARY = $(LIB_DIR)/lib$(TARGET).a

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

.PHONY: all clean depend


all: $(OBJ) 
	mkdir -p $(LIB_DIR)
	$(AR) rcs $(LIBRARY) $(OBJ)

clean:
	-rm -rf $(OBJ) $(LIBRARY)
