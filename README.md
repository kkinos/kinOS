# kinOS

## カーネルのビルド

`kernel/`ディレクトリ内で

```bash
$ source ~/osbook/devenv/buildenv.sh
$ make
```

## ブートローダーのビルド

### EDK2をクローンする

```bash
$ git clone https://github.com/tianocore/edk2.git
```

### 環境変数設定

`edk2/`ディレクトリ内で

```bash
$ make -C BaseTools/Source/C
```

さらに

```bash
$ source edksetup.sh
```

によりConf/target.txtファイルが生成され、EDK2のビルドに必要な環境変数が読み込まれる。

### Conf/target.txtを編集

| 設定項目 | 設定値 |
| ----  | ---- |
| ACTIVE_PLATFORM | kinLoaderPkg/kinLoaderPkg.dsc |
| TARGET | DEBUG |
|TARGET_ARCH | X64 |
| TOOL_CHAIN_TAG | CLANG38 |

### ブートローダーのリンクを貼る

```bash
$ ln -s /path/to/kinos/kinLoaderPkg ./

```

### ビルドする

```bash
$ build
```

## QEMUで起動

```bash
$ ~/osbook/devenv/run_qemu.sh ~/edk2/Build/kinLoaderX64/DEBUG_CLANG38/X64/kinLoader.efi ~/projects/kinOS/kernel/kernel.elf
```

## 参考資料

