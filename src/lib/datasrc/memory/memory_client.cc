// Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
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

#include <exceptions/exceptions.h>

#include <datasrc/memory/memory_client.h>
#include <datasrc/memory/logger.h>
#include <datasrc/memory/zone_data.h>
#include <datasrc/memory/rdataset.h>
#include <datasrc/memory/segment_object_holder.h>
#include <datasrc/memory/treenode_rrset.h>
#include <datasrc/memory/zone_finder.h>
#include <datasrc/memory/zone_data_loader.h>

#include <util/memory_segment_local.h>
#include <util/memory_segment_mmap.h>

#include <datasrc/data_source.h>
#include <datasrc/factory.h>
#include <datasrc/result.h>

#include <dns/name.h>
#include <dns/rdataclass.h>
#include <dns/rrclass.h>

#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <utility>
#include <cctype>
#include <cassert>
#include <fstream>

#include <unistd.h>

using namespace std;
using namespace isc::dns;
using namespace isc::dns::rdata;
using namespace isc::datasrc::memory;

namespace isc {
namespace datasrc {
namespace memory {

using detail::SegmentObjectHolder;

namespace { // unnamed namespace

// A helper internal class used by the memory client, used for deleting
// filenames stored in an internal tree.
class FileNameDeleter {
public:
    FileNameDeleter() {}

    void operator()(std::string* filename) const {
        delete filename;
    }
};

} // end of unnamed namespace

InMemoryClient::InMemoryClient(util::MemorySegment& mem_sgmt,
                               RRClass rrclass) :
    mem_sgmt_(mem_sgmt),
    rrclass_(rrclass),
    zone_count_(0),
    mmap_sgmt_(NULL),
    mapped_file_version_(-1)
{
    SegmentObjectHolder<ZoneTable, RRClass> holder(
        mem_sgmt_, ZoneTable::create(mem_sgmt_, rrclass), rrclass_);

    file_name_tree_ = FileNameTree::create(mem_sgmt_, false);

    zone_table_ = holder.release();
}

InMemoryClient::~InMemoryClient() {
    FileNameDeleter deleter;
    FileNameTree::destroy(mem_sgmt_, file_name_tree_, deleter);
    if (mmap_sgmt_ != NULL) {
        delete mmap_sgmt_; // just unmap it, we shouldn't delete the content
    } else if (zone_table_ != NULL) {
        ZoneTable::destroy(mem_sgmt_, zone_table_, rrclass_);
    }
}

result::Result
InMemoryClient::loadInternal(util::MemorySegment& load_mem_sgmt,
                             const isc::dns::Name& zone_name,
                             const std::string& filename,
                             ZoneData* zone_data)
{
    SegmentObjectHolder<ZoneData, RRClass> holder(
        load_mem_sgmt, zone_data, rrclass_);

    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_MEMORY_MEM_ADD_ZONE).
        arg(zone_name).arg(rrclass_);

    // Set the filename in file_name_tree_ now, so that getFileName()
    // can use it (during zone reloading).
    FileNameNode* node(NULL);
    // Note that this should be me mem_sgmt_, not load_mem_sgmt
    switch (file_name_tree_->insert(mem_sgmt_, zone_name, &node)) {
    case FileNameTree::SUCCESS:
    case FileNameTree::ALREADYEXISTS:
        // These are OK
        break;
    default:
        // Can Not Happen
        assert(false);
    }
    // node must point to a valid node now
    assert(node != NULL);

    const std::string* tstr = node->setData(new std::string(filename));
    delete tstr;

    const ZoneTable::AddResult result(zone_table_->addZone(load_mem_sgmt,
                                                           rrclass_, zone_name,
                                                           holder.release()));
    if (result.code == result::SUCCESS) {
        // Only increment the zone count if the zone doesn't already
        // exist.
        ++zone_count_;
    }
    // Destroy the old instance of the zone if there was any
    if (result.zone_data != NULL) {
        ZoneData::destroy(load_mem_sgmt, result.zone_data, rrclass_);
    }

