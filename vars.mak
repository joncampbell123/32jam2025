
# In the Win16 world, modules are known by their module names once loadad.
# To avoid DLL hell in Windows 3.1, make sure this base module name is unique
# to your game so that this game's DLLs never conflict in memory with another
# game's DLLs. In 32-bit Windows, conflicting DLLs in memory are never an issue.
# 
# If you're in a hurry or lazy, use uuidgen
#
# Perhaps in the future, some game dev tools will just patch over this string
# in the EXE without recompiling, so it helps to keep the string long.
#
# 2025/11/06: Oooooh, keep the base name short. Windows 3.1 Process Status
#             (win31ps.exe) cannot handle long module names and will CRASH
#             when it attempts to display them!
#
#             If you don't give a crap about Windows 3.1 Process Status, go
#             ahead and use long module names.
MODULENAME_BASE = game251101

