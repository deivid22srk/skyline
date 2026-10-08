// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IScanRequest.h"
#include "IRequest.h"
#include "IGeneralService.h"

namespace skyline::service::nifm {
    IGeneralService::IGeneralService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IGeneralService::CreateScanRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IScanRequest), session, response);
        return {};
    }

    Result IGeneralService::CreateRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IRequest), session, response);
        return {};
    }

    Result IGeneralService::GetCurrentIpAddress(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::NoInternetConnection;
    }

    Result IGeneralService::GetInternetConnectionStatus(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct Status {
            u8 type{1};
            u8 wifiStrength{3};
            u8 state{4};
        } status{};
        response.Push(status);
        return {};
    }

    Result IGeneralService::IsAnyInternetRequestAccepted(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // We don't emulate networking so always return false
        response.Push(false);
        return {};
    }
}
