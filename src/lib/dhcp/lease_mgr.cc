// Copyright (C) 2012 Internet Systems Consortium, Inc. ("ISC")
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

#include <dhcp/lease_mgr.h>
#include <exceptions/exceptions.h>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>

#include <time.h>

using namespace std;

using namespace isc::dhcp;

Lease6::Lease6(LeaseType type, const isc::asiolink::IOAddress& addr, DuidPtr duid,
               uint32_t iaid, uint32_t preferred, uint32_t valid, uint32_t t1,
               uint32_t t2, SubnetID subnet_id, uint8_t prefixlen)
    :type_(type), addr_(addr), prefixlen_(prefixlen), iaid_(iaid), duid_(duid),
     preferred_lft_(preferred), valid_lft_(valid), t1_(t1), t2_(t2),
     subnet_id_(subnet_id), fixed_(false), fqdn_fwd_(false),
     fqdn_rev_(false) {
    if (!duid) {
        isc_throw(InvalidOperation, "DUID must be specified for a lease");
    }

    cltt_ = time(NULL);
}

std::string LeaseMgr::getParameter(const std::string& name) const {
    ParameterMap::const_iterator param = parameters_.find(name);
    if (param == parameters_.end()) {
        isc_throw(BadValue, "Parameter not found");
    }
    return (param->second);
}

std::string
Lease6::toText() {
    ostringstream stream;

    stream << "Type:          " << static_cast<int>(type_) << " (";
    switch (type_) {
        case Lease6::LEASE_IA_NA:
            stream << "IA_NA)\n";
            break;
        case Lease6::LEASE_IA_TA:
            stream << "IA_TA)\n";
            break;
        case Lease6::LEASE_IA_PD:
            stream << "IA_PD)\n";
            break;
        default:
            stream << "unknown)\n";
    }
    stream << "Address:       " << addr_.toText() << "\n"
           << "Prefix length: " << static_cast<int>(prefixlen_) << "\n"
           << "IAID:          " << iaid_ << "\n"
           << "Pref life:     " << preferred_lft_ << "\n"
           << "Valid life:    " << valid_lft_ << "\n"
           << "Cltt:          " << cltt_ << "\n"
           << "Subnet ID:     " << subnet_id_ << "\n";

    return (stream.str());
}

bool
Lease6::operator==(const Lease6& other) const {
    return (
        type_ == other.type_ &&
        addr_ == other.addr_ &&
        prefixlen_ == other.prefixlen_ &&
        iaid_ == other.iaid_ &&
        *duid_ == *other.duid_ &&
        preferred_lft_ == other.preferred_lft_ &&
        valid_lft_ == other.valid_lft_ &&
        cltt_ == other.cltt_ &&
        subnet_id_ == other.subnet_id_
        );
}
