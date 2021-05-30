.PHONY: all

all:
	clang -target x86_64-pc-win32-coff \
	-mno-red-zone -fno-stack-protector -fshort-wchar -Wall -c Main.c
	lld-link /subsystem:efi_application /entry:EfiMain /out:Main.efi Main.o