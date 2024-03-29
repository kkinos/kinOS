#pragma once

#include <stdlib.h>

#include "../../libs/common/message.hpp"

class FileDescriptor {
   public:
    virtual ~FileDescriptor() = default;
    virtual size_t Read(Message msg) = 0;
    virtual size_t Write(Message msg) = 0;
    virtual size_t Size() const = 0;

    virtual size_t Load(void* buf, size_t len, size_t offset) = 0;
    virtual void Close() = 0;
};

class FatFileDescriptor : public ::FileDescriptor {
   public:
    explicit FatFileDescriptor(uint64_t id, char* filename);
    size_t Read(Message msg) override;
    size_t Write(Message msg) override;
    size_t Size() const override { return 0; }
    size_t Load(void* buf, size_t len, size_t offset) override { return 0; }
    void Close() override { return; }

   private:
    uint64_t id_;
    char filename_[32];

    size_t rd_off_ =
        0;  //  for read file ,bytes offset, for read dir , index offset
    unsigned long rd_cluster_ = 0;
    size_t wr_off_ = 0;
};

class TerminalFileDescriptor : public FileDescriptor {
   public:
    explicit TerminalFileDescriptor(uint64_t id);
    size_t Read(Message msg) override;
    size_t Write(Message msg) override;
    size_t Size() const override { return 0; }
    size_t Load(void* buf, size_t len, size_t offset) override { return 0; }
    void Close() override { return; }

   private:
    uint64_t id_;
    uint64_t terminal_server_id_;
};

class PipeFileDescriptor : public FileDescriptor {
   public:
    explicit PipeFileDescriptor(uint64_t id);
    size_t Read(Message msg) override;
    size_t Write(Message msg) override;
    size_t Size() const override { return 0; }
    size_t Load(void* buf, size_t len, size_t offset) override { return 0; }
    void Close() override;

   private:
    uint64_t id_;
    char data_[16];
    size_t len_{0};
    size_t count_;
    bool closed_{false};
};