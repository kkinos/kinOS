#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>

#define MAX_COMMAND_LEN     100

void putc(CHAR16 c) {
	CHAR16 str[2] = L" ";
	str[0] = c;
	Print(str);
}

CHAR16 getc(void) {
    EFI_INPUT_KEY key;
    UINTN waitindex;

    gBS->WaitForEvent(1, &(gST->ConIn->WaitForKey), &waitindex);
    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &key));
    return key.UnicodeChar;

}

UINT32 gets(CHAR16 *buf, UINT32 buf_size) {
    UINT32 i;

    for (i = 0; i < buf_size - 1;) {
        buf[i] = getc();
        putc(buf[i]);
        if (buf[i] == L'\r') {
            putc(L'\n');
            break;
        }
        i++;
    }
    buf[i] = L'\0';

    return i;
}

int strcmp(const CHAR16 *s1, const CHAR16 *s2)
{
	char is_equal = 1;

	for (; (*s1 != L'\0') && (*s2 != L'\0'); s1++, s2++) {
		if (*s1 != *s2) {
			is_equal = 0;
			break;
		}
	}

	if (is_equal) {
		if (*s1 != L'\0') {
			return 1;
		} else if (*s2 != L'\0') {
			return -1;
		} else {
			return 0;
		}
	} else {
		return (int)(*s1 - *s2);
	}
}

/*メモリマップ*/
struct MemoryMap {
    UINTN buffer_size;              /*メモリマップ全体を書き込むためのサイズ*/
    VOID* buffer;                   /*メモリマップの先頭を指すポインタ*/
    UINTN map_size;                 /*メモリマップのサイズ*/
    UINTN map_key;                  /*メモリマップの種類*/
    UINTN descriptor_size;          /*個々のEFI_MEMORY_DESCRIPTORのサイズ*/
    UINT32 descriptor_version;      /*EFI_MEMORY_DESCRIPTORのバージョン*/
};

EFI_STATUS GetMemoryMap(struct MemoryMap* map) {
    if (map->buffer == NULL) {
        return EFI_BUFFER_TOO_SMALL;
    }

    map->map_size = map->buffer_size;
    return gBS->GetMemoryMap (
        &map->map_size,
        (EFI_MEMORY_DESCRIPTOR*)map->buffer,
        &map->map_key,
        &map->descriptor_size,
        &map->descriptor_version);
}

/*メモリマップの種類の判別*/
const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
  switch (type) {
    case EfiReservedMemoryType: return L"EfiReservedMemoryType";
    case EfiLoaderCode: return L"EfiLoaderCode";
    case EfiLoaderData: return L"EfiLoaderData";
    case EfiBootServicesCode: return L"EfiBootServicesCode";
    case EfiBootServicesData: return L"EfiBootServicesData";
    case EfiRuntimeServicesCode: return L"EfiRuntimeServicesCode";
    case EfiRuntimeServicesData: return L"EfiRuntimeServicesData";
    case EfiConventionalMemory: return L"EfiConventionalMemory";
    case EfiUnusableMemory: return L"EfiUnusableMemory";
    case EfiACPIReclaimMemory: return L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS: return L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO: return L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace: return L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode: return L"EfiPalCode";
    case EfiPersistentMemory: return L"EfiPersistentMemory";
    case EfiMaxMemoryType: return L"EfiMaxMemoryType";
    default: return L"InvalidMemoryType";
  }
}


EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file) {
    CHAR8 buf[256];
    UINTN len;

    CHAR8* header =
        "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
    len = AsciiStrLen(header);
    file->Write(file, &len, header);

    Print(L"map->buffer = %08lx, map->map_size = %08lx\n",
      map->buffer, map->map_size);

     EFI_PHYSICAL_ADDRESS iter;
     int i;
     for (iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0;
          iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
          iter += map->descriptor_size, i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;
        len = AsciiSPrint(
            buf, sizeof(buf),
            "%u, %x, %-ls, %08lx, %lx, %lx\n",
            i, desc->Type, GetMemoryTypeUnicode(desc->Type),
            desc->PhysicalStart, desc->NumberOfPages,
            desc->Attribute & 0xffffflu);
        file->Write(file, &len, buf);
        }
    
    return EFI_SUCCESS;
};

EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL **root) {
    EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

    gBS->OpenProtocol(
        image_handle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&loaded_image,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    gBS->OpenProtocol(
      loaded_image->DeviceHandle,
      &gEfiSimpleFileSystemProtocolGuid,
      (VOID**)&fs,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    fs->OpenVolume(fs, root);
    return EFI_SUCCESS;
}

/*初期化する*/
void efi_init(void) {
    Print(L"welcome to \n");
    Print(L"\n");
    Print(L":::    ::: ::::::::::: ::::    :::  ::::::::   ::::::::\n");
    Print(L":+:   :+:      :+:     :+:+:   :+: :+:    :+: :+:    :+:\n");
    Print(L"+:+  +:+       +:+     :+:+:+  +:+ +:+    +:+ +:+\n");
    Print(L"+#++:++        +#+     +#+ +:+ +#+ +#+    +:+ +#++:++#++\n");
    Print(L"+#+  +#+       +#+     +#+  +#+#+# +#+    +#+        +#+\n");
    Print(L"#+#   #+#      #+#     #+#   #+#+# #+#    #+# #+#    #+#\n");
    Print(L"###    ### ########### ###    ####  ########   ########\n");
    Print(L"\n");
    Print(L"\n");
    Print(L"this is kinos's bootloader\n");
    Print(L"command 'kinos' to kernel\n");
    Print(L"command 'help' for more infomation\n");
    Print(L"\n");
    Print(L"\n");
    gBS->SetWatchdogTimer(0, 0, 0, NULL);
}

/*シェル*/
void shell(EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE *system_table) {
    CHAR16 com[MAX_COMMAND_LEN];
    CHAR8 memmap_buf[4096 * 4];
    struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
    GetMemoryMap(&memmap);

    EFI_FILE_PROTOCOL* root_dir;
    OpenRootDir(image_handle, &root_dir);

    EFI_FILE_PROTOCOL* memmap_file;
    root_dir->Open(
        root_dir, &memmap_file, L"\\memmap",
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
        
    SaveMemoryMap(&memmap, memmap_file);
    memmap_file->Close(memmap_file);
    Print(L"Memory is OK!\n");
    Print(L"\n");
    Print(L"\n");

    while (TRUE) {
        Print(L"kinos:>\n");
        Print(L"$ ");
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;
        if(!strcmp(L"hello", com)) Print(L"Hello kinpoko!\n");
        else if(!strcmp(L"help", com)) Print(L"help kinpoko!\n");
        else if(!strcmp(L"welcome", com)) efi_init();
        else Print(L"Command not found\n");

    }
    
}

EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE* system_table) {

        efi_init();
        shell(image_handle, system_table);

        return EFI_SUCCESS;
    }