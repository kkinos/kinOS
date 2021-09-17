#include "fs.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

Vector2D<int> CalcCursorPos()
{
    return kTopLeftMargin + Vector2D<int>{4 + 8 * cursorx, 4 + 16 * cursory};
}

void DrawCursor(uint64_t layer_id, bool visible)
{
    const auto color = visible ? 0xffffff : 0;
    WinFillRectangle(layer_id, true, CalcCursorPos().x, CalcCursorPos().y, 7, 15, color);
}

Rectangle<int> BlinkCursor(uint64_t layer_id)
{
    cursor_visible_ = !cursor_visible_;
    DrawCursor(layer_id, cursor_visible_);
    return {CalcCursorPos(), {7, 15}};
}

void Scroll1(uint64_t layer_id)
{
    WinMoveRec(layer_id, false, Marginx + 4, Marginy + 4, Marginx + 4, Marginy + 4 + 16, 8 * kColumns, 16 * (kRows - 1));
    WinFillRectangle(layer_id, false, 4, 24 + 4 + 16 * cursory, (8 * kColumns), 16, 0);
    WinRedraw(layer_id);
}

void Print(uint64_t layer_id, char c)
{
    auto newline = [layer_id]()
    {
        cursorx = 0;
        if (cursory < kRows - 1)
        {
            ++cursory;
        }
        else
        {
            Scroll1(layer_id);
        }
    };

    if (c == '\n')
    {
        newline();
    }
    else
    {
        WinWriteChar(layer_id, true, CalcCursorPos().x, CalcCursorPos().y, 0xffffff, c);
        if (cursorx == kColumns - 1)
        {
            newline();
        }
        else
        {
            ++cursorx;
        }
    }
}

void Print(uint64_t layer_id, const char *s, std::optional<size_t> len)
{
    DrawCursor(layer_id, false);
    if (len)
    {
        for (size_t i = 0; i < *len; ++i)
        {
            Print(layer_id, *s);
            ++s;
        }
    }
    else
    {
        while (*s)
        {
            Print(layer_id, *s);
            ++s;
        }
    }
    DrawCursor(layer_id, true);
}

int PrintToTerminal(uint64_t layer_id, const char *format, ...)
{
    va_list ap;
    int result;
    char s[128];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    Print(layer_id, s);
    return result;
}

Rectangle<int> InputKey(
    uint64_t layer_id, uint8_t modifier, uint8_t keycode, char ascii)
{
    DrawCursor(layer_id, false);
    Rectangle<int> draw_area{CalcCursorPos(), {8 * 2, 16}};

    if (ascii == '\n')
    {

        linebuf_index_ = 0;
        cursorx = 0;

        if (cursory < kRows - 1)
        {
            ++cursory;
        }
        else
        {
            Scroll1(layer_id);
        }
    }
    else if (ascii == '\b')
    {
        if (cursorx > 0)
        {
            --cursorx;
            WinFillRectangle(layer_id, true, CalcCursorPos().x, CalcCursorPos().y, 8, 16, 0);
            draw_area.pos = CalcCursorPos();
            if (linebuf_index_ > 0)
            {
                --linebuf_index_;
            }
        }
    }
    else if (ascii != 0)
    {
        if (cursorx < kColumns - 1 && linebuf_index_ < kLineMax - 1)
        {

            ++linebuf_index_;
            WinWriteChar(layer_id, true, CalcCursorPos().x, CalcCursorPos().y, 0xffffff, ascii);
            ++cursorx;
        }
    }
    DrawCursor(layer_id, true);
    return draw_area;
};

/**
 * @brief ファイルサーバの最初の処理 BPBを読み取りファイル操作に必要な容量を確保する
 * 
 * @return Error 
 */
Error InitializeFat()
{
    auto [ret, err] = SyscallReadVolumeImage(&boot_volume_image, 0, 1);
    if (err)
    {
        return MAKE_ERROR(Error::sSyscallError);
    }

    // FAT32のみ対応 TODO:他のFATへの対応
    // ファイル操作に必要な容量を確保しておく
    if (boot_volume_image.total_sectors_16 == 0)
    {
        fat = reinterpret_cast<uint32_t *>(fat_buf);
        fat_file = reinterpret_cast<uint32_t *>(malloc(boot_volume_image.sectors_per_cluster *
                                                       SECTOR_SIZE));

        return MAKE_ERROR(Error::kSuccess);
    }
    else
    {
        return MAKE_ERROR(Error::sNotAcceptable);
    }
}

/**
 * @brief クラスタ番号を受け取りFATを読んで次のクラスタ番号を取得 エラーは0
 * 
 * @param cluster 
 * @return unsigned long 
 */
unsigned long NextCluster(
    unsigned long cluster)
{
    // クラスタ番号がFATの何ブロック目にあるか
    unsigned long sector_offset = cluster /
                                  (boot_volume_image.bytes_per_sector / sizeof(uint32_t));

    // 1ブロックだけ取得
    auto [ret, err] = SyscallReadVolumeImage(fat, boot_volume_image.reserved_sector_count + sector_offset, 1);
    if (err)
    {
        return 0;
    }

    // ブロックの先頭から何番目にあるか
    cluster = cluster - ((boot_volume_image.bytes_per_sector / sizeof(uint32_t)) * sector_offset);
    uint32_t next = fat[cluster];

    // クラスタチェーンの終わり
    if (next >= 0x0ffffff8ul)
    {
        return 0x0ffffffflu;
    }

    return next;
}

