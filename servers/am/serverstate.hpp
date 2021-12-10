#pragma once

enum State {
    StateErr,
    StateInit,
    StateExecFile,
    StateCreateTask,
    StateStartTask,
    StateRedirect,
    StatePipe,
    StateExit,
    StateOpen,
    StateAllcateFD,
    StateRead,
    StateWrite,
    StateWaitingKey,
    StateSendEvent,
};

enum Target {
    File,
    Dir,
};

class ApplicationManagementServer;

class ServerState {
   public:
    virtual ~ServerState() = default;
    virtual ServerState* ReceiveMessage() = 0;
    virtual ServerState* HandleMessage() = 0;
    virtual ServerState* SendMessage() = 0;
};

class ErrState : public ::ServerState {
   public:
    explicit ErrState(ApplicationManagementServer* server) { server_ = server; }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override { return this; }
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class InitState : public ::ServerState {
   public:
    explicit InitState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override { return this; }
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class ExecFileState : public ::ServerState {
   public:
    explicit ExecFileState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class CreateTaskState : public ::ServerState {
   public:
    explicit CreateTaskState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class StartTaskState : public ::ServerState {
   public:
    explicit StartTaskState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class RedirectState : public ::ServerState {
   public:
    explicit RedirectState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class PipeState : public ::ServerState {
   public:
    explicit PipeState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override { return this; }
    ServerState* SendMessage() override { return this; }

   private:
    ApplicationManagementServer* server_;
};

class ExitState : public ::ServerState {
   public:
    explicit ExitState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class OpenState : public ::ServerState {
   public:
    explicit OpenState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

    void SetTarget(Target target) { target_ = target; }

   private:
    ApplicationManagementServer* server_;
    Target target_;
};

class AllocateFDState : public ::ServerState {
   public:
    explicit AllocateFDState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override { return this; }

    void SetTarget(Target target) { target_ = target; }

   private:
    ApplicationManagementServer* server_;
    Target target_;
};

class ReadState : public ::ServerState {
   public:
    explicit ReadState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

    void SetTarget(Target target) { target_ = target; }

   private:
    ApplicationManagementServer* server_;
    Target target_;
};

class WriteState : public ::ServerState {
   public:
    explicit WriteState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override { return this; }
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};

class WaitingKeyState : public ::ServerState {
   public:
    explicit WaitingKeyState(ApplicationManagementServer* server) {
        server_ = server;
    }
    ServerState* ReceiveMessage() override;
    ServerState* HandleMessage() override;
    ServerState* SendMessage() override;

   private:
    ApplicationManagementServer* server_;
};