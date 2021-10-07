#include "fs.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

Vector2D<int> CalcCursorPos() {
    return kTopLeftMargin + Vector2D<int>{4 + 8 * cursorx, 4 + 16 * cursory};
}

void DrawCursor(uint64_t layer_id, bool visible) {
    const auto color = visible ? 0xffffff : 0;
    WinFillRectangle(layer_id, true, CalcCursorPos().x, CalcCursorPos().y, 7,
                     15, color);
}

Rectangle<int> BlinkCursor(uint64_t layer_id) {
    cursor_visible_ = !cursor_visible_;
    DrawCursor(layer_id, cursor_visible_);
    return {CalcCursorPos(), {7, 15}};
}

void Scroll1(uint64_t layer_id) {
    WinMoveRec(layer_id, false, Marginx + 4, Marginy + 4, Marginx + 4,
               Marginy + 4 + 16, 8 * kColumns, 16 * (kRows - 1));
    WinFillRectangle(layer_id, false, 4, 24 + 4 + 16 * cursory, (8 * kColumns),
                     16, 0);
    WinRedraw(layer_id);
}

void Print(uint64_t layer_id, char c) {
    auto newline = [layer_id]() {
        cursorx = 0;
        if (cursory < kRows - 1) {
            ++cursory;
        } else {
            Scroll1(layer_id);
        }
    };

    if (c == '\n') {
        newline();
    } else {
        WinWriteChar(layer_id, true, CalcCursorPos().x, CalcCursorPos().y,
                     0xffffff, c);
        if (cursorx == kColumns - 1) {
            newline();
        } else {
            ++cursorx;
        }
    }
}

void Print(uint64_t layer_id, const char *s, std::optional<size_t> len) {
    DrawCursor(layer_id, false);
    if (len) {
        for (size_t i = 0; i < *len; ++i) {
            Print(layer_id, *s);
            ++s;
        }
    } else {
        while (*s) {
            Print(layer_id, *s);
            ++s;
        }
    }
    DrawCursor(layer_id, true);
}

int PrintToTerminal(uint64_t layer_id, const char *format, ...) {
    va_list ap;
    int result;
    char s[128];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    Print(layer_id, s);
    return result;
}

Rectangle<int> InputKey(uint64_t layer_id, uint8_t modifier, uint8_t keycode,
                        char ascii) {
    DrawCursor(layer_id, false);
    Rectangle<int> draw_area{CalcCursorPos(), {8 * 2, 16}};

    if (ascii == '\n') {
        linebuf_index_ = 0;
        cursorx = 0;

        if (cursory < kRows - 1) {
            ++cursory;
        } else {
            Scroll1(layer_id);
        }
    } else if (ascii == '\b') {
        if (cursorx > 0) {
            --cursorx;
            WinFillRectangle(layer_id, true, CalcCursorPos().x,
                             CalcCursorPos().y, 8, 16, 0);
            draw_area.pos = CalcCursorPos();
            if (linebuf_index_ > 0) {
                --linebuf_index_;
            }
        }
    } else if (ascii != 0) {
        if (cursorx < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
            ++linebuf_index_;
            WinWriteChar(layer_id, true, CalcCursorPos().x, CalcCursorPos().y,
                         0xffffff, ascii);
            ++cursorx;
        }
    }
    DrawCursor(layer_id, true);
    return draw_area;
};

Error InitializeFat() {
    auto [ret, err] = SyscallReadVolumeImage(&boot_volume_image, 0, 1);
    if (err) {
        return MAKE_ERROR(Error::sSyscallError);
    }

    // FAT32のみ対応 TODO:他のFATへの対応
    // ファイル操作に必要な容量を確保しておく
    if (boot_volume_image.total_sectors_16 == 0) {
        fat = reinterpret_cast<uint32_t *>(fat_buf);
        fat_file = reinterpret_cast<uint32_t *>(
            malloc(boot_volume_image.sectors_per_cluster * SECTOR_SIZE));

        return MAKE_ERROR(Error::kSuccess);
    } else {
        return MAKE_ERROR(Error::sNotAcceptable);
    }
}

unsigned long NextCluster(unsigned long cluster) {
    // クラスタ番号がFATの何ブロック目にあるか
    unsigned long sector_offset =
        cluster / (boot_volume_image.bytes_per_sector / sizeof(uint32_t));

    // 1ブロックだけ取得
    auto [ret, err] = SyscallReadVolumeImage(
        fat, boot_volume_image.reserved_sector_count + sector_offset, 1);
    if (err) {
        return 0;
    }

    // ブロックの先頭から何番目にあるか
    cluster =
        cluster - ((boot_volume_image.bytes_per_sector / sizeof(uint32_t)) *
                   sector_offset);
    uint32_t next = fat[cluster];

    // クラスタチェーンの終わり
    if (next >= 0x0ffffff8ul) {
        return 0x0ffffffflu;
    }

    return next;
}

