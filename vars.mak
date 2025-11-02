
# In the Win16 world, modules are known by their module names once loadad.
# To avoid DLL hell in Windows 3.1, make sure this base module name is unique
# to your game so that this game's DLLs never conflict in memory with another
# game's DLLs. In 32-bit Windows, conflicting DLLs in memory are never an issue.
# 
# If you're in a hurry or lazy, use uuidgen
#
# Perhaps in the future, some game dev tools will just patch over this string
# in the EXE without recompiling, so it helps to keep the string long.
MODULENAME_BASE = game-12324928-7b6b-4f1c-80d1-faa748ddce9b

