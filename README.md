# NES Debugger/NES, I guess?

NES Debugger is an application which allows for the runtime disassembly and
debugging of NES games within the terminal. Almost none of the code for the
debugging aspect of this program has been written yet. The files currently
in this repo represent "Nes, I guess?", which is the name of the emulator
which NES Debugger will built on top of.

"NES, I guess?" was written from scratch using the documentation available
on nesdev.com. In the master branch, the emulator can be compiled and will
run games using INES mappers 0-2. The style in the master branch is lackluster,
this is being addressed in the active branch, cpp. The master branch is written
entirely in C. The structure of the program follows the general idea of the
individual components of the NES being emulated alone and communicating with
each other similar to how they would in the real console. This allows for
an accurate emulation, if not an optimized one.

In order to compile the master branch, you need only the build tools for your
distribution of Linux, gcc, and SDL2-Dev. It can then be compiled using
the given make file. The emulator will be created as the executable "ndb".

The cpp branch represents a work-in-progress rewrite of this project to C++.
This is being done to increase the clarity of the code, as NES emulation
requires runtime polymorphism, which is more clear in C++. The cpp branch
is also a complete style rewrite, roughly following the Google C++ style
guide. This branch cannot be compiled as of yet, and is non-functional and
mostly untested.