uint32_t *ReadCluster(unsigned long cluster) {
    unsigned long sector_offset =
        boot_volume_image.reserved_sector_count +
        boot_volume_image.num_fats * boot_volume_image.fat_size_32 +
        (cluster - 2) * boot_volume_image.sectors_per_cluster;

    auto [ret, err] = SyscallReadVolumeImage(
        fat_file, sector_offset, boot_volume_image.sectors_per_cluster);
    if (err) {
        return nullptr;
    }

    return fat_file;
}

bool NameIsEqual(DirectoryEntry &entry, const char *name) {
    unsigned char name83[11];
    memset(name83, 0x20, sizeof(name83));

    int i = 0;
    int i83 = 0;
    for (; name[i] != 0 && i83 < sizeof(name83); ++i, ++i83) {
        if (name[i] == '.') {
            i83 = 7;
            continue;
        }
        name83[i83] = toupper(name[i]);
    }
    return memcmp(entry.name, name83, sizeof(name83)) == 0;
}

std::pair<const char *, bool> NextPathElement(const char *path,
                                              char *path_elem) {
    const char *next_slash = strchr(path, '/');
    if (next_slash == nullptr) {
        strcpy(path_elem, path);
        return {nullptr, false};
    }

    const auto elem_len = next_slash - path;
    strncpy(path_elem, path, elem_len);
    path_elem[elem_len] = '\0';
    return {&next_slash[1], true};
}

std::pair<DirectoryEntry *, bool> FindFile(const char *path,
                                           unsigned long directory_cluster) {
    if (path[0] == '/') {
        directory_cluster = boot_volume_image.root_cluster;
        ++path;
    } else if (directory_cluster == 0) {
        directory_cluster = boot_volume_image.root_cluster;
    }

    char path_elem[13];
    const auto [next_path, post_slash] = NextPathElement(path, path_elem);
    const bool path_last = next_path == nullptr || next_path[0] == '\0';

    while (directory_cluster != 0x0ffffffflu) {
        DirectoryEntry *dir =
            reinterpret_cast<DirectoryEntry *>(ReadCluster(directory_cluster));

        for (int i = 0; i < (boot_volume_image.bytes_per_sector *
                             boot_volume_image.sectors_per_cluster) /
                                sizeof(DirectoryEntry);
             ++i) {
            if (dir[i].name == 0x00) {
                goto not_found;
            } else if (!NameIsEqual(dir[i], path_elem)) {
                continue;
            }

            if (dir[i].attr == Attribute::kDirectory && !path_last) {
                return FindFile(next_path, dir[i].FirstCluster());
            } else {
                return {&dir[i], post_slash};
            }
        }
        directory_cluster = NextCluster(directory_cluster);
    }
not_found:
    return {nullptr, post_slash};
}

void ReadName(DirectoryEntry &entry, char *base, char *ext) {
    memcpy(base, &entry.name[0], 8);
    base[8] = 0;
    for (int i = 7; i >= 0 && base[i] == 0x20; --i) {
        base[i] = 0;
    }
    memcpy(ext, &entry.name[8], 3);
    ext[3] = 0;
    for (int i = 2; i >= 0 && ext[i] == 0x20; --i) {
        ext[i] = 0;
    }
}

