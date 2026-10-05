// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IManager.h"

namespace skyline::service::socket {
    IManager::IManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IManager::UnknownCommand(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // The semantics of nsd command 0x15 are unverified, stub it out with a success response so callers proceed
        Logger::Debug("nsd IManager command 0x{:X} stubbed", request.payload->value);
        return {};
    }
}
