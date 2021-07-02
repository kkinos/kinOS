# kinOS

## ビルド方法

### EDK2 をクローンする

`tools/`ディレクトリ内で

```bash
git clone https://github.com/tianocore/edk2.git -b edk2-stable202102
```

### 環境変数設定

`edk2/`ディレクトリ内で

```bash
git submodule update --init
make -C BaseTools/Source/C
source edksetup.sh
```

### Conf/target.txt を編集

| 設定項目        | 設定値                        |
| --------------- | ----------------------------- |
| ACTIVE_PLATFORM | kinLoaderPkg/kinLoaderPkg.dsc |
| TARGET          | DEBUG                         |
| TARGET_ARCH     | X64                           |
| TOOL_CHAIN_TAG  | CLANG38                       |

### ブートローダーのリンクを貼る

`edk2/`ディレクトリで

```bash
ln -s /path/to/kinos/kinLoaderPkg ./

```

### カーネルとブートローダーをビルドする

ルートディレクトリ内で

```bash
make build
```

## QEMU で起動

ルートディレクトリ内で

```bash
make run
```

## ビルドして起動

ルートディレクトリ内で

```bash
make
```
