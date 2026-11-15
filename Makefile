# wb900: AROS libraries for Exec900 on the Commodore 900 (Z8001).
#
# A library is libs/NAME/*.c plus libs/NAME/NAME_lib.fd, built as
# Exec900's docs/platform.md "Building a disk library" says: genlvo
# writes the callers' header and the vector table, the objects are linked
# with `ld -r' and progabs.o, and mkhunk makes the hunk file that goes in
# LIBS:.  tests/NAME.c is a root program that boots from a disk holding
# the libraries and passes with a "PASS NAME" line.
#
# Exec900 is used through its MIT interface files (include/, fd/,
# genlvo's output, lib/, platform/progabs.s and portio.s) and its build,
# which provides exec900.sys and the dos.library and handler hunks:
#   make -C $(EXEC900) OUT=$(EXEC900_OUT) ... all

SHELL = /bin/sh
.DELETE_ON_ERROR:
.SECONDARY:

TOP	:= $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
GITDIR	:= $(abspath $(TOP)/..)
OUT	?= $(TOP)/build/wb900
OBJ	= $(OUT)/obj

C900_TOOLCHAIN	?= $(GITDIR)/commodore-900-toolchain
EXEC900		?= $(TOP)/build/exec900-src/c900
EXEC900_OUT	?= $(TOP)/build/exec900
KBOOTSRC	?= $(GITDIR)/commodore-900-kboot
KBOOT		?= $(KBOOTSRC)/build/kboot
C900_EMU	?= $(GITDIR)/commodore-900-emulator

TC	= $(C900_TOOLCHAIN)/host/build
CC0	= $(TC)/z8001/cc0-z8001
CC1	= $(TC)/z8001/cc1-z8001
CC2	= $(TC)/z8001/cc2-z8001
AS	= $(TC)/as-z8001 -N
LD	= $(TC)/ld-z8001 -N

GENLVO	= python3 $(EXEC900)/tools/genlvo.py
MKHUNK	= python3 $(EXEC900)/tools/mkhunk.py --lname --ld=$(TC)/ld-z8001
MKDISK	= python3 $(EXEC900)/tools/mkdisk.py
SYMCHECK = python3 $(EXEC900)/tools/symcheck.py --lname
RUNTEST	= sh $(EXEC900)/tools/runtest.sh $(C900_EMU) $(BUDGET)

# Emulator budget per test, in instructions.
BUDGET	?= 400000000

# Exec900's cc0 variants plus -VSVARG: an int in a `...' slot warns.
VAR	= 800000020820

