#!/bin/bash
rel=..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

# example.exe is compiling into a real-mode EXE that Windows can't seem to track the DS
# segment properly. I'm sorry but it's hard to write code for an environment that randomly
# depending on the compiled EXE, gives you a DS value that's 20-50 real-mode paragraphs
# off from the real segment base. God damn. No wonder Microsoft was eager to drop real-mode
# Windows. Real-mode Windows isn't even the target platform for this project, we're focused
# on protected mode Windows 3.1/95 here. This isn't an academic DOSLIB exercise in writing
# code for old Windows in general, this is more specific and I don't have time for bullshit.
win30=1 # Windows 3.0
win31=1 # Windows 3.1
winnt=1 # Windows NT
win32=1 # Windows 9x/NT/XP/Vista/etc.
win32s=1 # Windows 3.1 + Win32s
#win386=1 # Windows 3.0 + Watcom win386
#win38631=1 # Windows 3.1 + Watcom win386

if [ "$1" == "clean" ]; then
	do_clean
	rm -Rfv linux-host
	rm -fv test.dsk test2.dsk nul.err tmp.cmd tmp1.cmd tmp2.cmd
	exit 0
fi

if [ "$1" == "disk" ]; then
	make_msdos_data_disk test.dsk || exit 1
	mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
#	allow_build_list="dos386f dos486f dos586f dos686f"
	make_buildlist
	begin_bat

	what=all
	if [ x"$2" != x ]; then what="$2"; fi
    if [ x"$3" != x ]; then build_list="$3"; fi

	for name in $build_list; do
		mkdir -p "$name/lib" "$name/dll" || exit 1
		do_wmake $name "$what" || exit 1
		bat_wmake $name "$what" || exit 1
	done

	end_bat
fi

