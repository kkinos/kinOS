typedef unsigned short CHAR16;
typedef unsigned long long EFI_STATUS; /*Status code*/
typedef void *EFI_HANDLE; /*A collection of related interfaces*/

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef EFI_STATUS (*EFI_TEXT_STRING)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16                           *String
);

typedef struct  _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL{
    void                    *dummy;
    EFI_TEXT_STRING         OutputString;            

} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    char                            dummy[52]; /*52byte*/
    EFI_HANDLE                      ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *Conout;

} EFI_SYSTEM_TABLE;


EFI_STATUS EfiMain(EFI_HANDLE       ImageHandle,
                   EFI_SYSTEM_TABLE *SystemTable) {
    SystemTable->Conout->OutputString(SystemTable->Conout, L"Hello, kinpoko!\n");
    while(1);
    return 0;
}