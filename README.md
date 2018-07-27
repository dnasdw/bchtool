# bchtool

A tool for exporting/importing bch file.

## History

- v1.3.0 @ 2018.01.04 - A new beginning
- v1.3.1 @ 2018.07.27 - Update cmake

### v1.2

- v1.2.0 @ 2017.12.06 - Check L4/A4 section size

### v1.1

- v1.1.0 @ 2017.12.01 - Fix L4/A4 section

### v1.0

- v1.0.0 @ 2017.07.30 - First release
- v1.0.1 @ 2017.10.23 - Support very old version

## Platforms

- Windows
- Linux
- macOS

## Building

### Dependencies

- cmake
- zlib
- libpng

### Compiling

- make 64-bit version
~~~
mkdir build
cd build
cmake -DUSE_DEP=OFF ..
make
~~~

- make 32-bit version
~~~
mkdir build
cd build
cmake -DBUILD64=OFF -DUSE_DEP=OFF ..
make
~~~

### Installing

~~~
make install
~~~

## Usage

### Windows

~~~
bchtool [option...] [option]...
~~~

### Other

~~~
bchtool.sh [option...] [option]...
~~~

## Options

See `bchtool --help` messages.
