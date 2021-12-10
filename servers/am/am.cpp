#include "am.hpp"

#include <cstdio>

#include "../../libs/kinos/common/print.hpp"

ApplicationManagementServer::ApplicationManagementServer() {}

void ApplicationManagementServer::Initialize() {
    app_manager_ = new AppManager;

    state_pool_.emplace_back(new ErrState(this));
    state_pool_.emplace_back(new InitState(this));
    state_pool_.emplace_back(new ExecFileState(this));
    state_pool_.emplace_back(new CreateTaskState(this));
    state_pool_.emplace_back(new StartTaskState(this));
    state_pool_.emplace_back(new RedirectState(this));
    state_pool_.emplace_back(new PipeState(this));
    state_pool_.emplace_back(new ExitState(this));
    state_pool_.emplace_back(new OpenState(this));
    state_pool_.emplace_back(new AllocateFDState(this));
    state_pool_.emplace_back(new ReadState(this));
    state_pool_.emplace_back(new WriteState(this));
    state_pool_.emplace_back(new WaitingKeyState(this));

    state_ = GetServerState(State::StateInit);

    Print("[ am ] ready\n");
}

extern "C" void main() {
    server = new ApplicationManagementServer;
    server->Initialize();

    while (true) {
        server->ReceiveMessage();
        server->HandleMessage();
        server->SendMessage();
    }
}

ApplicationManagementServer* server;