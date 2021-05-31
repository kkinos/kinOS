#include <Uefi.h>
#include <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleTextIn.h>

#define MAX_COMMAND_LEN     100

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
    Print(L"command 'kinos' to GUI\n");
    Print(L"command 'help' for more infomation\n");
    Print(L"\n");
    Print(L"\n");
    gBS->SetWatchdogTimer(0, 0, 0, NULL);
}

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

/*シェル*/
void shell(void) {
    CHAR16 com[MAX_COMMAND_LEN];

    while (TRUE) {
        Print(L"kinos>$ ");
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;
        if(!strcmp(L"hello", com)) Print(L"Hello kinpoko!\n");
        else if(!strcmp(L"help", com)) Print(L"help kinpoko!\n");
        else Print(L"Command not found\n");
    }
}

EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE *system_table) {

        efi_init();

        shell();

        return EFI_SUCCESS;
    }