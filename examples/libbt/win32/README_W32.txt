The Windows port of libbt is currently in development. So far, only btget.exe is functional (as a console app). If you're adventurous, compiling a library is possible with minor changes to the project. A DLL is planned once the core functionality is fully tested. It has been developed and tested in WinXP, but that's it. There's no reason why it shouldn't work under Win98/ME, NT4 and Win2k, but I haven't tested it. It'll probably crash under Win95, or older versions of NT.

The project and solution files in this directory are for VisualStudio.NET version 7.1. I have compiled it under VC6 with no problems, but you'll have to create a new project file for it. I haven't tried any other compilers, so if you build a makefile for it, let us know so we can include it in the source tree.

Creating a new project or makefile is easy enough. The windows port uses the same source and header files as the Linux version, with one addition: getopt.c/h These files are included in this directory and are required to compile btget.exe in Visual Studio (people with the standard GNU libs might already have it, I didn't). You should also make sure that all of the necessary include directories are added to the project. These are, but aren't limited to: libbt\include;libbt\win32;(path to windows SDK)\include

Libbt also requires two external and two windows libraries. These should be part of your project's Linker->Input->Dependencies.

The libraries are as follows: libcurl.lib, libeay32.lib, ws2_32.lib, Rpcrt4.lib

You can find source for these at:

* OpenSSL (libeay32) (http://www.openssl.org/)
* LibCURL (http://curl.haxx.se/libcurl/)

The other two libs are windows standard. They should be in your windows/system32 dir, or you can find them in the Windows SDK.

The following preprocessor definitions are required too: WIN32;CURLLIB_EXPORTS

It compiles clean for me (no errors or warnings), so if you're getting errors, you're probably missing a path, library or preprocessor definition. To run btget.exe, you need a copy of libcurl.dll and libeay32.dll in the same directory (or pathed). Make sure you use quotes if your torrent filename has spaces. (ex: btget "my long torrent name.torrent")

That's about it. This is alpha software, so use at your own risk. When compiled with the _BTDEBUG flag set to 1 (in util.h), you can expect your console to get spammed with debug messages. You can pipe the output into a log file if you want to review it later. (ex: btget "filename.torrent" >> btget.log) Ctrl-C works if you need to abort a transfer or just kill the program.


If you have any questions, or want to help out, mail me at: jbarons@shaw.ca