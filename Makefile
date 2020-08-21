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

SUPPORTED_linux_x86:=1
SUPPORTED_linux_x86_64:=1
SUPPORTED_macos_x86_64:=1
SUPPORTED_android_x86_64:=1

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

TARGET_COLOUR = "\033[32m";
RESET_COLOUR = "\033[0m"

# Platform support
ifneq ($(and $(ARCH),$(OS)),)
 ifeq ($(SUPPORTED_$(OS)_$(ARCH)),)
$(error Unsupported platform combination $(OS)-$(ARCH))
 endif
endif

ifeq ($(OS),linux)
	ifeq ($(ARCH),x86)
		PLAT_CFLAGS:= -m32
	endif
	ifeq ($(ARCH),x86_64)
		PLAT_CFLAGS:=
	endif
endif
ifeq ($(OS),macos)
	ifeq ($(ARCH),x86_64)
	endif
endif
ifeq ($(OS),android)
	ifeq ($(ARCH),x86_64)
	endif
endif

# Macros

# args: name, dependency
define os_arch_target
$(foreach arch,$(CANDIDATE_ARCH),$(eval $(call build_arch,$(arch),$(1),$(2))))
endef

# args: arch, name, dependency
define build_arch
$(foreach os,$(CANDIDATE_OS),$(eval $(call build_os,$(os),$(1),$(2),$(3))))
endef

# args: os, arch, name, dependency
define build_os
$(3)$(1)-$(2):
	OS=$(1) ARCH=$(2) make $(4)
endef

# args: name, notes
define show
.show-hdr-$(1):
	@printf "$(2)"
	@printf "\n"

	@printf $(TARGET_COLOUR)
	@printf "\t"
	@printf "$(1)"
	@printf $(RESET_COLOUR)
	@printf "\n"

.show-footer-$(1): .show-hdr-$(1)
	@printf "\n"

SHOW+=.show-footer-$(1)
endef

# args: name, notes
define os_arch_show
$(foreach arch,$(CANDIDATE_ARCH),$(eval $(call show_arch,$(arch),$(1))))
.os-arch-show-hdr-$(1):
	@printf  "$(2)"
	@printf "\n"

.os-arch-show-footer-$(1): $$(SHOW-$(1))
	@printf "\n"

SHOW+=.os-arch-show-footer-$(1)
endef

# args: arch, name
define show_arch
$(foreach os,$(CANDIDATE_OS),$(eval $(call show_os,$(os),$(1),$(2))))
endef

# args: os, arch, name
define show_os
.os-arch-show-$(3)$(1)-$(2): .os-arch-show-hdr-$(3)
ifneq ($(SUPPORTED_$(1)_$(2)),)
	@printf $(TARGET_COLOUR)
	@printf "\t"
 ifeq ($(3),)
	@printf "$(1)-$(2)"
 else
	@printf "$(3)-$(1)-$(2)"
 endif
	@printf $(RESET_COLOUR)
	@printf "\n"
endif

SHOW-$(3)+=.os-arch-show-$(3)$(1)-$(2)
endef

# Build help system

$(eval $(call os_arch_show,,Build all of the components for the given platform))
$(eval $(call show,clean,Delete all built artefacts))
$(eval $(call show,afl,Build the AFL examples for comparison))
$(eval $(call os_arch_show,devkit,Build the FRIDA GUM devkit))

$(eval $(call os_arch_show,target-fork,Build stalker AFL fork mode example))
$(eval $(call os_arch_show,target-persistent,Build stalker AFL persistent mode example))
$(eval $(call show,fork-instr,Build afl-clang stalker fork mode example))
$(eval $(call show,persistent-instr,Build afl-clang persistent mode example))

$(eval $(call os_arch_show,run-target-fork,Run stalker AFL fork mode example))
$(eval $(call os_arch_show,run-target-persistent,Run stalker AFL persistent mode example))
$(eval $(call show,run-fork-instr,Run afl-clang stalker fork mode example))
$(eval $(call show,run-persistent-instr,Run afl-clang persistent mode example))

# targets

default: $(SHOW)

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

.target-persistent: $(TARGET_PERSISTENT)

$(eval $(call os_arch_target,target-persistent-,.target-persistent))

## FORK_INSTR

$(FORK_INSTR): $(AFL_CLANG_FAST) \
		$(BASELINE_DIR)fork_instr.c $(TARGET_DIR)target.c
	$(AFL_CLANG_FAST) \
		-o $@ \
		$(BASELINE_DIR)fork_instr.c \
		$(TARGET_DIR)target.c

fork-instr: $(FORK_INSTR)

# PERSISTENT_INSTR

$(PERSISTENT_INSTR): $(AFL_CLANG_FAST) \
		$(BASELINE_DIR)persistent_instr.c $(TARGET_DIR)target.c
	$(AFL_CLANG_FAST) \
		-o $@ \
		$(BASELINE_DIR)persistent_instr.c \
		$(TARGET_DIR)target.c

persistent-inst: $(PERSISTENT_INSTR)

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

run-fork-instr: $(FORK_INSTR) $(SAMPLE_TXT) $(FINDINGS_DIR)
	$(AFL_FUZZ) $(AFL_FLAGS) -i $(TEST_CASE_DIR) -o $(FINDINGS_DIR) $<

run-persistent-instr: $(PERSISTENT_INSTR) $(SAMPLE_TXT) $(FINDINGS_DIR)
	$(AFL_FUZZ) $(AFL_FLAGS) -i $(TEST_CASE_DIR) -o $(FINDINGS_DIR) $<

$(eval $(call os_arch_target,run-target-fork-,.run-target-fork))
$(eval $(call os_arch_target,run-target-persistent-,.run-target-persistent))