# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_ZLIB_LIB
CFLAGS_THIS = -fr=nul -i.. -dHAVE_CONFIG_H -dSTDC

!include "../vars.mak"

SUBDIR_LIB = $(SUBDIR)$(HPS)lib

#--------------------
OBJS_LIB = $(SUBDIR_LIB)$(HPS)adler32.obj $(SUBDIR_LIB)$(HPS)compress.obj $(SUBDIR_LIB)$(HPS)crc32.obj $(SUBDIR_LIB)$(HPS)deflate.obj $(SUBDIR_LIB)$(HPS)infback.obj $(SUBDIR_LIB)$(HPS)inffast.obj $(SUBDIR_LIB)$(HPS)inflate.obj $(SUBDIR_LIB)$(HPS)inftrees.obj $(SUBDIR_LIB)$(HPS)trees.obj $(SUBDIR_LIB)$(HPS)uncompr.obj $(SUBDIR_LIB)$(HPS)zutil.obj $(SUBDIR_LIB)$(HPS)libmain.obj

EXT_ZLIB_EXAMPLE_EXE_LIB = $(SUBDIR_LIB)$(HPS)example.exe
EXT_ZLIB_LIB_LIB=$(SUBDIR_LIB)$(HPS)zlib.lib

#--------------------
$(EXT_ZLIB_LIB_LIB): $(OBJS_LIB)
	wlib -q -b -c $(EXT_ZLIB_LIB_LIB) -+$(SUBDIR_LIB)$(HPS)adler32.obj -+$(SUBDIR_LIB)$(HPS)compress.obj -+$(SUBDIR_LIB)$(HPS)crc32.obj -+$(SUBDIR_LIB)$(HPS)deflate.obj -+$(SUBDIR_LIB)$(HPS)infback.obj -+$(SUBDIR_LIB)$(HPS)inffast.obj -+$(SUBDIR_LIB)$(HPS)inflate.obj -+$(SUBDIR_LIB)$(HPS)inftrees.obj -+$(SUBDIR_LIB)$(HPS)trees.obj -+$(SUBDIR_LIB)$(HPS)uncompr.obj -+$(SUBDIR_LIB)$(HPS)zutil.obj -+$(SUBDIR_LIB)$(HPS)libmain.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit

#----------------------------------------------------------
$(EXT_ZLIB_EXAMPLE_EXE_LIB): $(EXT_ZLIB_LIB) $(EXT_ZLIB_LIB_LIB) $(SUBDIR_LIB)$(HPS)example.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR_LIB)$(HPS)example.obj library $(EXT_ZLIB_LIB_LIB) name $(EXT_ZLIB_EXAMPLE_EXE_LIB)
!ifeq TARGET_MSDOS 16
	%write tmp.cmd option protmode # Protected mode Windows only, this shit won't run properly in real-mode Windows for weird arcane reasons!
	%write tmp.cmd option MODNAME=$(MODULENAME_BASE)_ZLIB_LIB_EXAMPLE
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!endif
	%write tmp.cmd option map=$(EXT_ZLIB_EXAMPLE_EXE_LIB).map
	@wlink @tmp.cmd
!ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(EXT_ZLIB_EXAMPLE_EXE_LIB)
	@wbind $(EXT_ZLIB_EXAMPLE_EXE_LIB) -q -n
!endif
	@$(COPY) ..$(HPS)dos32a.dat $(SUBDIR_LIB)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXT_ZLIB_EXAMPLE_EXE_LIB)
!endif
!ifeq TARGET_WINDOWS 20
	../tool/win2xhdrpatch.pl $(EXT_ZLIB_EXAMPLE_EXE_LIB)
	../tool/win2xstubpatch.pl $(EXT_ZLIB_EXAMPLE_EXE_LIB)
!endif

$(SUBDIR_LIB)$(HPS)example.obj: example.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

#----------------------------------------------------------
$(SUBDIR_LIB)$(HPS)adler32.obj: adler32.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)compress.obj: compress.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)crc32.obj: crc32.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)deflate.obj: deflate.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)infback.obj: infback.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)inffast.obj: inffast.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)inflate.obj: inflate.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)inftrees.obj: inftrees.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)trees.obj: trees.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)uncompr.obj: uncompr.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)zutil.obj: zutil.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_LIB)$(HPS)libmain.obj: libmain.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_LIB)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic

lib: $(EXT_ZLIB_LIB_LIB) .symbolic

testexe: $(EXT_ZLIB_EXAMPLE_EXE_LIB) .symbolic

exe: testexe .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

