PWD:=$(shell pwd)/

AFL_DIR:=$(PWD)AFL/
LLVM_MODE_DIR:=$(AFL_DIR)llvm_mode/
AFL_SHOW_MAP:=$(AFL_DIR)afl-showmap
AFL_CLANG_FAST:=$(AFL_DIR)afl-clang-fast
AFL_FUZZ:=$(AFL_DIR)afl-fuzz
AFL_FLAGS:=-m 128

VERSION:=12.11.10
DEV_KIT_FILENAME:=frida-gum-devkit-$(VERSION)-$(OS)-$(ARCH).tar.xz
DEV_KIT_URL:=https://github.com/frida/frida/releases/download/$(VERSION)/$(DEV_KIT_FILENAME)
BIN_DIR:=$(PWD)bin/
OUT_DIR:=$(BIN_DIR)$(OS)-$(ARCH)/
DEV_KIT:=$(OUT_DIR)$(DEV_KIT_FILENAME)

DEV_KIT_DIR:=$(OUT_DIR)devkit/
LIB_FRIDA_GUM:=$(DEV_KIT_DIR)libfrida-gum.a

INCLUDES:= -I$(PWD)inc/ -I$(DEV_KIT_DIR)
CFLAGS:= -g -Wall -Werror -Wno-unused-function $(INCLUDES)
LDFLAGS:= -lpthread -ldl -lresolv

ifeq ($(OS),linux)
	ifeq ($(ARCH),x86)
		PLAT_CFLAGS:= -m32
	endif
	ifeq ($(ARCH),x86_64)
		PLAT_CFLAGS:=
	endif
endif
ifeq ($(OS),macos)
	ifeq ($(ARCH),x86)
$(error Unsupported platform combination)
	endif
	ifeq ($(ARCH),x86_64)
	endif
endif
ifeq ($(OS),android)
	ifeq ($(ARCH),x86)
	endif
	ifeq ($(ARCH),x86_64)
	endif
endif

SRC_DIR:=$(PWD)src/
BASELINE_DIR:=$(SRC_DIR)baseline/
TARGET_DIR:=$(SRC_DIR)target/
SRC_FILES:=$(wildcard $(SRC_DIR)*.c)

TARGET_FORK:=$(OUT_DIR)target_fork
TARGET_PERSISTENT:=$(OUT_DIR)target_persistent
FORK_INSTR:=$(BIN_DIR)fork_instr
PERSISTENT_INSTR:=$(BIN_DIR)persistent_instr
TARGETS:=$(TARGET_FORK) $(TARGET_PERSISTENT) $(FORK_INSTR) $(PERSISTENT_INSTR)

TEST_CASE_DIR:=$(BIN_DIR)testcase_dir/
SAMPLE_TXT:=$(TEST_CASE_DIR)sample.txt
FINDINGS_DIR:=$(BIN_DIR)findings_dir/

CANDIDATE_OS:=linux macos android
CANDIDATE_ARCH:=x86 x86_64
EMPTY :=
SPACE := $(EMPTY) $(EMPTY)
JOIN = $(subst $(SPACE),$1,$(strip $2))

# name, dependency
define os_arch_target
$(foreach arch,$(CANDIDATE_ARCH),$(eval $(call build_arch,$(arch),$(1),$(2))))
endef

# arch, name, dependency
define build_arch
$(foreach os,$(CANDIDATE_OS),$(eval $(call build_os,$(os),$(1),$(2),$(3))))
endef

# os, arch, name, dependency
define build_os
$(3)$(1)-$(2):
	OS=$(1) ARCH=$(2) make $(4)
endef

TARGET_COLOUR = "\033[32m";
VARIABLE_COLOUR = "\033[36m"
RESET_COLOUR = "\033[0m"

#name
define os_arch_show
$(foreach arch,$(CANDIDATE_ARCH),$(eval $(call show_arch,$(arch),$(1))))
.show-$(1):
	@echo Component $(1)

SHOW+=$$(SHOW-$(1))
endef

# arch, name
define show_arch
$(foreach os,$(CANDIDATE_OS),$(eval $(call show_os,$(os),$(1),$(2))))
endef

# os, arch, name
define show_os
.show-$(3)$(1)-$(2): .show-$(3)
	@echo \\t$(3)$(1)-$(2);

SHOW-$(3)+=.show-$(3)$(1)-$(2)
endef

$(eval $(call os_arch_show,devkit))

default: $(SHOW)
	@echo "default"


$(eval $(call os_arch_target,,.targets))

