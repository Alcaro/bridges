<h1>Moved to https://git.disroot.org/Sir_Walrus/bridges</h1>

# Lord of Bridges

Enhanced remake of http://www.notdoppler.com/kingofbridges.php

![Screenshot](https://github.com/Alcaro/bridges/blob/master/pic.png)

## Compilation - Linux (Debian/Ubuntu)

```
sudo apt-get install make libx11-dev mesa-common-dev python3
# (not sure if the above is complete)
git clone
make OPT=1
```

## Compilation - other Linux

Adjust the above.

## Compilation - Windows

Install Python and MinGW; I use [https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/8.1.0/threads-win32/seh/](mingw-w64), others (for example MSYS2) are untested but will probably work. Run `mingw32-make OPT=1`. (Not tested on a real Windows, I just use mingw in Wine.)

Alternatively, if you prefer binaries, [here's mine](https://github.com/Alcaro/bridges/raw/master/bridges.exe).

## Compilation - other

Untested and unlikely to work, but if you're feeling adventurous, feel free to try.