    return (result.code);
}

RRClass
InMemoryClient::getClass() const {
    return (rrclass_);
}

unsigned int
InMemoryClient::getZoneCount() const {
    return (zone_count_);
}

isc::datasrc::DataSourceClient::FindResult
InMemoryClient::findZone(const isc::dns::Name& zone_name) const {
    LOG_DEBUG(logger, DBG_TRACE_DATA,
              DATASRC_MEMORY_MEM_FIND_ZONE).arg(zone_name);

    if (zone_table_ == NULL) {
        isc_throw(DataSourceError, "in-memory zone table is NULL");
    }

    ZoneTable::FindResult result(zone_table_->findZone(zone_name));

    ZoneFinderPtr finder;
    if (result.code != result::NOTFOUND) {
        finder.reset(new InMemoryZoneFinder(*result.zone_data, getClass()));
    }

    return (DataSourceClient::FindResult(result.code, finder));
}

const ZoneData*
InMemoryClient::findZoneData(const isc::dns::Name& zone_name) {
    ZoneTable::FindResult result(zone_table_->findZone(zone_name));
    return (result.zone_data);
}

result::Result
InMemoryClient::load(const isc::dns::Name& zone_name,
                     const std::string& filename)
{
    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_MEMORY_MEM_LOAD).arg(zone_name).
        arg(filename);

