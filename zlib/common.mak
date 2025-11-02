# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_ZLIB_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -dHAVE_CONFIG_H -dSTDC

!include "../vars.mak"

EXT_ZLIB_LIB_STATIC=$(SUBDIR)$(HPS)zlib.s.lib # to avoid conflicts with DLL import library
EXT_ZLIB_LIB_DLL=$(SUBDIR)$(HPS)zlib.lib # to avoid conflicts with DLL import library
EXT_ZLIB_DLL=$(SUBDIR)$(HPS)ZLIB.DLL
EXT_ZLIB_EXAMPLE_EXE = $(SUBDIR)$(HPS)example.exe

OBJS = $(SUBDIR)$(HPS)adler32.obj $(SUBDIR)$(HPS)compress.obj $(SUBDIR)$(HPS)crc32.obj $(SUBDIR)$(HPS)deflate.obj $(SUBDIR)$(HPS)infback.obj $(SUBDIR)$(HPS)inffast.obj $(SUBDIR)$(HPS)inflate.obj $(SUBDIR)$(HPS)inftrees.obj $(SUBDIR)$(HPS)trees.obj $(SUBDIR)$(HPS)uncompr.obj $(SUBDIR)$(HPS)zutil.obj $(SUBDIR)$(HPS)libmain.obj

!ifndef NO_TEST_EXE
! ifdef WIN386
EXT_ZLIB_LIB_CHOICE=$(EXT_ZLIB_LIB_STATIC)
! else
EXT_ZLIB_LIB_CHOICE=$(EXT_ZLIB_LIB_DLL)
! endif

$(EXT_ZLIB_EXAMPLE_EXE): $(EXT_ZLIB_LIB_CHOICE) $(SUBDIR)$(HPS)example.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)example.obj library $(EXT_ZLIB_LIB_CHOICE) name $(EXT_ZLIB_EXAMPLE_EXE)
! ifeq TARGET_MSDOS 16
	%write tmp.cmd option MODNAME=$(MODULENAME_BASE)_ZLIB_EXAMPLE
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
! endif
	%write tmp.cmd option map=$(EXT_ZLIB_EXAMPLE_EXE).map
	@wlink @tmp.cmd
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(EXT_ZLIB_EXAMPLE_EXE)
	@wbind $(EXT_ZLIB_EXAMPLE_EXE) -q -n
! endif
	@$(COPY) ..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXT_ZLIB_EXAMPLE_EXE)
! endif
! ifeq TARGET_WINDOWS 20
	../tool/win2xhdrpatch.pl $(EXT_ZLIB_EXAMPLE_EXE)
	../tool/win2xstubpatch.pl $(EXT_ZLIB_EXAMPLE_EXE)
! endif
!endif

!ifndef EXT_ZLIB_LIB_NO_LIB
$(EXT_ZLIB_LIB_STATIC): $(OBJS)
	wlib -q -b -c $(EXT_ZLIB_LIB_STATIC) -+$(SUBDIR)$(HPS)adler32.obj -+$(SUBDIR)$(HPS)compress.obj -+$(SUBDIR)$(HPS)crc32.obj -+$(SUBDIR)$(HPS)deflate.obj -+$(SUBDIR)$(HPS)infback.obj -+$(SUBDIR)$(HPS)inffast.obj -+$(SUBDIR)$(HPS)inflate.obj -+$(SUBDIR)$(HPS)inftrees.obj -+$(SUBDIR)$(HPS)trees.obj -+$(SUBDIR)$(HPS)uncompr.obj -+$(SUBDIR)$(HPS)zutil.obj

! ifndef WIN386
$(EXT_ZLIB_DLL) $(EXT_ZLIB_LIB_DLL): $(OBJS)
	%write tmp.cmd option quiet system $(WLINK_DLL_SYSTEM) file $(SUBDIR)$(HPS)adler32.obj file $(SUBDIR)$(HPS)compress.obj file $(SUBDIR)$(HPS)crc32.obj file $(SUBDIR)$(HPS)deflate.obj file $(SUBDIR)$(HPS)infback.obj file $(SUBDIR)$(HPS)inffast.obj file $(SUBDIR)$(HPS)inflate.obj file $(SUBDIR)$(HPS)inftrees.obj file $(SUBDIR)$(HPS)trees.obj file $(SUBDIR)$(HPS)uncompr.obj file $(SUBDIR)$(HPS)zutil.obj file $(SUBDIR)$(HPS)libmain.obj
	%write tmp.cmd option MODNAME=$(MODULENAME_BASE)_ZLIB
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA MOVEABLE
!  endif
	%write tmp.cmd option impfile=$(SUBDIR)$(HPS)ZLIB.LCF
	%write tmp.cmd name $(EXT_ZLIB_DLL)
	@wlink @tmp.cmd
	perl lcflib.pl --nofilter $(SUBDIR)$(HPS)zlib.lib $(SUBDIR)$(HPS)ZLIB.LCF
!  ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXT_ZLIB_DLL)
!  endif
! endif
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) -dZLIB_DLL $(CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR)$(HPS)example.obj: example.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib dll exe .symbolic

!ifdef WIN386
lib: $(EXT_ZLIB_LIB_STATIC) .symbolic
!else
lib: $(EXT_ZLIB_LIB_STATIC) $(EXT_ZLIB_LIB_DLL) .symbolic
!endif

!ifdef WIN386
dll: .symbolic
!else
dll: $(EXT_ZLIB_DLL) .symbolic
!endif

!ifndef NO_TEST_EXE
testexe: $(EXT_ZLIB_EXAMPLE_EXE) .symbolic
!else
testexe: .symbolic
!endif

exe: testexe .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