clean:
	rm -rf $(BIN_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OUT_DIR): | $(BIN_DIR)
	mkdir -p $(OUT_DIR)

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
ifeq ($(and $(ARCH),$(OS)),)
.check:
	@echo "OS must be selected ($(call JOIN,/,$(CANDIDATE_OS))) - selected = [$(OS)]";
	@echo "ARCH must be selected ($(call JOIN,/,$(CANDIDATE_ARCH))) - selected = [$(ARCH)]";
	@false;
else
.check:
endif

$(DEV_KIT): | .check $(DEV_KIT_DIR)
	wget -O $(DEV_KIT) $(DEV_KIT_URL)

$(DEV_KIT_DIR): |$(OUT_DIR)
	mkdir -p $@

$(LIB_FRIDA_GUM): | .check $(DEV_KIT) $(DEV_KIT_DIR)
	tar Jxf $(DEV_KIT) -C $(DEV_KIT_DIR)

.devkit: $(LIB_FRIDA_GUM)

$(eval $(call os_arch_target,devkit-,.devkit))

## TARGET_FORK

$(TARGET_FORK): $(SRC_FILES) \
		$(TARGET_DIR)target.c $(TARGET_DIR)target_fork.c $(LIB_FRIDA_GUM) | $(OUT_DIR)
	$(CC) $(CFLAGS) $(PLAT_CFLAGS) \
		-o $@ \
		$(SRC_FILES) \
		$(TARGET_DIR)target.c \
		$(TARGET_DIR)target_fork.c \
		$(LIB_FRIDA_GUM) \
		$(LDFLAGS)

.target-fork: $(TARGET_FORK)

$(eval $(call os_arch_target,target-fork-,.target-fork))

## TARGET_PERSISTENT

$(TARGET_PERSISTENT): $(SRC_FILES) \
		$(TARGET_DIR)target.c $(TARGET_DIR)target_persistent.c $(LIB_FRIDA_GUM) | $(OUT_DIR)
	$(CC) $(CFLAGS) $(PLAT_CFLAGS) \
		-o $@ \
		$(SRC_FILES) \
		$(TARGET_DIR)target.c \
		$(TARGET_DIR)target_persistent.c \
		$(LIB_FRIDA_GUM) \
		$(LDFLAGS)

.target_persistent: $(TARGET_PERSISTENT)

$(eval $(call os_arch_target,target-persistent-,.target_persistent))

## FORK_INSTR

$(FORK_INSTR): $(AFL_CLANG_FAST) \
		$(BASELINE_DIR)fork_instr.c $(TARGET_DIR)target.c
	$(AFL_CLANG_FAST) \
		-o $@ \
		$(BASELINE_DIR)fork_instr.c \
		$(TARGET_DIR)target.c

# PERSISTENT_INSTR

$(PERSISTENT_INSTR): $(AFL_CLANG_FAST) \
		$(BASELINE_DIR)persistent_instr.c $(TARGET_DIR)target.c
	$(AFL_CLANG_FAST) \
		-o $@ \
		$(BASELINE_DIR)persistent_instr.c \
		$(TARGET_DIR)target.c

.targets: $(TARGETS)

## TEST CASE

$(TEST_CASE_DIR): | $(BIN_DIR)
	mkdir -p $@

$(FINDINGS_DIR): | $(BIN_DIR)
	mkdir -p $@

$(SAMPLE_TXT): | $(TEST_CASE_DIR)
	echo "AAA" > $@

## RUN

.run-target-fork: $(TARGET_FORK) $(SAMPLE_TXT) $(FINDINGS_DIR)
	$(AFL_FUZZ) $(AFL_FLAGS) -i $(TEST_CASE_DIR) -o $(FINDINGS_DIR) $<

.run-target-persistent: $(TARGET_PERSISTENT) $(SAMPLE_TXT) $(FINDINGS_DIR)
	$(AFL_FUZZ) $(AFL_FLAGS) -i $(TEST_CASE_DIR) -o $(FINDINGS_DIR) $<

.run-fork-instr: $(FORK_INSTR) $(SAMPLE_TXT) $(FINDINGS_DIR)
	$(AFL_FUZZ) $(AFL_FLAGS) -i $(TEST_CASE_DIR) -o $(FINDINGS_DIR) $<

.run-persistent-instr: $(PERSISTENT_INSTR) $(SAMPLE_TXT) $(FINDINGS_DIR)
	$(AFL_FUZZ) $(AFL_FLAGS) -i $(TEST_CASE_DIR) -o $(FINDINGS_DIR) $<

$(eval $(call os_arch_target,run-target-fork-,.run-target-fork))
$(eval $(call os_arch_target,run-target-persistent-,.run-target-persistent))
$(eval $(call os_arch_target,run-fork-instr-,.run-fork-instr))
$(eval $(call os_arch_target,run-persistent-instr-,.run-persistent-instr))