    util::MemorySegment& mem_sgmt =
        (mmap_sgmt_ != NULL) ? *mmap_sgmt_ : mem_sgmt_;
    ZoneData* zone_data = loadZoneData(mem_sgmt, rrclass_, zone_name,
                                       filename);
    return (loadInternal(mem_sgmt, zone_name, filename, zone_data));
}

result::Result
InMemoryClient::load(const isc::dns::Name& zone_name, ZoneIterator& iterator) {
    util::MemorySegment& mem_sgmt =
        (mmap_sgmt_ != NULL) ? *mmap_sgmt_ : mem_sgmt_;
    ZoneData* zone_data = loadZoneData(mem_sgmt, rrclass_, zone_name,
                                       iterator);
    return (loadInternal(mem_sgmt, zone_name, string(), zone_data));
}

template <typename SourceType>
result::Result
InMemoryClient::loadNewMap(const isc::dns::Name& zone_name,
                           SourceType source, const std::string& filename)
{
    if (mmap_sgmt_ == NULL) {
        isc_throw(isc::InvalidParameter, "loadNewMap without mmap segment");
    }

    // Copy the current version of mapped file to a new one.
    const std::string current_fname = mmap_file_ + "." +
        boost::lexical_cast<std::string>(mapped_file_version_);
    const int new_version = (mapped_file_version_ + 1) % 2; // 0=>1, 1=>0
    const std::string new_fname = mmap_file_ + "." +
        boost::lexical_cast<std::string>(new_version);
    unlink(new_fname.c_str());
    ifstream f_in(current_fname.c_str(), fstream::binary);
    ofstream f_out(new_fname.c_str(), fstream::binary | fstream::trunc);
    f_out << f_in.rdbuf();

    // Create a new mapped memory segment in the non-create mode.
    util::MemorySegmentMmap* new_mem_sgmt =
        new util::MemorySegmentMmap(new_fname, false);
    ZoneTable* new_zone_table = static_cast<ZoneTable*>(
        new_mem_sgmt->getNamedAddress("zone_table"));

    // Remove old version of zone data first.  This will be a bit more
    // space efficient, but a bit more tricky.
    ZoneData* empty_zone_data = ZoneData::create(*new_mem_sgmt, zone_name);
    const ZoneTable::AddResult result(new_zone_table->addZone(
                                          *new_mem_sgmt, rrclass_, zone_name,
                                          empty_zone_data));
    if (result.zone_data != NULL) {
        ZoneData::destroy(*new_mem_sgmt, result.zone_data, rrclass_);
    }

    // Then create a new version of zone data, link it into the new table.
    ZoneData* zone_data = loadZoneData(*new_mem_sgmt, rrclass_, zone_name,
                                       source);
    zone_table_ = new_zone_table;
    const result::Result load_result =
        loadInternal(*new_mem_sgmt, zone_name, filename, zone_data);

    delete mmap_sgmt_;
    mmap_sgmt_ = new_mem_sgmt;
    mapped_file_version_ = new_version;

    return (load_result);
}

result::Result
InMemoryClient::loadNewMap(const isc::dns::Name& zone_name,
                           const std::string& filename)
{
    return (loadNewMap(zone_name, filename, filename));
}

result::Result
InMemoryClient::loadNewMap(const isc::dns::Name& zone_name,
                           ZoneIterator& iterator)
{
    return (loadNewMap<ZoneIterator&>(zone_name, iterator, std::string()));
}

const std::string
InMemoryClient::getFileName(const isc::dns::Name& zone_name) const {
    const FileNameNode* node(NULL);
    const FileNameTree::Result result = file_name_tree_->find(zone_name,
                                                              &node);
    if (result == FileNameTree::EXACTMATCH) {
        return (*node->getData());
    } else {
        return (std::string());
    }
}

namespace {

class MemoryIterator : public ZoneIterator {
private:
    ZoneChain chain_;
    const RdataSet* set_node_;
    const RRClass rrclass_;
    const ZoneTree& tree_;
    const ZoneNode* node_;
    // Only used when separate_rrs_ is true
    ConstRRsetPtr rrset_;
    RdataIteratorPtr rdata_iterator_;
    bool separate_rrs_;
    bool ready_;
public:
    MemoryIterator(const RRClass rrclass,
                   const ZoneTree& tree, const Name& origin,
                   bool separate_rrs) :
        rrclass_(rrclass),
        tree_(tree),
        separate_rrs_(separate_rrs),
        ready_(true)
    {
        // Find the first node (origin) and preserve the node chain for future
        // searches
        ZoneTree::Result result(tree_.find(origin, &node_, chain_));
        // It can't happen that the origin is not in there
        if (result != ZoneTree::EXACTMATCH) {
            isc_throw(Unexpected,
                      "In-memory zone corrupted, missing origin node");
        }
        // Initialize the iterator if there's somewhere to point to
        if (node_ != NULL && node_->getData() != NULL) {
            set_node_ = node_->getData();
            if (separate_rrs_ && set_node_ != NULL) {
                rrset_.reset(new TreeNodeRRset(rrclass_,
                                               node_, set_node_, true));
                rdata_iterator_ = rrset_->getRdataIterator();
            }
        }
    }

