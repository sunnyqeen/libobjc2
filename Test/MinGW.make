
.SUFFIXES:	.c .cxx .cc .cpp .m .mm .o
.PHONY:		all clean test install uninstall

DEPS = MinGW.make ../MinGW.conf


# TODO: on OSX this should set up a cross-compile environment.
#       Also override a few things for the future 64-bit build.
include ../MinGW.conf


LIBOBJC = ../build/libobjc.a

### Compiler/Tools options

COPT = -fexceptions -fblocks -fobjc-runtime=gnustep-1.7 -g -O2 \
	-Wall -Werror -Wno-unused-variable
COPT_ARC = -fobjc-arc
CXXOPT =
CDEFS =
CINCDIRS = -I../
LDOPT = -g
LDLIBS = -Wl,--whole-archive $(LIBOBJC) -Wl,--no-whole-archive \
	-lpthread


### Product/Build dirs

TEST_EXECS = \
	$(BUILD_DIR)/RuntimeTest.exe \
	$(BUILD_DIR)/AllocatePair.exe \
	$(BUILD_DIR)/AssociatedObject.exe \
	$(BUILD_DIR)/BlockImpTest.exe \
	$(BUILD_DIR)/BlockTest_arc.exe \
	$(BUILD_DIR)/BoxedForeignException.exe \
	$(BUILD_DIR)/ExceptionTest.exe \
	$(BUILD_DIR)/ForeignException.exe \
	$(BUILD_DIR)/Forward.exe \
	$(BUILD_DIR)/ManyManySelectors.exe \
	$(BUILD_DIR)/NestedExceptions.exe \
	$(BUILD_DIR)/PropertyAttributeTest.exe \
	$(BUILD_DIR)/PropertyIntrospectionTest.exe \
	$(BUILD_DIR)/ProtocolCreation.exe \
	$(BUILD_DIR)/ResurrectInDealloc_arc.exe \
	$(BUILD_DIR)/RuntimeTest.exe \
	$(BUILD_DIR)/WeakReferences_arc.exe \
	$(BUILD_DIR)/objc_msgSend.exe \
	$(BUILD_DIR)/msgInterpose.exe \
	$(BUILD_DIR)/CXXException.exe

	# TODO: this test probably has a bug
	# $(BUILD_DIR)/PropertyIntrospectionTest2.exe \


TEST_LOG = $(BUILD_DIR)/Test.log


### Build Rules

all: $(BUILD_DIR) $(OBJ_DIR) $(TEST_EXECS)

test: all
	@( \
	rm -f $(TEST_LOG) ; \
	for i in $(TEST_EXECS) ; do \
		echo "$$i:" ; \
		if ./$$i >> $(TEST_LOG) ; then \
			echo "---------------------------------------- Passed"; \
		else \
			echo "**************************************** FAILED"; \
		fi \
	done)

$(BUILD_DIR): $(DEPS)
	@mkdir -p $(BUILD_DIR)

$(OBJ_DIR): $(DEPS)
	@mkdir -p $(OBJ_DIR)

$(BUILD_DIR)/CXXException.exe: $(OBJ_DIR)/CXXException.m.o $(OBJ_DIR)/CXXException.cc.o $(LIBOBJC) $(DEPS)
	$(LDXX) $(LDOPT) $< $(OBJ_DIR)/CXXException.cc.o $(LDLIBS) -o $@

$(BUILD_DIR)/%_arc.exe: $(OBJ_DIR)/%_arc.m.o $(OBJ_DIR)/Test.m.o $(LIBOBJC) $(DEPS)
	$(LD) $(LDOPT) $< $(OBJ_DIR)/Test.m.o $(LDLIBS) -o $@

$(BUILD_DIR)/%.exe: $(OBJ_DIR)/%.m.o $(LIBOBJC) $(DEPS)
	$(LD) $(LDOPT) $< $(LDLIBS) -o $@

$(OBJ_DIR)/%.c.o: %.c $(DEPS)
	$(CC) $(COPT) $(CDEFS) $(CINCDIRS) -c $< -o $@

$(OBJ_DIR)/%_arc.m.o: %_arc.m $(DEPS)
	$(CC) $(COPT) $(COPT_ARC) $(CDEFS) $(CINCDIRS) -c $< -o $@

$(OBJ_DIR)/%.m.o: %.m $(DEPS)
	$(CC) $(COPT) $(CDEFS) $(CINCDIRS) -c $< -o $@

$(OBJ_DIR)/%.mm.o: %.mm $(DEPS)
	$(CXX) $(COPT) $(CXXOPT) $(CDEFS) $(CINCDIRS) -c $< -o $@

$(OBJ_DIR)/%.cc.o: %.cc $(DEPS)
	$(CXX) $(COPT) $(CXXOPT) $(CDEFS) $(CINCDIRS) -c $< -o $@


### Clean
clean:
	@rm -f $(TEST_EXECS) $(TEST_LOG) $(OBJ_DIR)/*
