# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_ZLIB_LIB
CFLAGS_THIS = -fr=nul -i.. -dHAVE_CONFIG_H -dSTDC -fo=$(SUBDIR)$(HPS).obj

!include "../vars.mak"

#--------------------
OBJS_LIB = $(SUBDIR)$(HPS)adler32.obj $(SUBDIR)$(HPS)compress.obj $(SUBDIR)$(HPS)crc32.obj $(SUBDIR)$(HPS)deflate.obj $(SUBDIR)$(HPS)infback.obj $(SUBDIR)$(HPS)inffast.obj $(SUBDIR)$(HPS)inflate.obj $(SUBDIR)$(HPS)inftrees.obj $(SUBDIR)$(HPS)trees.obj $(SUBDIR)$(HPS)uncompr.obj $(SUBDIR)$(HPS)zutil.obj

EXT_ZLIB_EXAMPLE_EXE_LIB = $(SUBDIR)$(HPS)example.exe
EXT_ZLIB_LIB_LIB=$(SUBDIR)$(HPS)zlibifo.lib

#--------------------
$(EXT_ZLIB_LIB_LIB): $(OBJS_LIB)
	wlib -q -b -c $(EXT_ZLIB_LIB_LIB) -+$(SUBDIR)$(HPS)adler32.obj -+$(SUBDIR)$(HPS)compress.obj -+$(SUBDIR)$(HPS)crc32.obj -+$(SUBDIR)$(HPS)deflate.obj -+$(SUBDIR)$(HPS)infback.obj -+$(SUBDIR)$(HPS)inffast.obj -+$(SUBDIR)$(HPS)inflate.obj -+$(SUBDIR)$(HPS)inftrees.obj -+$(SUBDIR)$(HPS)trees.obj -+$(SUBDIR)$(HPS)uncompr.obj -+$(SUBDIR)$(HPS)zutil.obj

#----------------------------------------------------------
$(EXT_ZLIB_EXAMPLE_EXE_LIB): $(EXT_ZLIB_LIB) $(EXT_ZLIB_LIB_LIB) $(SUBDIR)$(HPS)example.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)example.obj library $(EXT_ZLIB_LIB_LIB) name $(EXT_ZLIB_EXAMPLE_EXE_LIB)
!ifeq TARGET_MSDOS 16
	%write tmp.cmd option protmode # Protected mode Windows only, this shit won't run properly in real-mode Windows for weird arcane reasons!
	%write tmp.cmd option MODNAME=$(MODULENAME_BASE)_ZLIBIFO_LIB_EXAMPLE
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
	@$(COPY) ..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXT_ZLIB_EXAMPLE_EXE_LIB)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic

lib: $(EXT_ZLIB_LIB_LIB) .symbolic

testexe: $(EXT_ZLIB_EXAMPLE_EXE_LIB) .symbolic

exe: testexe .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

