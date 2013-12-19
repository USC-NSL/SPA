
seccomp-sandbox includes a Gyp file for building with the Gyp makefile
generator (see http://code.google.com/p/gyp/).  This is primarily for
building seccomp-sandbox as part of Chromium, but it can also be used
to build seccomp-sandbox standalone.


To build seccomp-sandbox with Gyp you can do the following:

1) Check out Gyp from SVN:

$ svn checkout http://gyp.googlecode.com/svn/trunk

2) Add the "gyp" executable (a Python script) to your PATH

3) Run Gyp:

$ gyp seccomp.gyp --depth=.

This generates "Makefile".  Note that the non-Gyp, non-generated
makefile is named "makefile" without an upper case "M" so that it does
not get overwritten by Gyp.

4) Run GNU make to build the library:

$ make -f Makefile
...
  AR(target) out/Default/obj.target/libseccomp_sandbox.a

The resulting library is put in out/Default/obj.target.

Note that the tests are not built by seccomp.gyp yet.


To use a build directory that is separate from the source directory,
you can do the following:

mkdir -p build
cd build
gyp ../seccomp.gyp --depth=.
make
