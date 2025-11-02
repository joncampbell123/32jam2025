
!ifeq HPS /
MAKECMD=env "parent_build_list=$(TO_BUILD)" ./make.sh
COPY=cp
RM=rm
!else
MAKECMD=make.bat
COPY=copy
RM=del
!endif

# we need to know where "HERE" is
HERE = $+$(%cwd)$-

# FMT\OMF\OMFSEGDG utility
!ifeq HPS /
OMFSEGDG=$(REL)$(HPS)fmt$(HPS)omf$(HPS)linux-host$(HPS)omfsegdg
!else
OMFSEGDG=$(REL)$(HPS)fmt$(HPS)omf$(HPS)dos386f$(HPS)omfsegdg
!endif

# CLSG module flags.
# must be given last.
# must specify small memory model, no library dependencies, no stack checking, no stack frames, DS/FS/GS float, inline floating point.
# you should use another version of this CFLAGS_CLSG if you're compiling related code for your memory model, so long as your entry
# points use the small memory model. you must specify the -r switch so that small/medium model code can safely call your routines.
CFLAGS_CLSG = -ms -zl -zq -s -bt=dos -oilrtm -wx -q -zu -zdf -zff -zgf -zc -fpi87 -r

