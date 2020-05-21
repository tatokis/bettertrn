# BetterTRN
An educational computer emulator!
___

![screenshot](https://raw.githubusercontent.com/tatokis/bettertrn/master/screenshot.png)

Originally developed at NTUA.

Unfortunately, the original software was really slow, with many bugs, and practically unusable on Linux despite being written in Java.
BetterTRN is a complete implementation of the TRN+ computer from scratch, using the cross platform Qt framework, with many, many improvements.

## Installation

### Windows
Download the latest [win32 release archive](https://github.com/tatokis/bettertrn/releases/latest) and extract it somewhere. Then run `bettertrn.exe`.

### Linux
Build from source. Either use Qt Creator, or run the following commands.
Instructions are for Ubuntu, but should apply to all distros with the appropriate package manager command

```
sudo apt install build-essential git qt5-default
git clone https://github.com/tatokis/bettertrn
cd bettertrn
qmake && make -j4
./bettertrn
```

### macOS
Either install the official Qt package and open up the project in Qt Creator, or use homebrew with qmake + make

## Documentation and examples
Can be found inside the docs and examples folders.

## License
Licensed under GNU GPLv3 or (at your option) any later version. See LICENSE.
