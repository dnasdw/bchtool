# bchtool

A tool for exporting/importing bch file.

## History

- v1.0.0 @ 2017.07.30 - First release

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
