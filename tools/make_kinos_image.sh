#!/bin/sh -ex

DEVENV_DIR=$(dirname "$0")
MOUNT_POINT=./mnt

LOADER_EFI=$DEVENV_DIR/edk2/Build/kinLoaderX64/DEBUG_CLANG38/X64/kinLoader.efi
KERNEL_ELF=$KINOS_DIR/kernel/kernel.elf

$DEVENV_DIR/make_image.sh $DISK_IMG $MOUNT_POINT $LOADER_EFI $KERNEL_ELF
$DEVENV_DIR/mount_image.sh $DISK_IMG $MOUNT_POINT

if [ "$APPS_DIR" != "" ]
then
  sudo mkdir $MOUNT_POINT/$APPS_DIR
fi

for APP in $(ls "$KINOS_DIR/apps")
do
  if [ -f $KINOS_DIR/apps/$APP/$APP ]
  then
    sudo cp "$KINOS_DIR/apps/$APP/$APP" $MOUNT_POINT/$APPS_DIR
  fi
done

if [ "$RESOURCE_DIR" != "" ]
then
  sudo cp $KINOS_DIR/$RESOURCE_DIR/* $MOUNT_POINT/
fi

sleep 0.5
sudo umount $MOUNT_POINT