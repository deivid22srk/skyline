// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IClient.h"
#include <cerrno>

namespace skyline::service::socket {
    IClient::IClient(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IClient::PushBsdResult(ipc::IpcResponse &response, i32 result, i32 errorCode) {
        // Ported from Strato (services/socket/bsd/IClient.cpp, commit 20a9ab65,
        // MPL-2.0): the bsd protocol returns (i32 result, i32 errno) pairs where
        // result == -1 signals failure and errno carries the POSIX reason
        if (errorCode != 0)
            result = -1;

        response.Push<i32>(result);
        response.Push<i32>(errorCode);
        return {};
    }

    Result IClient::RegisterClient(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0);
        return {};
    }

    Result IClient::StartMonitoring(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::Select(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::Socket(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // CB4 (UE4 FOnlineSubsystemSwitch) calls Socket during online init and
        // previously received a zeroed TLS buffer (fd = 0, "success") because
        // the command was missing, leaving the online subsystem in an
        // undefined retrying state. Return a proper BSD failure instead so
        // offline init fails fast and gracefully. ENETDOWN (100) is a Linux
        // errno value as the bsd service protocol uses Linux errno numbering
        // (https://switchbrew.org/wiki/Sockets_services). Real sockets are
        // intentionally not created: the remaining socket operations in this
        // service are stubs that do not carry real traffic.
        Logger::Info("bsd::IClient::Socket: network unavailable, returning ENETDOWN");
        return PushBsdResult(response, -1, ENETDOWN);
    }

    Result IClient::Fcntl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // No real sockets can exist (Socket always fails), so any fd the guest
        // attempts to manipulate is invalid by definition
        Logger::Info("bsd::IClient::Fcntl: returning EBADF");
        return PushBsdResult(response, -1, EBADF);
    }

    Result IClient::Poll(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::Recv(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::RecvFrom(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::Send(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::SendTo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0);
        return {};
    }

    Result IClient::Accept(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::Bind(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::Connect(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::Listen(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::SetSockOpt(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::Shutdown(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::ShutdownAllSockets(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::Write(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IClient::Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0);
        response.Push<u32>(0);
        return {};
    }

    Result IClient::Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
