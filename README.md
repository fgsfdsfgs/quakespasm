# Quakespasm - Xbox port

## How to play

1. Put `default.xbe` into a folder on your HDD.
2. Put the `id1` directory from your Quake install next to it.
3. Delete any `.cfg` files that are in the `id1` directory.
4. (Optional) Put `quakespasm.pak` from the `Quake` directory in this repo into the `id1` directory.
5. Run the XBE.

## How to build

This should work on Linux or WSL. You need clang and lld installed before installing NXDK.

1. Get [NXDK](https://github.com/XboxDev/nxdk) and all its requirements:
   ```
   git clone --recursive https://github.com/XboxDev/nxdk.git
   ```
2. Get [pbGL](https://github.com/fgsfdsfgs/pbgl) and put it into `$(NXDK_DIR)/lib/`:
   ```
   cd nxdk/lib
   git clone https://github.com/fgsfdsfgs/pbgl.git
   ```
3. Get [nxdk-floatfmt](https://github.com/fgsfdsfgs/nxdk-floatfmt) and put it into `$(NXDK_DIR)/lib/`:
   ```
   cd nxdk/lib
   git clone --recursive https://github.com/fgsfdsfgs/nxdk-floatfmt.git
   ```
4. Get [this repository](https://github.com/fgsfdsfgs/quakespasm.git):
   ```
   git clone --recursive https://github.com/fgsfdsfgs/quakespasm.git --branch nxdk
   ```
5. Activate NXDK:
   ```
   cd nxdk
   ./bin/activate
   ```
6. Build:
   ```
   cd quakespasm/Quake
   # If you put pbGL and nxdk-floatfmt into nxdk/lib:
   make -f Makefile.xbox -j8
   # Or, if you put them in different directories:
   make -f Makefile.xbox -j8 PBGL_DIR=path/to/pbgl FLOATFMT_DIR=path/to/floatfmt
   ```
