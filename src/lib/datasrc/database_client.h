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

#ifndef __DATABASE_CLIENT_H
#define __DATABASE_CLIENT_H 1

#include <datasrc/client.h>

namespace isc {
namespace datasrc {
class DataBaseConnection;

class DataBaseDataSourceClient : public DataSourceClient {
public:
    DataBaseDataSourceClient();
    virtual void open(const std::string& param);
    virtual FindResult findZone(const isc::dns::Name& name) const;
    virtual ZoneIteratorPtr createZoneIterator(const isc::dns::Name&) const;
private:
    std::string param_;
    DataBaseConnection* conn_;
};
}
}

#endif  // __DATABASE_CLIENT_H

// Local Variables:
// mode: c++
// End:
