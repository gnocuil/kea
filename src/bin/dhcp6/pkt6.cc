// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>

#include "dhcp6/dhcp6.h"
#include "dhcp6/pkt6.h"
#include <iostream>

namespace isc {

///
/// constructor
///
/// \param dataLen - length of the data to be allocated
///
Pkt6::Pkt6(int dataLen)
    :local_addr_("::"),
     remote_addr_("::") {
    try {
	data_ = boost::shared_array<char>(new char[dataLen]);
	data_len_ = dataLen;
    } catch (const std::exception&) {
	// TODO move to LOG_FATAL()
	// let's continue with empty pkt for now
        std::cout << "Failed to allocate " << dataLen << " bytes."
                  << std::endl;
        data_len_ = 0;
    }
}

Pkt6::~Pkt6() {
    // no need to delete anything shared_ptr will take care of data_
}

};
