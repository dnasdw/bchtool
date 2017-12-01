# bchtool

A tool for exporting/importing bch file.

## History

- v1.0.0 @ 2017.07.30 - First release
- v1.0.1 @ 2017.10.23 - Support very old version
- v1.1.0 @ 2017.12.01 - Fix L4/A4 section

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
mkdir project
cd project
cmake -DUSE_DEP=OFF ..
make
~~~

- make 32-bit version
~~~
mkdir project
cd project
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
