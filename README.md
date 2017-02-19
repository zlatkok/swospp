# SWOS++

Add-on for legendary Sensible World of Soccer. Indispensable at tournaments. Features:
- play games over the Internet
- save and replay complete matches
- in-game control calibration and support for second keyboard player
- DIY competition editor
- expanded shirt range to 0-255
- auto-save options, even on fast exit
- some small original game bug fixes
- ...and more... :)

Full change list can be seen [here](doc/changes.txt).


## Installation

Copy `swospp.bin`, `loader.bin` and `patchit.com` into your Sensible World of Soccer installation directory.
Run `patchit.com` and choose option 1 (install). If everything goes well it should patch the game's executable
and print a message that everything went well. After successful patch `patchit.com` is not necessary anymore and
may be deleted.

To use SWOS++ simply run the game as usual as all features are integrated.


## Hierarchy

<table>
    <tr><th>Directory</th><th>Description</th></tr>
    <tr><td>bin</td><td>binary files, SWOS++, loader, utility programs...</td></tr>
    <tr><td>dist</td><td>this is where the dist archive will be created</tr>
    <tr><td>doc</td><td>various documentation, both internal and public</tr>
    <tr><td>extra</td><td>additional scripts and executables needed for build</tr>
    <tr><td>loader</td><td>loader source</tr>
    <tr><td>main</td><td>SWOS++ source</tr>
    <tr><td>mapcvt</td><td>map file and source of converter from map file to .asm/.inc/.h</tr>
    <tr><td>obj</td><td>object files, as well as executables</tr>
    <tr><td>other</td><td>other sources not belonging to the project</tr>
    <tr><td>patch</td><td>patcher source</tr>
    <tr><td>pe2bin</td><td>Win32 PE to custom executable format converter</tr>
    <tr><td>test</td><td>unit tests for various components, and miscellaneous test code</tr>
    <tr><td>var</td><td>various files that don't belong anywhere else, listings, lib files, etc.</tr>
</table>


## Software needed for full recompilation

- GNU make
- small subset of any working Unix environment (for shell, and some UNIX utilities)
- GCC 4.9.x (4.8.x might not work)
- Open Watcom C Compiler (main program used to be compiled with it, but
  nowadays used for test programs only)
- NASM 0.98.36 my patched version (in /extra)
- Perl, any not-totally-ancient version will do
- Python 3, same note
- alink, my modified version (in /extra)


## Environment

Few environment variables need to be set to build and use utility scripts:
- SWOS        - contains path where SWOS is installed
- SWOSPP      - contains path to SWOS++ dev root dir
- SWOS_DIRS   - additional SWOS directories for testing (separated with semicolon)



## License

MIT
