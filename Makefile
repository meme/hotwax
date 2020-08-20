PWD:=$(shell pwd)/

AFL_DIR:=$(PWD)AFL/
LLVM_MODE_DIR:=$(AFL_DIR)llvm_mode/
AFL_SHOW_MAP:=$(AFL_DIR)afl-showmap
AFL_CLANG_FAST:=$(AFL_DIR)afl-clang-fast
AFL_FUZZ:=$(AFL_DIR)afl-fuzz
AFL_FLAGS:=-m 128

DEV_KIT_URL:=https://github.com/frida/frida/releases/download/12.11.10/frida-gum-devkit-12.11.10-linux-x86_64.tar.xz
BIN_DIR:=$(PWD)bin/
DEV_KIT:=$(BIN_DIR)frida-gum-devkit-12.11.10-linux-x86_64.tar.xz
DEV_KIT_DIR:=$(BIN_DIR)devkit/
LIB_FRIDA_GUM:=$(DEV_KIT_DIR)libfrida-gum.a

INCLUDES:= -I$(PWD)inc/ -I$(DEV_KIT_DIR)
CFLAGS:= -g -Wall -Wno-unused-function $(INCLUDES)
LDFLAGS:= -lpthread -ldl -lresolv 

SRC_DIR:=$(PWD)src/
BASELINE_DIR:=$(SRC_DIR)baseline/
TARGET_DIR:=$(SRC_DIR)target/
SRC_FILES:=$(wildcard $(SRC_DIR)*.c)

TARGET_FORK:=$(BIN_DIR)target_fork
TARGET_PERSISTENT:=$(BIN_DIR)target_persistent
FORK_INSTR:=$(BIN_DIR)fork_instr
PERSISTENT_INSTR:=$(BIN_DIR)persistent_instr

TEST_CASE_DIR:=$(BIN_DIR)testcase_dir/
SAMPLE_TXT:=$(TEST_CASE_DIR)sample.txt
FINDINGS_DIR:=$(BIN_DIR)findings_dir/

all: $(TARGET_FORK) $(TARGET_PERSISTENT) $(FORK_INSTR) $(PERSISTENT_INSTR)

clean:
	rm -rf $(BIN_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

## AFL

$(LLVM_MODE_DIR):
	git submodule init
	git submodule update

$(AFL_SHOW_MAP): | $(LLVM_MODE_DIR)
	make -C $(AFL_DIR)

$(AFL_CLANG_FAST): $(AFL_SHOW_MAP)
	make -C $(LLVM_MODE_DIR)

afl: $(AFL_CLANG_FAST)

## DEVKIT

$(DEV_KIT): | $(BIN_DIR)
	wget -O $(DEV_KIT) $(DEV_KIT_URL)

$(DEV_KIT_DIR): |$(BIN_DIR)
	mkdir -p $@

$(LIB_FRIDA_GUM): | $(DEV_KIT) $(DEV_KIT_DIR)
	tar Jxf $(DEV_KIT) -C $(DEV_KIT_DIR)

devkit: $(LIB_FRIDA_GUM)

## TARGET_FORK

$(TARGET_FORK): $(SRC_FILES) \
		$(TARGET_DIR)target.c $(TARGET_DIR)target_fork.c $(LIB_FRIDA_GUM)
	$(CC) $(CFLAGS) \
		-o $@ \
		$(SRC_FILES) \
		$(TARGET_DIR)target.c \
		$(TARGET_DIR)target_fork.c \
		$(LIB_FRIDA_GUM) \
		$(LDFLAGS)

target_fork: $(TARGET_FORK)

## TARGET_PERSISTENT 

$(TARGET_PERSISTENT): $(SRC_FILES) \
		$(TARGET_DIR)target.c $(TARGET_DIR)target_persistent.c $(LIB_FRIDA_GUM)
	$(CC) $(CFLAGS) \
		-o $@ \
		$(SRC_FILES) \
		$(TARGET_DIR)target.c \
		$(TARGET_DIR)target_persistent.c \
		$(LIB_FRIDA_GUM) \
		$(LDFLAGS)

target_persistent: $(TARGET_PERSISTENT)

## FORK_INSTR

$(FORK_INSTR): $(AFL_CLANG_FAST) \
		$(BASELINE_DIR)fork_instr.c $(TARGET_DIR)target.c
	$(AFL_CLANG_FAST) \
		-o $@ \
		$(BASELINE_DIR)fork_instr.c \
		$(TARGET_DIR)target.c

fork_instr: $(FORK_INSTR)

# PERSISTENT_INSTR

$(PERSISTENT_INSTR): $(AFL_CLANG_FAST) \
		$(BASELINE_DIR)persistent_instr.c $(TARGET_DIR)target.c
	$(AFL_CLANG_FAST) \
		-o $@ \
		$(BASELINE_DIR)persistent_instr.c \
		$(TARGET_DIR)target.c

persistent_instr: $(PERSISTENT_INSTR)

## TEST CASE

$(TEST_CASE_DIR): | $(BIN_DIR)
	mkdir -p $@

$(FINDINGS_DIR): | $(BIN_DIR)
	mkdir -p $@

$(SAMPLE_TXT): | $(TEST_CASE_DIR)
	echo "AAA" > $@

## RUN

run_target_fork: $(TARGET_FORK) $(SAMPLE_TXT) $(FINDINGS_DIR)
	$(AFL_FUZZ) $(AFL_FLAGS) -i $(TEST_CASE_DIR) -o $(FINDINGS_DIR) $<

run_target_persistent: $(TARGET_PERSISTENT) $(SAMPLE_TXT) $(FINDINGS_DIR)
	$(AFL_FUZZ) $(AFL_FLAGS) -i $(TEST_CASE_DIR) -o $(FINDINGS_DIR) $<

run_fork_instr: $(FORK_INSTR) $(SAMPLE_TXT) $(FINDINGS_DIR)
	$(AFL_FUZZ) $(AFL_FLAGS) -i $(TEST_CASE_DIR) -o $(FINDINGS_DIR) $<

run_persistent_instr: $(PERSISTENT_INSTR) $(SAMPLE_TXT) $(FINDINGS_DIR)
	$(AFL_FUZZ) $(AFL_FLAGS) -i $(TEST_CASE_DIR) -o $(FINDINGS_DIR) $<

