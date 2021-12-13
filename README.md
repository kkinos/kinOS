# kinOS

kinOS is a microkernel-like operating system based on [MikanOS](https://github.com/uchan-nos/mikanos).

![screenshot](screenshot.png)

## Build

The tools required for building are the same as for [MikanOS](https://github.com/uchan-nos/mikanos)

### Clone EDK2

In `tools/`

```bash
git clone https://github.com/tianocore/edk2.git -b edk2-stable202102
```

### Setting

In `edk2/`

```bash
git submodule update --init
```

```bash
make -C BaseTools/Source/C
```

```bash
source edksetup.sh
```

### Edit Conf/target.txt

| item            | value                         |
| --------------- | ----------------------------- |
| ACTIVE_PLATFORM | kinLoaderPkg/kinLoaderPkg.dsc |
| TARGET          | DEBUG                         |
| TARGET_ARCH     | X64                           |
| TOOL_CHAIN_TAG  | CLANG38                       |

### Make a symbolic link

In `edk2/`

```bash
ln -s /path/to/kinos/kinLoaderPkg ./

```

### Build and run

```bash
make
```
