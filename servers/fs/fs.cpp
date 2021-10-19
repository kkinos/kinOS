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
        return MAKE_ERROR(Error::kSyscallError);
    }

    // supports only FAT32 TODO:supports other fat
    if (boot_volume_image.total_sectors_16 == 0) {
        fat = reinterpret_cast<uint32_t *>(fat_buf);
        fat_file = reinterpret_cast<uint32_t *>(
            malloc(boot_volume_image.sectors_per_cluster * SECTOR_SIZE));

        return MAKE_ERROR(Error::kSuccess);
    } else {
        return MAKE_ERROR(Error::kNotAcceptable);
    }
}

unsigned long NextCluster(unsigned long cluster) {
    unsigned long sector_offset =
        cluster / (boot_volume_image.bytes_per_sector / sizeof(uint32_t));

    // copy 1 block
    auto [ret, err] = SyscallReadVolumeImage(
        fat, boot_volume_image.reserved_sector_count + sector_offset, 1);
    if (err) {
        return 0;
    }

    cluster =
        cluster - ((boot_volume_image.bytes_per_sector / sizeof(uint32_t)) *
                   sector_offset);
    uint32_t next = fat[cluster];

    // end of cluster chain
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

void ProcessAccordingToMessage(Message *msg, uint64_t am_id) {
    switch (msg->type) {
        case Message::kExecuteFile: {
            const char *path = received_message[0].arg.executefile.filename;

            auto [file_entry, post_slash] = FindFile(path);
            // the file doesn't exist
            if (!file_entry) {
                sent_message[0] = *msg;
                sent_message[0].arg.executefile.exist = false;
                sent_message[0].arg.executefile.directory = false;
                SyscallSendMessage(sent_message, msg->src_task);
                goto end;
            }
            // the file is a directory
            else if (file_entry->attr == Attribute::kDirectory) {
                sent_message[0] = *msg;
                sent_message[0].arg.executefile.exist = true;
                sent_message[0].arg.executefile.directory = true;
                SyscallSendMessage(sent_message, msg->src_task);
                goto end;
            }
            // the file exists and not a directory
            else {
                sent_message[0] = *msg;
                sent_message[0].arg.executefile.exist = true;
                sent_message[0].arg.executefile.directory = false;
                SyscallSendMessage(sent_message, msg->src_task);
                ExpandTaskBuffer(am_id, file_entry);
                goto end;
            }
        }
        default:
            SyscallWriteKernelLog("[ fs ] Unknown message type \n");
            goto end;
    }

end:
    return;
}

void ExpandTaskBuffer(uint64_t am_id, DirectoryEntry *file_entry) {
    while (true) {
        SyscallClosedReceiveMessage(received_message, 1, am_id);
        switch (received_message[0].type) {
            case Message::kError:
                if (received_message[0].arg.error.retry) {
                    SyscallSendMessage(sent_message, am_id);
                    SyscallWriteKernelLog("[ fs ] retry\n");
                    break;
                } else {
                    SyscallWriteKernelLog("[ fs ] error at am server\n");
                    goto end;
                }
            case Message::kExecuteFile: {
                // message to kernel to expand task buffer
                uint64_t id = received_message[0].arg.executefile.id;
                sent_message[0].type = Message::kExpandTaskBuffer;
                sent_message[0].arg.expand.id = id;
                sent_message[0].arg.expand.bytes = file_entry->file_size;
                SyscallSendMessage(sent_message, 1);
                CopyToTaskBuffer(id, am_id, file_entry);
                goto end;
            }

            default:
                SyscallWriteKernelLog(
                    "[ fs ] Unknown message type from am server\n");
                goto end;
        }
    }
end:
    return;
}

void CopyToTaskBuffer(uint64_t id, uint64_t am_id, DirectoryEntry *file_entry) {
    while (true) {
        SyscallClosedReceiveMessage(received_message, 1, 1);

        switch (received_message[0].type) {
            case Message::kError:
                if (received_message[0].arg.error.retry) {
                    SyscallSendMessage(sent_message, 1);
                    SyscallWriteKernelLog("[ fs ] retry\n");
                    break;
                } else {
                    SyscallWriteKernelLog("[ fs ] error at kernel\n");
                    goto end;
                }
            // copy file to task buffer
            case Message::kExpandTaskBuffer: {
                auto cluster = file_entry->FirstCluster();
                auto remain_bytes = file_entry->file_size;
                int offset = 0;

                while (cluster != 0 && cluster != 0x0ffffffflu) {
                    char *p = reinterpret_cast<char *>(ReadCluster(cluster));
                    int i = 0;
                    for (; i < boot_volume_image.bytes_per_sector *
                                   boot_volume_image.sectors_per_cluster &&
                           i < remain_bytes;
                         ++i) {
                        auto [res, err] =
                            SyscallCopyToTaskBuffer(id, p, offset, 1);
                        if (err) {
                            SyscallWriteKernelLog("[ fs ] Syscall error\n");
                            sent_message[0].type = Message::kError;
                            sent_message[0].arg.error.retry = false;
                            SyscallSendMessage(sent_message, am_id);
                            goto end;
                        }
                        ++p;
                        ++offset;
                    }
                    remain_bytes -= i;
                    cluster = NextCluster(cluster);
                }
                sent_message[0].type = Message::kReady;
                SyscallSendMessage(sent_message, am_id);
                goto end;
            }

            default:
                goto end;
        }
    }
end:
    return;
}

extern "C" void main() {
    int layer_id = OpenWindow(kColumns * 8 + 12 + Marginx,
                              kRows * 16 + 12 + Marginy, 20, 20, "fs");
    if (layer_id == -1) {
        exit(1);
    }
    WinFillRectangle(layer_id, true, Marginx, Marginy, kCanvasWidth,
                     kCanvasHeight, 0);

    InitializeFat();
    SyscallWriteKernelLog("[ fs ] ready\n");
    auto [am_id, err] = SyscallFindServer("servers/am");
    if (err) {
        SyscallWriteKernelLog(
            "[ fs ] cannot find file application management server\n");
    }

    while (true) {
        SyscallOpenReceiveMessage(received_message, 1);
        if (received_message[0].type == Message::kKeyPush) {
            if (received_message[0].arg.keyboard.press) {
                const auto area = InputKey(
                    layer_id, received_message[0].arg.keyboard.modifier,
                    received_message[0].arg.keyboard.keycode,
                    received_message[0].arg.keyboard.ascii);
            }
        }

        // message from am server
        if (received_message[0].src_task == am_id) {
            ProcessAccordingToMessage(received_message, am_id);
        }
    }
}