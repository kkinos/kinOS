#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Library/MemoryAllocationLib.h>
#include  <Library/BaseMemoryLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>
#include  <Guid/FileInfo.h>
#include  "frame_buffer_config.hpp"
#include  "elf.hpp"
#include  "memory_map.hpp"




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
    EFI_STATUS status;
    CHAR8 buf[256];
    UINTN len;

    CHAR8* header =
        "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
    len = AsciiStrLen(header);
    status = file->Write(file, &len, header);
    if (EFI_ERROR(status)) {
    return status;
    }

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
        status = file->Write(file, &len, buf);
        if (EFI_ERROR(status)) {
        return status;
        }
    }
    
    return EFI_SUCCESS;
};

EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL **root) {
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

    status = gBS->OpenProtocol(
        image_handle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&loaded_image,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status)) {
      return status;
    }

    status = gBS->OpenProtocol(
      loaded_image->DeviceHandle,
      &gEfiSimpleFileSystemProtocolGuid,
      (VOID**)&fs,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status)) {
      return status;
    }

    return fs->OpenVolume(fs, root);
    
}
/*gopを取得 gop->Mode->FrameBufferBaseにフレームバッファの先頭アドレス、gop->Mode->FrameBufferSizeに全体のサイズ、
　gop->Mode->Info->PixelFormatにピクセルのデータ形式*/
EFI_STATUS OpenGOP(EFI_HANDLE image_handle,
                   EFI_GRAPHICS_OUTPUT_PROTOCOL** gop) {
    EFI_STATUS status;
    UINTN num_gop_handles = 0;
    EFI_HANDLE* gop_handles = NULL;

    status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        &num_gop_handles,
        &gop_handles);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = gBS->OpenProtocol(
        gop_handles[0],
        &gEfiGraphicsOutputProtocolGuid,
        (VOID**)gop,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status)) {
        return status;
    }

    FreePool(gop_handles);

    return EFI_SUCCESS;

}

const CHAR16* GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt) {
  switch (fmt) {
    case PixelRedGreenBlueReserved8BitPerColor:
      return L"PixelRedGreenBlueReserved8BitPerColor";
    case PixelBlueGreenRedReserved8BitPerColor:
      return L"PixelBlueGreenRedReserved8BitPerColor";
    case PixelBitMask:
      return L"PixelBitMask";
    case PixelBltOnly:
      return L"PixelBltOnly";
    case PixelFormatMax:
      return L"PixelFormatMax";
    default:
      return L"InvalidPixelFormat";
  }
}

void Halt(void) {
  while (1) __asm__("hlt");
}

/*LOADセグメントすべて合わせたものの先頭と末尾のアドレスを求める*/
void CalcLoadAddressRange(Elf64_Ehdr* ehdr, UINT64* first, UINT64* last) {
  Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
  *first = MAX_UINT64;
  *last = 0;
  for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_LOAD) continue;
    *first = MIN(*first, phdr[i].p_vaddr);
    *last = MAX(*last, phdr[i].p_vaddr + phdr[i].p_memsz);
  }
}

void CopyLoadSegments(Elf64_Ehdr* ehdr) {
  Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
  for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_LOAD) continue;

    UINT64 segm_in_file = (UINT64)ehdr + phdr[i].p_offset;
    CopyMem((VOID*)phdr[i].p_vaddr, (VOID*)segm_in_file, phdr[i].p_filesz);

    UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
    SetMem((VOID*)(phdr[i].p_vaddr + phdr[i].p_filesz), remain_bytes, 0);
  }
}

EFI_STATUS ReadFile(EFI_FILE_PROTOCOL* file, VOID** buffer) {
    EFI_STATUS status;

    UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
    UINT8 file_info_buffer[file_info_size];
    status = file->GetInfo(
        file, &gEfiFileInfoGuid,
        &file_info_size, file_info_buffer);
    if (EFI_ERROR(status)) {
        return status;
    }

    EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer;
    UINTN file_size = file_info->FileSize;

    status = gBS->AllocatePool(EfiLoaderData, file_size, buffer);
    if (EFI_ERROR(status)) {
        return status;
    }

    return file->Read(file, &file_size, *buffer);
}