/**
 * @brief クラスタ番号から該当するクラスタを読む エラーはnullptr
 * 
 * @param cluster 
 * @return uint32_t* 
 */
uint32_t *ReadCluster(
    unsigned long cluster)
{
    unsigned long sector_offset =
        boot_volume_image.reserved_sector_count +
        boot_volume_image.num_fats * boot_volume_image.fat_size_32 +
        (cluster - 2) * boot_volume_image.sectors_per_cluster;

    auto [ret, err] = SyscallReadVolumeImage(fat_file, sector_offset, boot_volume_image.sectors_per_cluster);
    if (err)
    {
        return nullptr;
    }

    return fat_file;
}

bool NameIsEqual(
    DirectoryEntry &entry,
    const char *name)
{
    unsigned char name83[11];
    memset(name83, 0x20, sizeof(name83));

    int i = 0;
    int i83 = 0;
    for (; name[i] != 0 && i83 < sizeof(name83); ++i, ++i83)
    {
        if (name[i] == '.')
        {
            i83 = 7;
            continue;
        }
        name83[i83] = toupper(name[i]);
    }
    return memcmp(entry.name, name83, sizeof(name83)) == 0;
}

/**
 * @brief 名前と一致するファイルを探し、なければnullptrを返す
 * 
 * @param name 
 * @param directory_cluster デフォルトで0になっていて、0のときルートディレクトリから探す
 * @return DirectoryEntry* 
 */
DirectoryEntry *FindFile(
    const char *name,
    unsigned long directory_cluster)
{
    if (directory_cluster == 0)
    {
        directory_cluster = boot_volume_image.root_cluster;
    }

    while (directory_cluster != 0x0ffffffflu)
    {

        DirectoryEntry *dir = reinterpret_cast<DirectoryEntry *>(ReadCluster(directory_cluster));

        for (int i = 0;
             i < (boot_volume_image.bytes_per_sector * boot_volume_image.sectors_per_cluster) /
                     sizeof(DirectoryEntry);
             ++i)
        {
            if (NameIsEqual(dir[i], name))
            {
                return &dir[i];
            }
        }
        directory_cluster = NextCluster(directory_cluster);
    }

    return nullptr;
}

void ReadName(
    DirectoryEntry &entry,
    char *base, 
    char *ext)
{
    memcpy(base, &entry.name[0], 8);
    base[8] = 0;
    for (int i = 7; i >= 0 && base[i] == 0x20; --i)
    {
        base[i] = 0;
    }
    memcpy(ext, &entry.name[8], 3);
    ext[3] = 0;
    for (int i = 2; i >= 0 && ext[i] == 0x20; --i)
    {
        ext[i] = 0;
    }
}

extern "C" void main()
{
    int layer_id = OpenWindow(kColumns * 8 + 12 + Marginx, kRows * 16 + 12 + Marginy, 20, 20);
    if (layer_id == -1)
    {
        exit(1);
    }

    WinFillRectangle(layer_id, true, Marginx, Marginy, kCanvasWidth, kCanvasHeight, 0);

    SyscallWriteKernelLog("File System Server Ready...\n");

    InitializeFat();

    PrintToTerminal(layer_id, "%d\n", boot_volume_image.sectors_per_cluster * SECTOR_SIZE);

    // auto file_entry = FindFile("memmap");
    // if (!file_entry)
    // {
    //     Print(layer_id, "no such file\n");
    // }
    // else
    // {
    //     auto cluster = file_entry->FirstCluster();
    //     auto remain_bytes = file_entry->file_size;
    //     PrintToTerminal(layer_id, "next cluster %d\n", cluster);
    //     DrawCursor(layer_id, false);

    //     while (cluster != 0 && cluster != 0x0ffffffflu)
    //     {
    //         char *p = reinterpret_cast<char *>(ReadCluster(cluster));
    //         int i = 0;
    //         for (; i < boot_volume_image.bytes_per_sector * boot_volume_image.sectors_per_cluster &&
    //                i < remain_bytes;
    //              ++i)
    //         {
    //             Print(layer_id, *p);
    //             ++p;
    //         }
    //         remain_bytes -= i;
    //         cluster = NextCluster(cluster);
    //     }
    //     DrawCursor(layer_id, true);
    //     PrintToTerminal(layer_id, "remain bytes %d\n", remain_bytes);
    //     PrintToTerminal(layer_id, "next cluster %d\n", cluster);
    // }

    Message msg[1];

    while (true)
    {
        auto [n, err] = SyscallReceiveMessage(msg, 1);
        if (err)
        {
            printf("ReadEvent failed: %s\n", strerror(err));
            break;
        }
        if (msg[0].type == Message::aKeyPush)
        {
            if (msg[0].arg.keyboard.press)
            {

                const auto area = InputKey(layer_id,
                                           msg[0].arg.keyboard.modifier,
                                           msg[0].arg.keyboard.keycode,
                                           msg[0].arg.keyboard.ascii);
            }
        }
    }
}