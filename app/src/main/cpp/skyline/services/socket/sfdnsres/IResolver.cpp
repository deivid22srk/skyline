// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IResolver.h"

namespace skyline::service::socket {
    IResolver::IResolver(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IResolver::GetAddrInfoRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // Adapted from Strato (services/socket/sfdnsres/IResolver.cpp, commit
        // 43195927, MPL-2.0) offline path: DNS resolution is unavailable, so
        // the guest receives an immediate clean failure (resultCode -1,
        // NetDbError::Internal, dataSize 0) instead of the zeroed TLS buffer
        // it previously got for this missing command - which made UE4's
        // FOnlineSubsystemSwitch retry DNS on parallel worker threads.
        Logger::Info("sfdnsres::IResolver::GetAddrInfoRequestWithOptions: DNS unavailable, returning resolver failure");
        response.Push<i32>(-1);                          // resultCode (getaddrinfo return value)
        response.Push(NetDbError::Internal);             // NetDBErrorCode
        response.Push<u32>(0);                           // serialized addrinfo size
        response.Push<u32>(0);                           // cancel handle
        return {};
    }
}