# Exec900's generated <exec/abi/*.h> are in $(EXEC900_OUT)/include.
INC	= -I$(TOP)/include -I$(OUT)/include -I$(EXEC900)/include -I$(EXEC900_OUT)/include
HDRS	= $(wildcard $(TOP)/include/*/*.h) $(wildcard $(EXEC900)/include/*/*.h) \
	  $(wildcard $(EXEC900_OUT)/include/*/*.h) \
	  $(wildcard $(EXEC900_OUT)/include/*/*/*.h)

# cc0 -> cc1 -> cc2, as Exec900 compiles; 0012 relocates frame
# references against SS, and the final 10 (VLNAME) keeps external names
# of up to 127 characters, which as, ld, mkhunk and symcheck take with -N
# or --lname.
define CCRULE
	@mkdir -p $(dir $(3))
	$(CC0) $(VAR) $(1) $(OBJ)/$(2).z0 $(INC)
	$(CC1) $(VAR) $(OBJ)/$(2).z0 $(OBJ)/$(2).z1
	$(CC2) 001200000010 $(OBJ)/$(2).z1 $(3) $(OBJ)/$(2).scr 0
endef

LIBS	= utility keymap graphics layers intuition gadtools icon workbench commodities
# graphics.library's display drivers (LIBS:NAME.gfxdriver)
GFXDRIVERS = hr
DEVS	= input console
# The device names that differ from their directory's
console_DEVNAME = wincon
TESTS	= $(basename $(notdir $(wildcard $(TOP)/tests/*.c)))
# The tests that open intuition.library
ITESTS	= intuition wbdemo wbinter gadtools gtdemo icon workbench wbboot console cxtools conhandler shellwin
# The demos also made as NAME-1x.bin, in the Workbench 1.x style
STYLED	= wbdemo wbinter gtdemo wbboot


.PHONY: all test clean
all: $(patsubst %,$(OUT)/libs/%.gfxdriver,$(GFXDRIVERS)) $(patsubst %,$(OUT)/libs/%.library,$(LIBS)) $(patsubst %,$(OUT)/devs/%.device,$(DEVS)) $(patsubst %,$(OUT)/%.bin,$(TESTS)) $(patsubst %,$(OUT)/%-1x.bin,$(STYLED))

clean:
	rm -rf $(OUT)

# Exec900's program objects.
$(OBJ)/progabs.o: $(EXEC900)/platform/progabs.s
	@mkdir -p $(OBJ)
	$(AS) -g -o $@ $<
$(OBJ)/portio.o: $(EXEC900)/platform/portio.s
	@mkdir -p $(OBJ)
	$(AS) -g -o $@ $<
$(OBJ)/startup.o: $(EXEC900)/lib/startup.s
	@mkdir -p $(OBJ)
	$(AS) -g -o $@ $<
$(OBJ)/amiga.o: $(EXEC900)/lib/amiga.c $(HDRS)
	$(call CCRULE,$<,amiga,$@)

# AROS amiga.lib's calls that intuition and its callers link.
ALIB	= $(patsubst $(TOP)/lib/%.c,$(OBJ)/lib/%.o,$(wildcard $(TOP)/lib/*.c))
$(OBJ)/lib/%.o: $(TOP)/lib/%.c $(HDRS)
	$(call CCRULE,$<,lib/$*,$@)

# Libraries and devices: the callers' header and the vector table
# NAMEFuncs from the fd file, the objects of libs/NAME (devs/NAME),
# amiga.lib's NewList, and the hunk file for LIBS: (DEVS:), in code hunks
# of at most 64 KB, with any NAME.s files of the module assembled beside
# them.  NAME_LINK names more objects, NAME_SRC their sources.
# $(call MODULE,name,Base,prefix,libs|devs,library|device|gfxdriver)
define MODULE
$(OUT)/include/lvo/$(1)_lib.h $(OBJ)/$(1)/vec.c: $(TOP)/$(4)/$(1)/$(1)_lib.fd $(EXEC900)/tools/genlvo.py
	@mkdir -p $(OBJ)/$(1)
	$(GENLVO) $$< $(OUT)/include/lvo/$(1)_lib.h --table $(OBJ)/$(1)/vec.c $(2)Funcs $(3)

$(1)_OBJS = $(OBJ)/$(1)/vec.o $(patsubst $(TOP)/$(4)/$(1)/%.c,$(OBJ)/$(1)/%.o,$(wildcard $(TOP)/$(4)/$(1)/*.c)) \
	$(patsubst $(TOP)/$(4)/$(1)/%.s,$(OBJ)/$(1)/%.o,$(wildcard $(TOP)/$(4)/$(1)/*.s))

$(OBJ)/$(1)/%.o: $(TOP)/$(4)/$(1)/%.c $(wildcard $(TOP)/$(4)/$(1)/*.h) $(HDRS) $(OUT)/include/lvo/$(1)_lib.h
	$$(call CCRULE,$$<,$(1)/$$*,$$@)
$(OBJ)/$(1)/%.o: $(TOP)/$(4)/$(1)/%.s
	@mkdir -p $(OBJ)/$(1)
	$(AS) -g -o $$@ $$<
$(OBJ)/$(1)/vec.o: $(OBJ)/$(1)/vec.c
	$$(call CCRULE,$$<,$(1)/vec,$$@)

$(OUT)/$(4)/$(1).$(5): $$($(1)_OBJS) $$($(1)_LINK) $(OBJ)/amiga.o $(OBJ)/progabs.o
	$(SYMCHECK) $(TOP)/$(4)/$(1) $(TOP)/include $$($(1)_SRC)
	@mkdir -p $(OUT)/$(4)
	$(MKHUNK) --split $$@ $$^
endef
LIBRARY = $(call MODULE,$(1),$(2),$(3),libs,library)
DEVICE = $(call MODULE,$(1),$(2),$(3),devs,device)

$(eval $(call LIBRARY,utility,Utility,ut))
$(eval $(call LIBRARY,keymap,Keymap,km))
$(eval $(call LIBRARY,graphics,Graphics,gfx))
$(eval $(call LIBRARY,layers,Layers,la))
# graphics calls layers.library through the base layers leaves in GfxBase.
$(graphics_OBJS): $(OUT)/include/lvo/layers_lib.h
$(eval $(call DEVICE,input,Input,id))
console_LINK = $(ALIB)
console_SRC = $(TOP)/lib
$(eval $(call DEVICE,console,Console,cd))
$(console_OBJS): $(patsubst %,$(OUT)/include/lvo/%_lib.h,utility graphics intuition keymap)
# The HR driver writes the card's control port.
hr_LINK = $(OBJ)/portio.o
$(eval $(call MODULE,hr,HR,hr,libs,gfxdriver))
intuition_LINK = $(ALIB)
intuition_SRC = $(TOP)/lib
$(eval $(call LIBRARY,intuition,Intuition,it))
# intuition calls the libraries and devices it opens through their bases.
$(intuition_OBJS): $(patsubst %,$(OUT)/include/lvo/%_lib.h,utility graphics layers keymap input)
gadtools_LINK = $(ALIB)
gadtools_SRC = $(TOP)/lib
$(eval $(call LIBRARY,gadtools,GadTools,gt))
$(gadtools_OBJS): $(patsubst %,$(OUT)/include/lvo/%_lib.h,utility graphics layers intuition)
$(eval $(call LIBRARY,icon,Icon,ic))
$(icon_OBJS): $(patsubst %,$(OUT)/include/lvo/%_lib.h,utility graphics intuition workbench)
$(eval $(call LIBRARY,workbench,Workbench,wb))
$(workbench_OBJS): $(patsubst %,$(OUT)/include/lvo/%_lib.h,utility intuition icon)
$(eval $(call LIBRARY,commodities,Commodities,cx))
$(commodities_OBJS): $(patsubst %,$(OUT)/include/lvo/%_lib.h,utility keymap)

# CON: and RAW: (fs/con), a boot module: lib/startup.s (its process
# entry), the handler and amiga.lib's calls.
CONH_OBJS = $(patsubst $(TOP)/fs/con/%.c,$(OBJ)/fs_con/%.o,$(wildcard $(TOP)/fs/con/*.c))
$(OBJ)/fs_con/%.o: $(TOP)/fs/con/%.c $(wildcard $(TOP)/fs/con/*.h) $(HDRS) $(patsubst %,$(OUT)/include/lvo/%_lib.h,utility intuition gadtools input console)
	$(call CCRULE,$<,fs_con/$*,$@)
$(OUT)/l/con-handler: $(OBJ)/startup.o $(CONH_OBJS) $(OBJ)/amiga.o $(OBJ)/progabs.o
	$(SYMCHECK) $(TOP)/fs/con
	@mkdir -p $(OUT)/l
	$(LD) -r -o $(OBJ)/fs_con.r $^
	$(MKHUNK) --entry=start $@ $(OBJ)/fs_con.r

# Test programs: lib/startup.s first, so that start opens the code hunk,
# then the test, amiga.lib's KPrintf, port I/O and progabs.o.
$(OBJ)/t_%.o: $(TOP)/tests/%.c $(HDRS) $(patsubst %,$(OUT)/include/lvo/%_lib.h,$(LIBS) $(DEVS))
	$(call CCRULE,$<,t_$*,$@)

$(OUT)/progs/%: $(OBJ)/startup.o $(OBJ)/t_%.o $(OBJ)/amiga.o $(OBJ)/portio.o $(OBJ)/progabs.o
	$(SYMCHECK) $(TOP)/tests/$*.c $(if $(filter $(ALIBTESTS),$*),$(TOP)/lib) $(if $(filter wbboot shellwin,$*),$(WBSRC))
	@mkdir -p $(OUT)/progs
	$(LD) -r -o $(OBJ)/p_$*.r $^
	$(MKHUNK) --entry=start $@ $(OBJ)/p_$*.r
# The tests that link amiga.lib
ALIBTESTS = intuition gadtools gtdemo console conhandler shellwin
$(patsubst %,$(OUT)/progs/%,$(ALIBTESTS)): $(ALIB)
# amiga.lib's commodities calls
CXLIB	= $(patsubst $(TOP)/lib/%.c,$(OBJ)/lib/%.o,$(wildcard $(TOP)/lib/cx/*.c))
$(OUT)/progs/commodities: $(OBJ)/lib/cx/hotkey.o

# Programs for Workbench's disk (wb/), linked as the tests are.
$(OBJ)/w_%.o: $(TOP)/wb/%.c $(HDRS) $(patsubst %,$(OUT)/include/lvo/%_lib.h,$(LIBS) $(DEVS))
	$(call CCRULE,$<,w_$*,$@)
$(OUT)/wb/%: $(OBJ)/startup.o $(OBJ)/w_%.o $(OBJ)/amiga.o $(OBJ)/portio.o $(OBJ)/progabs.o
	$(SYMCHECK) $(TOP)/wb/$*.c
	@mkdir -p $(OUT)/wb
	$(LD) -r -o $(OBJ)/q_$*.r $^
	$(MKHUNK) --entry=start $@ $(OBJ)/q_$*.r
# The commodities (wb/commodities), with amiga.lib's commodities calls
CXTOOLS	= ClickToFront AutoPoint DepthMenu Blanker NoCapsLock
$(OBJ)/c_%.o: $(TOP)/wb/commodities/%.c $(HDRS) $(patsubst %,$(OUT)/include/lvo/%_lib.h,$(LIBS) $(DEVS))
	$(call CCRULE,$<,c_$*,$@)
$(OUT)/cx/%: $(OBJ)/startup.o $(OBJ)/c_%.o $(CXLIB) $(OBJ)/amiga.o $(OBJ)/portio.o $(OBJ)/progabs.o
	$(SYMCHECK) $(TOP)/wb/commodities/$*.c $(TOP)/lib/cx
	@mkdir -p $(OUT)/cx
	$(LD) -r -o $(OBJ)/c_$*.r $^
	$(MKHUNK) --entry=start $@ $(OBJ)/c_$*.r
# Workbench, which tests/wbboot.c starts
WBSRC	= $(patsubst %,$(TOP)/wb/%.c,workbench drawer selftest)
$(OUT)/progs/wbboot $(OUT)/progs/shellwin: $(patsubst $(TOP)/wb/%.c,$(OBJ)/w_%.o,$(WBSRC))
$(patsubst $(TOP)/wb/%.c,$(OBJ)/w_%.o,$(WBSRC)): $(TOP)/wb/workbench.h

# The icons of Workbench's disk: $(OUT)/wb/NAME.info, from mkinfo.py
# with NAME_INFO's options
MKINFO	= python3 $(TOP)/tools/mkinfo.py
Hello_INFO = --type=tool --art=$(TOP)/wb/hello.txt --stack=2048
Disk_INFO = --type=disk --art=builtin:disk --window=60,40,420,180
Demos_INFO = --type=drawer --art=builtin:drawer --select=builtin:drawer_open \
	--window=140,110,400,140
Trashcan_INFO = --type=trashcan --art=builtin:trash --select=builtin:trash_open
Notes_INFO = --type=project --art=builtin:project --tool=SYS:Demos/Hello
Tools_INFO = --type=drawer --art=builtin:drawer --select=builtin:drawer_open \
	--window=180,150,300,100
Commodities_INFO = --type=drawer --art=builtin:drawer \
	--select=builtin:drawer_open --window=120,200,520,110
CX_INFO	= --type=tool --art=builtin:tool --stack=2048 --tooltype=CX_PRIORITY=0
ClickToFront_INFO = $(CX_INFO) --tooltype=QUALIFIER=NONE --tooltype=NUMCLICKS=2
AutoPoint_INFO = $(CX_INFO)
DepthMenu_INFO = $(CX_INFO)
NoCapsLock_INFO = $(CX_INFO)
Blanker_INFO = $(CX_INFO) --tooltype=SECONDS=120 "--tooltype=BLANKKEY=ctrl alt b"
# tests/cxtools.c's, which blanks after a second
BlankerTest_INFO = $(CX_INFO) --tooltype=SECONDS=1 "--tooltype=BLANKKEY=ctrl alt b"
$(OUT)/wb/%.info: $(TOP)/tools/mkinfo.py $(TOP)/wb/hello.txt $(TOP)/libs/icon/support_builtin.c
	@mkdir -p $(OUT)/wb
	$(MKINFO) $@ $($*_INFO)

# Boot volumes: Exec900, dos.library and the V7 handler (LIBS:), the
# display modules and gameport.dev every Exec900 image carries, the
# libraries, display drivers and devices.
# The exec partition holds 512 KB, so only the tests that open
# intuition.library carry it.
DISKLIBS = $(if $(filter $(ITESTS),$(1)),$(LIBS),$(filter-out intuition gadtools icon workbench,$(LIBS)))
DISK	= --module=$(EXEC900_OUT)/hunks/dos.library \
	  --module=$(EXEC900_OUT)/hunks/v7fs-handler \
	  $(if $(filter $(ITESTS),$(1)),--module=$(OUT)/l/con-handler) \
	  $(patsubst %,--file=$(EXEC900_OUT)/hunks/%.display,lr hr lc) \
	  --file=$(EXEC900_OUT)/hunks/gameport.dev \
	  $(foreach l,$(call DISKLIBS,$(1)),--put=Libs/$(l).library=$(OUT)/libs/$(l).library) \
	  $(foreach g,$(GFXDRIVERS),--put=Libs/$(g).gfxdriver=$(OUT)/libs/$(g).gfxdriver) \
	  $(foreach d,$(DEVS),--put=Devs/$(or $($(d)_DEVNAME),$(d)).device=$(OUT)/devs/$(d).device) \
	  $($(1)_PUT)

# A test's more files on its disk: NAME_PUT, mkdisk --put options
workbench_PUT = --put=Demos/Hello=$(OUT)/wb/hello --put=Demos/Hello.info=$(OUT)/wb/Hello.info
$(OUT)/workbench.bin: $(OUT)/wb/hello $(OUT)/wb/Hello.info
# Workbench's disk: a few drawers, a tool and a project with icons
WBDISK	= Disk.info Demos.info Demos/Hello.info Demos/Notes.info Trashcan.info \
	Tools.info Tools/Commodities.info $(patsubst %,Tools/Commodities/%.info,$(CXTOOLS))
CXPUT	= $(foreach t,$(CXTOOLS),--put=Tools/Commodities/$(t)=$(OUT)/cx/$(t))
wbboot_PUT = $(workbench_PUT) --put=Demos/Notes=$(TOP)/wb/Notes \
	--put=Demos/ReadMe=$(TOP)/wb/ReadMe --mkdir=Trashcan $(CXPUT) \
	$(foreach i,$(filter-out Demos/Hello.info,$(WBDISK)),--put=$(i)=$(OUT)/wb/$(notdir $(i)))
$(OUT)/wbboot.bin $(OUT)/wbboot-1x.bin: $(OUT)/wb/hello $(patsubst %,$(OUT)/wb/%,$(notdir $(WBDISK)))
$(OUT)/wbboot.bin $(OUT)/wbboot-1x.bin $(OUT)/shellwin.bin: $(patsubst %,$(OUT)/cx/%,$(CXTOOLS))
# The commodities' test: the tools, with a quick Blanker
cxtools_PUT = $(CXPUT) --put=Tools/Commodities/Blanker.info=$(OUT)/wb/BlankerTest.info \
	$(foreach t,$(filter-out Blanker,$(CXTOOLS)),--put=Tools/Commodities/$(t).info=$(OUT)/wb/$(t).info)
$(OUT)/cxtools.bin: $(patsubst %,$(OUT)/cx/%,$(CXTOOLS)) $(patsubst %,$(OUT)/wb/%.info,BlankerTest $(CXTOOLS))
# Workbench's disk with the Shell
shellwin_PUT = $(wbboot_PUT) --put=L/Shell-Seg=$(EXEC900_OUT)/hunks/Shell-Seg
$(OUT)/shellwin.bin: $(OUT)/wb/hello $(patsubst %,$(OUT)/wb/%,$(notdir $(WBDISK)))

BINDEPS	= $(patsubst %,$(OUT)/libs/%.library,$(LIBS)) $(patsubst %,$(OUT)/libs/%.gfxdriver,$(GFXDRIVERS)) $(patsubst %,$(OUT)/devs/%.device,$(DEVS)) $(OUT)/l/con-handler $(EXEC900_OUT)/exec900.sys
$(OUT)/%.bin: $(OUT)/progs/% $(BINDEPS)
	$(MKDISK) --kboot=$(KBOOT) --root=$(OUT)/progs/$* $(call DISK,$*) $@ $(EXEC900_OUT)/exec900.sys

# The Workbench 1.x style: DEVS:system-configuration holds the 1.0
# colours and OLD_LOOK, which intuition reads when it is first opened.
$(OUT)/devs/system-configuration: $(TOP)/tools/sysconfig1x.py
	@mkdir -p $(OUT)/devs
	python3 $< $@
$(OUT)/%-1x.bin: $(OUT)/progs/% $(BINDEPS) $(OUT)/devs/system-configuration
	$(MKDISK) --kboot=$(KBOOT) --root=$(OUT)/progs/$* $(call DISK,$*) \
	  --put=Devs/system-configuration=$(OUT)/devs/system-configuration $@ $(EXEC900_OUT)/exec900.sys

# make test [T=name]: boot each test (or one) in the emulator.
test: $(patsubst %,$(OUT)/%.bin,$(or $(T),$(TESTS)))
	@fail=0; for t in $(or $(T),$(TESTS)); do \
		$(RUNTEST) $$t $(OUT)/$$t.bin $(OUT)/$$t.log $(TOP)/tests/$$t.c || fail=1; \
	done; [ $$fail = 0 ]
