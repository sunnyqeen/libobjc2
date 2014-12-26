
.SUFFIXES:	.c .cxx .cc .cpp .m .mm .o
.PHONY:		all clean test install uninstall

DEPS = MinGW.make MinGW.conf


# TODO: on OSX this should set up a cross-compile environment.
#       Also override a few things for the future 64-bit build.
include MinGW.conf


### Compiler/Tools options

COPT = -fexceptions -fblocks -fobjc-runtime=gnustep-1.7 -g -O2 \
	-Wall -Werror -Wno-deprecated-objc-isa-usage -Wno-objc-root-class -Wno-unused-variable
CXXOPT =
CDEFS = -DGC_DEBUG -DGNUSTEP -DNO_LEGACY -DTYPE_DEPENDENT_DISPATCH \
	-D_BSD_SOURCE=1 -D_XOPEN_SOURCE=700 -D__BSD_VISIBLE=1 \
	-D__OBJC_RUNTIME_INTERNAL__=1 -Dobjc_EXPORTS
CINCDIRS = -I./
AROPT =


### Product/Build dirs

PRODUCT_NAME = libobjc
PRODUCT = $(BUILD_DIR)/$(PRODUCT_NAME).a


### Sources

OBJS = \
	$(OBJ_DIR)/abi_version.c.o \
	$(OBJ_DIR)/alias_table.c.o \
	$(OBJ_DIR)/block_to_imp.c.o \
	$(OBJ_DIR)/caps.c.o \
	$(OBJ_DIR)/category_loader.c.o \
	$(OBJ_DIR)/class_table.c.o \
	$(OBJ_DIR)/dtable.c.o \
	$(OBJ_DIR)/eh_personality.c.o \
	$(OBJ_DIR)/encoding2.c.o \
	$(OBJ_DIR)/hash_table.c.o \
	$(OBJ_DIR)/hooks.c.o \
	$(OBJ_DIR)/ivar.c.o \
	$(OBJ_DIR)/legacy_malloc.c.o \
	$(OBJ_DIR)/loader.c.o \
	$(OBJ_DIR)/mutation.m.o \
	$(OBJ_DIR)/protocol.c.o \
	$(OBJ_DIR)/runtime.c.o \
	$(OBJ_DIR)/sarray2.c.o \
	$(OBJ_DIR)/selector_table.c.o \
	$(OBJ_DIR)/sendmsg2.c.o \
	$(OBJ_DIR)/statics_loader.c.o \
	$(OBJ_DIR)/toydispatch.c.o \
	$(OBJ_DIR)/block_trampolines.S.o \
	$(OBJ_DIR)/objc_msgSend.S.o \
	$(OBJ_DIR)/NSBlocks.m.o \
	$(OBJ_DIR)/Protocol2.m.o \
	$(OBJ_DIR)/arc.m.o \
	$(OBJ_DIR)/associate.m.o \
	$(OBJ_DIR)/blocks_runtime.m.o \
	$(OBJ_DIR)/properties.m.o \
	$(OBJ_DIR)/gc_none.c.o \
	$(OBJ_DIR)/objcxx_eh.cc.o


### Build Rules

all: $(BUILD_DIR) $(OBJ_DIR) $(PRODUCT)

$(BUILD_DIR): $(DEPS)
	@mkdir -p $(BUILD_DIR)

$(OBJ_DIR): $(DEPS)
	@mkdir -p $(OBJ_DIR)

$(PRODUCT): $(OBJS) $(DEPS)
	$(AR) $(PRODUCT) $(AROPT) $(OBJS)

$(OBJ_DIR)/%.c.o: %.c $(DEPS)
	$(CC) $(COPT) $(CDEFS) $(CINCDIRS) -c $< -o $@

$(OBJ_DIR)/%.m.o: %.m $(DEPS)
	$(CC) $(COPT) $(CDEFS) $(CINCDIRS) -c $< -o $@

$(OBJ_DIR)/%.S.o: %.S $(DEPS)
	$(CC) $(COPT) $(CDEFS) $(CINCDIRS) -c $< -o $@

$(OBJ_DIR)/%.mm.o: %.mm $(DEPS)
	$(CXX) $(COPT) $(CXXOPT) $(CDEFS) $(CINCDIRS) -c $< -o $@

$(OBJ_DIR)/%.cc.o: %.cc $(DEPS)
	$(CXX) $(COPT) $(CXXOPT) $(CDEFS) $(CINCDIRS) -c $< -o $@


### Tests
test: all
	@cd Test ; make -f MinGW.make test


### Clean
clean:
	@rm -f $(PRODUCT) $(OBJ_DIR)/*
	@cd Test ; make -f MinGW.make clean