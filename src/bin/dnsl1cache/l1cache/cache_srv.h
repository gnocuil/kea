// Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifndef __CACHE_SRV_H
#define __CACHE_SRV_H 1

#include <dns/rrclass.h>

#include <config/ccsession.h>

#include <dnsl1cache/app_runner.h>

#include <vector>

namespace isc {
namespace dnsl1cache {

class DNSCacheSrv {
public:
    DNSCacheSrv();
    AppConfigHandler getConfigHandler();
    AppCommandHandler getCommandHandler();
    std::vector<RemoteConfigInfo> getRemoteHandlers();

private:
    data::ConstElementPtr configHandler(config::ModuleCCSession& cc_session,
                                        data::ConstElementPtr new_config);
    data::ConstElementPtr commandHandler(config::ModuleCCSession& cc_session,
                                         const std::string& command,
                                         data::ConstElementPtr args);
};

} // namespace dnsl1cache
} // namespace isc
#endif // __CACHE_SRV_H

// Local Variables:
// mode: c++
// End:
