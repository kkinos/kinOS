#pragma once

class FileSystemServer;

enum State {
    StateErr,
    StateInit,
    StateExecFile,
    StateExpandBuffer,
    StateCopyToBuffer,
    StateOpen,
    StateRead,
    StateWrite,
};

enum Target {
    File,
    Dir,
};

class ServerState {
   public:
    virtual ~ServerState() = default;
    virtual ServerState *ReceiveMessage() = 0;
    virtual ServerState *HandleMessage() = 0;
    virtual ServerState *SendMessage() = 0;
};

class ErrState : public ::ServerState {
   public:
    explicit ErrState(FileSystemServer *server) { server_ = server; }
    ServerState *ReceiveMessage() override { return this; }
    ServerState *HandleMessage() override { return this; }
    ServerState *SendMessage() override;

   private:
    FileSystemServer *server_;
};

class InitState : public ::ServerState {
   public:
    explicit InitState(FileSystemServer *server) { server_ = server; }
    ServerState *ReceiveMessage() override;
    ServerState *HandleMessage() override { return this; }
    ServerState *SendMessage() override;

   private:
    FileSystemServer *server_;
};

class ExecFileState : public ::ServerState {
   public:
    explicit ExecFileState(FileSystemServer *server) { server_ = server; }
    ServerState *ReceiveMessage() override;
    ServerState *HandleMessage() override;
    ServerState *SendMessage() override;

   private:
    FileSystemServer *server_;
};

class ExpandBufferState : public ::ServerState {
   public:
    explicit ExpandBufferState(FileSystemServer *server) { server_ = server; }
    ServerState *ReceiveMessage() override;
    ServerState *HandleMessage() override;
    ServerState *SendMessage() override;

   private:
    FileSystemServer *server_;
};

class CopyToBufferState : public ::ServerState {
   public:
    explicit CopyToBufferState(FileSystemServer *server) { server_ = server; }
    ServerState *ReceiveMessage() override { return this; }
    ServerState *HandleMessage() override;
    ServerState *SendMessage() override;

   private:
    FileSystemServer *server_;
};

class OpenState : public ::ServerState {
   public:
    explicit OpenState(FileSystemServer *server) { server_ = server; }
    ServerState *ReceiveMessage() override { return this; }
    ServerState *HandleMessage() override;
    ServerState *SendMessage() override { return this; }

    void SetTarget(Target target) { target_ = target; }

   private:
    FileSystemServer *server_;
    Target target_;
};

class ReadState : public ::ServerState {
   public:
    explicit ReadState(FileSystemServer *server) { server_ = server; }
    ServerState *ReceiveMessage() override { return this; }
    ServerState *HandleMessage() override;
    ServerState *SendMessage() override { return this; }

   private:
    FileSystemServer *server_;
};

class WriteState : public ::ServerState {
   public:
    explicit WriteState(FileSystemServer *server) { server_ = server; }
    ServerState *ReceiveMessage() override { return this; }
    ServerState *HandleMessage() override;
    ServerState *SendMessage() override;

   private:
    FileSystemServer *server_;
};