// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::socket {
    /**
     * @url https://switchbrew.org/wiki/Sockets_services#nsd:u.2C_nsd:a
     */
    class IManager : public BaseService {
      public:
        IManager(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Stub for unimplemented nsd commands (such as 0x15), returns success with no payload
         * @note Sifu (UE4) calls command 0x15 during network subsystem initialisation, unimplemented commands are non-fatal but produce log spam
         */
        Result UnknownCommand(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x15, IManager, UnknownCommand)
        )
    };
}