extern "C" void main() {
    int layer_id = OpenWindow(kColumns * 8 + 12 + Marginx,
                              kRows * 16 + 12 + Marginy, 20, 20);
    if (layer_id == -1) {
        exit(1);
    }

    WinFillRectangle(layer_id, true, Marginx, Marginy, kCanvasWidth,
                     kCanvasHeight, 0);

    InitializeFat();

    SyscallWriteKernelLog("[ fs ] Start\n");

    auto [am_id, err] = SyscallFindServer("servers/am");
    if (err) {
        SyscallWriteKernelLog(
            "[ fs ] cannot find file application management server\n");
    }

    // auto [file_entry, post_slash] = FindFile("app/cube");
    // if (!file_entry)
    // {
    //     Print(layer_id, "no such file or directory\n");
    // }
    // else
    // {
    //     Print(layer_id, "yes\n");
    //     // auto cluster = file_entry->FirstCluster();
    //     // auto remain_bytes = file_entry->file_size;
    //     // PrintToTerminal(layer_id, "next cluster %d\n", cluster);
    //     // DrawCursor(layer_id, false);

    //     // while (cluster != 0 && cluster != 0x0ffffffflu)
    //     // {
    //     //     char *p = reinterpret_cast<char *>(ReadCluster(cluster));
    //     //     int i = 0;
    //     //     for (; i < boot_volume_image.bytes_per_sector *
    //     boot_volume_image.sectors_per_cluster &&
    //     //            i < remain_bytes;
    //     //          ++i)
    //     //     {
    //     //         // Print(layer_id, *p);
    //     //         ++p;
    //     //     }
    //     //     remain_bytes -= i;
    //     //     cluster = NextCluster(cluster);
    //     // }
    //     // DrawCursor(layer_id, true);
    //     // PrintToTerminal(layer_id, "remain bytes %d\n", remain_bytes);
    //     // PrintToTerminal(layer_id, "next cluster %d\n", cluster);
    // }

    while (true) {
        SyscallOpenReceiveMessage(rmsg, 1);

        if (rmsg[0].type == Message::aKeyPush) {
            if (rmsg[0].arg.keyboard.press) {
                const auto area = InputKey(
                    layer_id, rmsg[0].arg.keyboard.modifier,
                    rmsg[0].arg.keyboard.keycode, rmsg[0].arg.keyboard.ascii);
            }
        }

        // amサーバからファイル実行のメッセージが来たとき
        if (rmsg[0].type == Message::aExecuteFile &&
            rmsg[0].src_task == am_id) {
            const char *path = rmsg[0].arg.executefile.filename;

            auto [file_entry, post_slash] = FindFile(path);

            // 指定されたファイルが無いとき
            if (!file_entry) {
                PrintToTerminal(layer_id, "no such file or directory\n");
                smsg[0] = rmsg[0];
                smsg[0].arg.executefile.exist = false;
                smsg[0].arg.executefile.directory = false;
                SyscallSendMessage(smsg, rmsg[0].src_task);
            }

            // 指定されたファイルがディレクトリのとき
            else if (file_entry->attr == Attribute::kDirectory) {
                PrintToTerminal(layer_id, "this is directory\n");
                smsg[0] = rmsg[0];
                smsg[0].arg.executefile.exist = true;
                smsg[0].arg.executefile.directory = true;
                SyscallSendMessage(smsg, rmsg[0].src_task);
            }

            // 指定されたファイルがあるとき
            else {
                // amサーバに新しいタスクを作成させる
                PrintToTerminal(layer_id, "%s exists\n", path);
                smsg[0] = rmsg[0];
                smsg[0].arg.executefile.exist = true;
                smsg[0].arg.executefile.directory = false;
                SyscallSendMessage(smsg, am_id);

                while (true) {
                    SyscallClosedReceiveMessage(rmsg, 1, am_id);

                    if (rmsg[0].type == Message::Error) {
                        if (rmsg[0].arg.error.retry) {
                            SyscallSendMessage(smsg, am_id);
                            SyscallWriteKernelLog("[ fs ] retry\n");
                        } else {
                            SyscallWriteKernelLog(
                                "fs: Error at other server\n");
                            break;
                        }
                    }

                    // 新しいタスクのidを用いてタスクバッファを拡大する
                    else if (rmsg[0].type == Message::aExecuteFile) {
                        PrintToTerminal(layer_id, "New Task ID is %d\n",
                                        rmsg[0].arg.executefile.id);

                        uint64_t id = rmsg[0].arg.executefile.id;
                        smsg[0].type = Message::kExpandTaskBuffer;
                        smsg[0].arg.expand.task_id = id;
                        smsg[0].arg.expand.bytes = file_entry->file_size;

                        SyscallSendMessage(smsg, 1);

                        while (true) {
                            SyscallClosedReceiveMessage(rmsg, 1, 1);

                            // タスクバッファに実行ファイルの内容をコピー
                            if (rmsg[0].type == Message::kExpandTaskBuffer) {
                                auto cluster = file_entry->FirstCluster();
                                auto remain_bytes = file_entry->file_size;
                                int offset = 0;

                                while (cluster != 0 &&
                                       cluster != 0x0ffffffflu) {
                                    char *p = reinterpret_cast<char *>(
                                        ReadCluster(cluster));
                                    int i = 0;
                                    for (; i < boot_volume_image
                                                       .bytes_per_sector *
                                                   boot_volume_image
                                                       .sectors_per_cluster &&
                                           i < remain_bytes;
                                         ++i) {
                                        auto [res, err] =
                                            SyscallCopyToTaskBuffer(id, p,
                                                                    offset, 1);
                                        if (err) {
                                            SyscallWriteKernelLog(
                                                "[ fs ] Syscall Error\n");
                                            smsg[0].type = Message::Error;
                                            smsg[0].arg.error.retry = false;
                                            SyscallSendMessage(smsg, am_id);
                                            break;
                                        }
                                        ++p;
                                        ++offset;
                                    }
                                    remain_bytes -= i;
                                    cluster = NextCluster(cluster);
                                }
                                // amサーバにコピーが完了したことを伝える
                                smsg[0].type = Message::Ready;
                                SyscallSendMessage(smsg, am_id);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
}