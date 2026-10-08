// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::socket {
    /**
     * @brief h_errno-style error codes returned alongside DNS resolver results, as defined by the sfdnsres protocol
     * @url https://switchbrew.org/wiki/Sockets_DNS_services
     */
    enum class NetDbError : i32 {
        Internal = -1,
        Success = 0,
        HostNotFound = 1,
        TryAgain = 2,
        NoRecovery = 3,
        NoData = 4,
    };

    /**
     * @url https://switchbrew.org/wiki/Sockets_DNS_services#sfdnsres
     */
    class IResolver : public BaseService {
      public:
        IResolver(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Resolves a host name with options (5.0.0+)
         * @url https://switchbrew.org/wiki/Sockets_DNS_services#GetAddrInfoRequestWithOptions
         */
        Result GetAddrInfoRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0xC, IResolver, GetAddrInfoRequestWithOptions)
        )
    };
}
