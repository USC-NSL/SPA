# Microsoft Developer Studio Project File - Name="w32_vc6" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=w32_vc6 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "w32_vc6.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "w32_vc6.mak" CFG="w32_vc6 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "w32_vc6 - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "w32_vc6 - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "w32_vc6 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x1009 /d "NDEBUG"
# ADD RSC /l 0x1009 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "w32_vc6 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\include" /I ".\\" /D "WIN32" /D "_DEBUG" /YX /FD /GZ /c
# ADD BASE RSC /l 0x1009 /d "_DEBUG"
# ADD RSC /l 0x1009 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "w32_vc6 - Win32 Release"
# Name "w32_vc6 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\benc.c
# End Source File
# Begin Source File

SOURCE=..\src\bitset.c
# End Source File
# Begin Source File

SOURCE=..\src\block.c

!IF  "$(CFG)" == "w32_vc6 - Win32 Release"

!ELSEIF  "$(CFG)" == "w32_vc6 - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\btcheck.c

!IF  "$(CFG)" == "w32_vc6 - Win32 Release"

!ELSEIF  "$(CFG)" == "w32_vc6 - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\btget.c
# End Source File
# Begin Source File

SOURCE=..\src\btlist.c

!IF  "$(CFG)" == "w32_vc6 - Win32 Release"

!ELSEIF  "$(CFG)" == "w32_vc6 - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\bts.c
# End Source File
# Begin Source File

SOURCE=..\src\context.c
# End Source File
# Begin Source File

SOURCE=.\getopt.c
# End Source File
# Begin Source File

SOURCE=..\src\peer.c
# End Source File
# Begin Source File

SOURCE=.\poll.c
# End Source File
# Begin Source File

SOURCE=..\src\random.c
# End Source File
# Begin Source File

SOURCE=..\src\segmenter.c
# End Source File
# Begin Source File

SOURCE=..\src\strbuf.c
# End Source File
# Begin Source File

SOURCE=..\src\stream.c
# End Source File
# Begin Source File

SOURCE=..\src\types.c
# End Source File
# Begin Source File

SOURCE=..\src\util.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\include\benc.h
# End Source File
# Begin Source File

SOURCE=..\include\bitset.h
# End Source File
# Begin Source File

SOURCE=..\include\bterror.h
# End Source File
# Begin Source File

SOURCE=..\include\bts.h
# End Source File
# Begin Source File

SOURCE=..\include\config.h
# End Source File
# Begin Source File

SOURCE=..\include\context.h
# End Source File
# Begin Source File

SOURCE=.\GETOPT.h
# End Source File
# Begin Source File

SOURCE=..\include\peer.h
# End Source File
# Begin Source File

SOURCE=.\poll.h
# End Source File
# Begin Source File

SOURCE=..\include\random.h
# End Source File
# Begin Source File

SOURCE=..\include\segmenter.h
# End Source File
# Begin Source File

SOURCE=..\include\strbuf.h
# End Source File
# Begin Source File

SOURCE=..\include\stream.h
# End Source File
# Begin Source File

SOURCE=..\include\types.h
# End Source File
# Begin Source File

SOURCE=..\include\util.h
# End Source File
# End Group
# End Target
# End Project
