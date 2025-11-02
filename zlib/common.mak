# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_ZLIB_LIB
CFLAGS_THIS = -fr=nul -i.. -dHAVE_CONFIG_H -dSTDC

!include "../vars.mak"

SUBDIR_DLL = $(SUBDIR)$(HPS)dll
SUBDIR_LIB = $(SUBDIR)$(HPS)lib

#--------------------
OBJS_DLL = $(SUBDIR_DLL)$(HPS)adler32.obj $(SUBDIR_DLL)$(HPS)compress.obj $(SUBDIR_DLL)$(HPS)crc32.obj $(SUBDIR_DLL)$(HPS)deflate.obj $(SUBDIR_DLL)$(HPS)infback.obj $(SUBDIR_DLL)$(HPS)inffast.obj $(SUBDIR_DLL)$(HPS)inflate.obj $(SUBDIR_DLL)$(HPS)inftrees.obj $(SUBDIR_DLL)$(HPS)trees.obj $(SUBDIR_DLL)$(HPS)uncompr.obj $(SUBDIR_DLL)$(HPS)zutil.obj $(SUBDIR_DLL)$(HPS)libmain.obj

EXT_ZLIB_EXAMPLE_EXE_DLL = $(SUBDIR_DLL)$(HPS)example.exe
EXT_ZLIB_LIB_DLL=$(SUBDIR_DLL)$(HPS)zlib.lib # to avoid conflicts with DLL import library
EXT_ZLIB_DLL=$(SUBDIR_DLL)$(HPS)ZLIB.DLL

#--------------------
OBJS_LIB = $(SUBDIR_LIB)$(HPS)adler32.obj $(SUBDIR_LIB)$(HPS)compress.obj $(SUBDIR_LIB)$(HPS)crc32.obj $(SUBDIR_LIB)$(HPS)deflate.obj $(SUBDIR_LIB)$(HPS)infback.obj $(SUBDIR_LIB)$(HPS)inffast.obj $(SUBDIR_LIB)$(HPS)inflate.obj $(SUBDIR_LIB)$(HPS)inftrees.obj $(SUBDIR_LIB)$(HPS)trees.obj $(SUBDIR_LIB)$(HPS)uncompr.obj $(SUBDIR_LIB)$(HPS)zutil.obj $(SUBDIR_LIB)$(HPS)libmain.obj

EXT_ZLIB_EXAMPLE_EXE_LIB = $(SUBDIR_LIB)$(HPS)example.exe
EXT_ZLIB_LIB_LIB=$(SUBDIR_LIB)$(HPS)zlib.lib # to avoid conflicts with DLL import library

#--------------------
$(EXT_ZLIB_LIB_LIB): $(OBJS_LIB)
	wlib -q -b -c $(EXT_ZLIB_LIB_LIB) -+$(SUBDIR_LIB)$(HPS)adler32.obj -+$(SUBDIR_LIB)$(HPS)compress.obj -+$(SUBDIR_LIB)$(HPS)crc32.obj -+$(SUBDIR_LIB)$(HPS)deflate.obj -+$(SUBDIR_LIB)$(HPS)infback.obj -+$(SUBDIR_LIB)$(HPS)inffast.obj -+$(SUBDIR_LIB)$(HPS)inflate.obj -+$(SUBDIR_LIB)$(HPS)inftrees.obj -+$(SUBDIR_LIB)$(HPS)trees.obj -+$(SUBDIR_LIB)$(HPS)uncompr.obj -+$(SUBDIR_LIB)$(HPS)zutil.obj

#--------------------
$(EXT_ZLIB_DLL) $(EXT_ZLIB_LIB_DLL): $(OBJS_DLL)
	%write tmp.cmd option quiet system $(WLINK_DLL_SYSTEM) file $(SUBDIR_DLL)$(HPS)adler32.obj file $(SUBDIR_DLL)$(HPS)compress.obj file $(SUBDIR_DLL)$(HPS)crc32.obj file $(SUBDIR_DLL)$(HPS)deflate.obj file $(SUBDIR_DLL)$(HPS)infback.obj file $(SUBDIR_DLL)$(HPS)inffast.obj file $(SUBDIR_DLL)$(HPS)inflate.obj file $(SUBDIR_DLL)$(HPS)inftrees.obj file $(SUBDIR_DLL)$(HPS)trees.obj file $(SUBDIR_DLL)$(HPS)uncompr.obj file $(SUBDIR_DLL)$(HPS)zutil.obj file $(SUBDIR_DLL)$(HPS)libmain.obj
	%write tmp.cmd option MODNAME=$(MODULENAME_BASE)_ZLIB
!ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA MOVEABLE
!endif
	%write tmp.cmd option impfile=$(SUBDIR_DLL)$(HPS)ZLIB.LCF
	%write tmp.cmd name $(EXT_ZLIB_DLL)
	@wlink @tmp.cmd
	perl lcflib.pl --nofilter $(SUBDIR_DLL)$(HPS)zlib.lib $(SUBDIR_DLL)$(HPS)ZLIB.LCF
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXT_ZLIB_DLL)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit

#----------------------------------------------------------
$(EXT_ZLIB_EXAMPLE_EXE_DLL): $(EXT_ZLIB_DLL) $(EXT_ZLIB_LIB_DLL) $(SUBDIR_DLL)$(HPS)example.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR_DLL)$(HPS)example.obj library $(EXT_ZLIB_LIB_DLL) name $(EXT_ZLIB_EXAMPLE_EXE_DLL)
!ifeq TARGET_MSDOS 16
	%write tmp.cmd option MODNAME=$(MODULENAME_BASE)_ZLIB_DLL_EXAMPLE
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!endif
	%write tmp.cmd option map=$(EXT_ZLIB_EXAMPLE_EXE_DLL).map
	%write tmp.cmd option protmode # Protected mode Windows only, this shit won't run properly in real-mode Windows for weird arcane reasons!
	@wlink @tmp.cmd
!ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(EXT_ZLIB_EXAMPLE_EXE_DLL)
	@wbind $(EXT_ZLIB_EXAMPLE_EXE_DLL) -q -n
!endif
	@$(COPY) ..$(HPS)dos32a.dat $(SUBDIR_DLL)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXT_ZLIB_EXAMPLE_EXE_DLL)
!endif
!ifeq TARGET_WINDOWS 20
	../tool/win2xhdrpatch.pl $(EXT_ZLIB_EXAMPLE_EXE_DLL)
	../tool/win2xstubpatch.pl $(EXT_ZLIB_EXAMPLE_EXE_DLL)
!endif

$(SUBDIR_DLL)$(HPS)example.obj: example.c
	%write tmp.cmd $(CFLAGS_THIS) -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

#----------------------------------------------------------
$(EXT_ZLIB_EXAMPLE_EXE_LIB): $(EXT_ZLIB_LIB) $(EXT_ZLIB_LIB_LIB) $(SUBDIR_LIB)$(HPS)example.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR_LIB)$(HPS)example.obj library $(EXT_ZLIB_LIB_LIB) name $(EXT_ZLIB_EXAMPLE_EXE_LIB)
!ifeq TARGET_MSDOS 16
	%write tmp.cmd option MODNAME=$(MODULENAME_BASE)_ZLIB_LIB_EXAMPLE
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
	%write tmp.cmd option protmode # Protected mode Windows only, this shit won't run properly in real-mode Windows for weird arcane reasons!
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
$(SUBDIR_DLL)$(HPS)adler32.obj: adler32.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)compress.obj: compress.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)crc32.obj: crc32.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)deflate.obj: deflate.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)infback.obj: infback.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)inffast.obj: inffast.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)inflate.obj: inflate.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)inftrees.obj: inftrees.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)trees.obj: trees.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)uncompr.obj: uncompr.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)zutil.obj: zutil.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR_DLL)$(HPS)libmain.obj: libmain.c
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL -fo=$(SUBDIR_DLL)$(HPS).obj $(CFLAGS) $[@
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

all: lib dll exe .symbolic

lib: $(EXT_ZLIB_LIB_LIB) $(EXT_ZLIB_LIB_DLL) $(EXT_ZLIB_DLL) .symbolic

dll: $(EXT_ZLIB_DLL) .symbolic

testexe: $(EXT_ZLIB_EXAMPLE_EXE_DLL) $(EXT_ZLIB_EXAMPLE_EXE_LIB) .symbolic

exe: testexe .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

