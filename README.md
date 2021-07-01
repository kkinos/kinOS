# kinOS

## カーネルのビルド

`kernel/`ディレクトリ内で

```bash
source ~/osbook/devenv/buildenv.sh
make
```

## ブートローダーのビルド

### EDK2 をクローンする

`tools/`ディレクトリで

```bash
git clone https://github.com/tianocore/edk2.git -b edk2-stable202102
```

### 環境変数設定

`edk2/`ディレクトリで

```bash
git submodule update --init
make -C BaseTools/Source/C
```

さらに

```bash
source edksetup.sh
```

により Conf/target.txt ファイルが生成され、EDK2 のビルドに必要な環境変数が読み込まれる。

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

### ビルドする

`edk2/`ディレクトリで

```bash
build
```

## QEMU で起動

ルートディレクトリ内で

```bash
~/osbook/devenv/run_qemu.sh ~projects/kinOS/tools/edk2/Build/kinLoaderX64/DEBUG_CLANG38/X64/kinLoader.efi ~/projects/kinOS/kernel/kernel.elf
```

## 参考資料