EFI_STATUS OpenBlockIoProtocolForLoadedImage(
    EFI_HANDLE image_handle, EFI_BLOCK_IO_PROTOCOL** block_io) {
        EFI_STATUS status;
        EFI_LOADED_IMAGE_PROTOCOL* loaded_image;

        status = gBS->OpenProtocol(
            image_handle,
            &gEfiLoadedImageProtocolGuid,
            (VOID**)&loaded_image,
            image_handle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        if (EFI_ERROR(status)) {
            return status;
        }

        status = gBS->OpenProtocol(
            loaded_image->DeviceHandle,
            &gEfiBlockIoProtocolGuid,
            (VOID**)block_io,
            image_handle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        
        return status;
    }

    EFI_STATUS ReadBlocks(
        EFI_BLOCK_IO_PROTOCOL* block_io, UINT32 media_id,
        UINTN read_bytes, VOID** buffer) {
            EFI_STATUS status;

            status = gBS->AllocatePool(EfiLoaderData, read_bytes, buffer);
            if (EFI_ERROR(status)) {
                return status;
            }

            status = block_io->ReadBlocks(
                block_io,
                media_id,
                0,
                read_bytes,
                *buffer);

            return status;
        }

/*シェルを初期化する*/
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
    Print(L"command 'kinos' to go kernel\n");
    Print(L"command 'help' for command list\n");
    Print(L"\n");
    Print(L"\n");
    gBS->SetWatchdogTimer(0, 0, 0, NULL);
}

/*シェルで呼び出す関数*/
void hello(void) {
    Print(L"Hello!\n");
}

void help(void) {
    Print(L"kinos --- go kernel\n");
    Print(L"welcome --- show welcome page again\n");
    Print(L"hello --- Hello!\n");

}

void shell(EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE *system_table) {
    EFI_STATUS status;
    CHAR16 com[MAX_COMMAND_LEN];
    CHAR8 memmap_buf[4096 * 4];
    struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
    status = GetMemoryMap(&memmap);
    if (EFI_ERROR(status)) {
        Print(L"failed to get memory map: %r\n", status);
        Halt();
    }

    EFI_FILE_PROTOCOL* root_dir;
    status = OpenRootDir(image_handle, &root_dir);
    if (EFI_ERROR(status)) {
        Print(L"failed to open root directory: %r\n", status);
        Halt();
    }

    EFI_FILE_PROTOCOL* memmap_file;
    status = root_dir->Open(
        root_dir, &memmap_file, L"\\memmap",
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(status)) {
        Print(L"failed to open file '\\memmap': %r\n", status);
        Print(L"Ignored.\n");
        } else {   
    status = SaveMemoryMap(&memmap, memmap_file);
    if (EFI_ERROR(status)) {
        Print(L"failed to save memory map: %r\n", status);
        Halt();
    }
    status = memmap_file->Close(memmap_file);
    if (EFI_ERROR(status)) {
      Print(L"failed to close memory map: %r\n", status);
      Halt();
    }
    }
    Print(L"Memory is OK!\n");
    Print(L"\n");
    Print(L"\n");



    while (TRUE) {
        Print(L"kinos:>\n");
        Print(L"$ ");
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;
        if(!strcmp(L"hello", com)) hello();
        else if(!strcmp(L"help", com)) help();
        else if(!strcmp(L"welcome", com)) efi_init();

        /*カーネル*/
        else if(!strcmp(L"kinos",com)) {
            /*ピクセルgopを取得*/
            EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
            status = OpenGOP(image_handle, &gop);
            if (EFI_ERROR(status)) {
                Print(L"failed to open GOP: %r\n", status);
                Halt();
            }

            /*カーネルファイル読み込み*/
            EFI_FILE_PROTOCOL* kernel_file;
            status = root_dir->Open(
                root_dir, &kernel_file, L"\\kernel.elf",
                EFI_FILE_MODE_READ, 0);
            if (EFI_ERROR(status)) {
                Print(L"failed to open file '\\kernel.elf': %r\n", status);
                Halt();
            }
            
            UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
            UINT8 file_info_buffer[file_info_size];
            status =  kernel_file->GetInfo(
                kernel_file, &gEfiFileInfoGuid,
                &file_info_size, file_info_buffer);
            if (EFI_ERROR(status)) {
                Print(L"failed to get file information: %r\n", status);
                Halt();
            }

            EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer;
            UINTN kernel_file_size = file_info->FileSize;
            
            /*カーネルファイルを一時的に読み込む*/
            VOID* kernel_buffer;
            status = gBS->AllocatePool(EfiLoaderData, kernel_file_size, &kernel_buffer);
            if (EFI_ERROR(status)) {
                Print(L"failed to allocate pool: %r\n", status);
                Halt();
            }
            status = kernel_file->Read(kernel_file, &kernel_file_size, kernel_buffer);
            if (EFI_ERROR(status)) {
                Print(L"error: %r", status);
                Halt();
            }
            
            /*LOADセグメントの全体のアドレスを計算*/
            Elf64_Ehdr* kernel_ehdr = (Elf64_Ehdr*)kernel_buffer;
            UINT64 kernel_first_addr, kernel_last_addr;
            CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr);

            UINTN num_pages = (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000;
            status = gBS->AllocatePages(AllocateAddress, EfiLoaderData,
                                        num_pages, &kernel_first_addr);
            if (EFI_ERROR(status)) {
                Print(L"failed to allocate pages: %r\n", status);
                Halt();
            }

            /*最終目的地へコピー*/
            CopyLoadSegments(kernel_ehdr);
            Print(L"Kernel: 0x%0lx - 0x%0lx\n", kernel_first_addr, kernel_last_addr);

            status = gBS->FreePool(kernel_buffer);
            if (EFI_ERROR(status)) {
                Print(L"failed to free pool: %r\n", status);
                Halt();
            }

            VOID* volume_image;

            EFI_FILE_PROTOCOL* volume_file;
            status = root_dir->Open(
                root_dir, &volume_file, L"\\fat_disk",
                EFI_FILE_MODE_READ, 0);
            if (status == EFI_SUCCESS) {
                status = ReadFile(volume_file, &volume_image);
                if (EFI_ERROR(status)) {
                    Print(L"failed to read volume file: %r", status );
                    Halt();
                }
            } else {
                EFI_BLOCK_IO_PROTOCOL* block_io;
                status = OpenBlockIoProtocolForLoadedImage(image_handle, &block_io);
                if (EFI_ERROR(status)) {
                    Print(L"failed to open Block I/O Protocol: %r\n", status);
                    Halt();
                }

                EFI_BLOCK_IO_MEDIA* media = block_io->Media;
                UINTN volume_bytes = (UINTN)media->BlockSize * (media->LastBlock + 1);
                if (volume_bytes > 16 * 1024 * 1024) {
                    volume_bytes = 16 * 1024 * 1024;
                }

                Print(L"Reading %lu bytes (Present %d, BlockSize %u, LastBlock %u)\n",
                      volume_bytes, media->MediaPresent, media->BlockSize, media->LastBlock);
                    
                status = ReadBlocks(block_io, media->MediaId, volume_bytes, &volume_image);
                if (EFI_ERROR(status)) {
                    Print(L"failed to read blocks: %r\n", status);
                    Halt();
                }
            }

            status = gBS->ExitBootServices(image_handle, memmap.map_key);
            if (EFI_ERROR(status)) {
                status = GetMemoryMap(&memmap);
                if (EFI_ERROR(status)) {
                    Print(L"failed to get memory map: %r\n", status);
                    Halt();
                    }
                status = gBS->ExitBootServices(image_handle, memmap.map_key);
                if (EFI_ERROR(status)) {
                    Print(L"Could not exit boot service: %r\n", status);
                    Halt();
                }
            }

            UINT64 entry_addr = *(UINT64*)(kernel_first_addr + 24);

            struct  FrameBufferConfig config = {
                (UINT8*)gop->Mode->FrameBufferBase,
                gop->Mode->Info->PixelsPerScanLine,
                gop->Mode->Info->HorizontalResolution,
                gop->Mode->Info->VerticalResolution,
                0
            };
            switch (gop->Mode->Info->PixelFormat) {
            case PixelRedGreenBlueReserved8BitPerColor:
                config.pixel_format = kPixelRGBResv8BitPerColor;
                break;
            case PixelBlueGreenRedReserved8BitPerColor:
                config.pixel_format = kPixelBGRResv8BitPerColor;
                break;
            default:
            Print(L"Unimplemented pixel format: %d\n", gop->Mode->Info->PixelFormat);
            Halt();
            }
            
            VOID* acpi_table = NULL;
            for (UINTN i = 0; i < system_table->NumberOfTableEntries; ++i) {
                if (CompareGuid(&gEfiAcpiTableGuid,
                                &system_table->ConfigurationTable[i].VendorGuid)) {
                acpi_table = system_table->ConfigurationTable[i].VendorTable;
                break;
                                }
            }

            typedef void EntryPointType(const struct FrameBufferConfig*,
                                        const struct MemoryMap*,
                                        const VOID*,
                                        VOID*);
            EntryPointType* entry_point = (EntryPointType*)entry_addr;
            entry_point(&config, &memmap, acpi_table, volume_image);
            while(1);

        }
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