    virtual ConstRRsetPtr getNextRRset() {
        if (!ready_) {
            isc_throw(Unexpected, "Iterating past the zone end");
        }
        /*
         * This cycle finds the first nonempty node with yet unused
         * RdataSset.  If it is NULL, we run out of nodes. If it is
         * empty, it doesn't contain any RdataSets. If we are at the
         * end, just get to next one.
         */
        while (node_ != NULL &&
               (node_->getData() == NULL || set_node_ == NULL)) {
            node_ = tree_.nextNode(chain_);
            // If there's a node, initialize the iterator and check next time
            // if the map is empty or not
            if (node_ != NULL && node_->getData() != NULL) {
                set_node_ = node_->getData();
                // New RRset, so get a new rdata iterator
                if (separate_rrs_ && set_node_ != NULL) {
                    rrset_.reset(new TreeNodeRRset(rrclass_,
                                                   node_, set_node_, true));
                    rdata_iterator_ = rrset_->getRdataIterator();
                }
            }
        }
        if (node_ == NULL) {
            // That's all, folks
            ready_ = false;
            return (ConstRRsetPtr());
        }

        if (separate_rrs_) {
            // For separate rrs, reconstruct a new RRset with just the
            // 'current' rdata
            RRsetPtr result(new RRset(rrset_->getName(),
                                      rrset_->getClass(),
                                      rrset_->getType(),
                                      rrset_->getTTL()));
            result->addRdata(rdata_iterator_->getCurrent());
            rdata_iterator_->next();
            if (rdata_iterator_->isLast()) {
                // all used up, next.
                set_node_ = set_node_->getNext();
                // New RRset, so get a new rdata iterator, but only if this
                // was not the final RRset in the chain
                if (set_node_ != NULL) {
                    rrset_.reset(new TreeNodeRRset(rrclass_,
                                                   node_, set_node_, true));
                    rdata_iterator_ = rrset_->getRdataIterator();
                }
            }
            return (result);
        } else {
            ConstRRsetPtr result(new TreeNodeRRset(rrclass_,
                                                   node_, set_node_, true));

            // This one is used, move it to the next time for next call
            set_node_ = set_node_->getNext();

            return (result);
        }
    }

    virtual ConstRRsetPtr getSOA() const {
        isc_throw(NotImplemented, "Not implemented");
    }
};

} // End of anonymous namespace

ZoneIteratorPtr
InMemoryClient::getIterator(const Name& name, bool separate_rrs) const {
    ZoneTable::FindResult result(zone_table_->findZone(name));
    if (result.code != result::SUCCESS) {
        isc_throw(DataSourceError, "No such zone: " + name.toText());
    }

    return (ZoneIteratorPtr(new MemoryIterator(
                                getClass(),
                                result.zone_data->getZoneTree(), name,
                                separate_rrs)));
}

ZoneUpdaterPtr
InMemoryClient::getUpdater(const isc::dns::Name&, bool, bool) const {
    isc_throw(isc::NotImplemented, "Update attempt on in memory data source");
}

pair<ZoneJournalReader::Result, ZoneJournalReaderPtr>
InMemoryClient::getJournalReader(const isc::dns::Name&, uint32_t,
                                 uint32_t) const
{
    isc_throw(isc::NotImplemented, "Journaling isn't supported for "
              "in memory data source");
}

void
InMemoryClient::setMappedFile(const std::string& mmap_file, bool build) {
    mmap_file_ = mmap_file;
    if (mmap_file_.empty()) {
        return;
    }

    ZoneTable::destroy(mem_sgmt_, zone_table_, rrclass_);
    zone_table_ = NULL;
    if (!build) {
        return;
    }

    assert(mapped_file_version_ == -1);
    assert(mmap_sgmt_ == NULL);
    mapped_file_version_ = 0;

    // XXX: for simplicity we don't care exception safety below.
    const std::string full_fname = mmap_file_ + "." +
        boost::lexical_cast<std::string>(mapped_file_version_);
    unlink(full_fname.c_str()); // XXX doesn't work in case full reconfigure
    mmap_sgmt_ = new util::MemorySegmentMmap(full_fname, true);
    zone_table_ = ZoneTable::create(*mmap_sgmt_, rrclass_);
    mmap_sgmt_->setNamedAddress("zone_table", zone_table_);
}

std::string
InMemoryClient::getMappedFile() const {
    return (mmap_file_);
}

void
InMemoryClient::remapFile(size_t version) {
    const std::string full_fname = mmap_file_ + "." +
        boost::lexical_cast<std::string>(version);
    util::MemorySegmentMmap* new_mem_sgmt =
        new util::MemorySegmentMmap(full_fname);
    delete mmap_sgmt_;
    mmap_sgmt_ = new_mem_sgmt;
    zone_table_ = static_cast<ZoneTable*>(
        new_mem_sgmt->getNamedAddress("zone_table"));
}

} // end of namespace memory
} // end of namespace datasrc
} // end of namespace isc
