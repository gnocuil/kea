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

#ifndef __PERF_PKT6_H
#define __PERF_PKT6_H

#include <time.h>
#include <boost/shared_ptr.hpp>
#include <dhcp/pkt6.h>

#include "localized_option.h"

namespace isc {
namespace perfdhcp {

/// \brief PerfPkt6 (DHCPv6 packet)
///
/// This class extends the functionality of \ref isc::dhcp::Pkt6 by
/// adding the ability to specify options offset in a DHCP message
/// and overridethe  options' contents with new option.
/// This approach is useful when we create packet object from
/// raw template buffer (from a file) and we want to use it as
/// a base to create test packets to be sent to the DHCP server.
///
/// Some of the contents of such a template packets always
/// have to be replaced e.g. transaction id, IA_NA. Other
/// contents (options) may be changed e.g. elapsed time,
/// server id.
///
/// In order to create a packet from a raw template buffer,
/// we have to pass this buffer along with transaction id
/// offset. The class will read the transaction id from the buffer.
/// Next, in order to replace the contents of the selected options
/// in a template packet, we need to add these selected options
/// to the packet object using addOption() method. Please note
/// that the options must be of the \ref isc::perfdhcp::LocalizedOption type.
///
/// \note: if you don't use template files, simply use constructors
/// inherited from parent class and isc::dhcp::Option type instead
///
class PerfPkt6 : public dhcp::Pkt6 {
public:

    /// \brief Represents offset value.
    ///
    /// This class represent offsets for DHCP message fields
    /// like transaction id. Constructors of PerfPkt6 take a
    /// number of arguments of integer type so it is easy to
    /// mess up arguments of constructors and for example
    /// swap transaction id with its offset.
    ///
    /// Use of this class implies that the client class has to
    /// explicitly use a constructor of this class to pass the
    /// offset value. This should prevent mistakes and save some
    /// time on debugging.
    class Offset {
    public:
        /// \brief Default constructor
        explicit Offset() :
            offset_(1) { };

        /// \brief Constructor
        ///
        /// \param offset offset value
        explicit Offset(size_t offset) :
            offset_(offset) { };

        /// \brief Returns offset value.
        ///
        /// \return offset value.
        size_t get() const { return offset_; };
    private:
        size_t offset_;    ///< offset value
    };

    /// Localized option pointer type.
    typedef boost::shared_ptr<LocalizedOption> LocalizedOptionPtr;

    /// \brief Constructor, used for outgoing DHCP messages.
    ///
    /// Creates new DHCPv6 message using the provided buffer. The
    /// transaction id and its offset are specified through this
    /// constructor, as they are stored in outgoing message
    /// when client class calls \ref PerfPkt6::rawPack.
    ///
    /// \note This constructor should be used only for outgoing
    /// messages that are created from raw buffer (e.g. read from
    /// template files).
    ///
    /// \param buf buffer holiding contents of the message (this can
    /// be directly read from template file).
    /// \param len length of the data in the buffer.
    /// \param transid transaction id to be stored in outgoing message.
    /// \param transid_offset transaction id offset in outgoing message.
    PerfPkt6(const uint8_t* buf,
             uint32_t len,
             uint32_t transid,
             const Offset& transid_offset);

    /// Constructor, used for incoming DHCP messages.
    ///
    /// Creates ia new DHCPv6 message using the provided buffer. The new object
    /// will keep a copy of contents of provided buffer. If the buffer contains
    /// options at custom offsets (e.g. if packet was read from a
    /// template file) additional information about the options'
    /// offsets has to be provided - see
    /// \ref isc::perfdhcp::LocalizedOption for details.
    ///
    /// The transaction id offset points to location of raw data where the
    /// transaction id field is stored. The transaction id will
    /// be read from this location when PerfPkt6::rawUnpack is
    /// called. The transid_ class member will be updated accordingly.
    ///
    /// \note use this constructor only in the case where you want to create
    /// incoming DHCPv6 object from the raw buffer and you know options offsets.
    /// Options offsets are specified from perfdhcp command line by the user.
    ///
    /// \param buf pointer to a buffer of received packet content.
    /// \param len size of buffer of packet content.
    /// \param transid_offset transaction id offset in a message.
    PerfPkt6(const uint8_t* buf,
             uint32_t len,
             const Offset& transid_offset);

    /// \brief Returns transaction id offset in packet buffer
    ///
    /// return transaction id offset in packet buffer
    size_t getTransIdOffset() const { return transid_offset_; };

    /// \brief Prepares on-wire format from raw buffer
    ///
    /// The method copies the user buffer to the output buffer and
    /// extracts the transaction id from it based on the transaction id
    /// offset provided in the constructor.
    ///
    /// \note: Use this method to prepare an on-wire DHCPv6 message
    /// when you use template packets that require replacement
    /// of selected options' contents before sending.
    ///
    /// \retrun false, id pack operation failed.
    bool rawPack();

    /// \brief Handles limited binary packet parsing for packets with
    /// custom offsets of options and transaction id
    ///
    /// This method  handles the reception of packets that have non-default
    /// values of options or transaction id offsets. Use
    /// \ref isc::dhcp::Pkt6::addOption to specify which options to parse.
    /// Each option should be of the: isc::perfdhcp::LocalizedOption
    /// type with the offset value indicated.
    ///
    /// \return false, if unpack operation failed.
    bool rawUnpack();

    /// \brief Update packet timestamp with current time
    ///
    /// \throw isc::Unexpected if timestamp update failed
    void updateTimestamp();

private:

    /// \brief Updates options in the output buffer
    ///
    /// The method uses the options collection added to object of this class
    /// with \ref dhcp::Pkt6::addOption to create on-wire data. Option objects
    /// have to be of \ref perfdhcp::LocalizedOption type and should
    /// have non-zero values of offsets specified.
    ///
    /// This method will use these offsets to seek to given position in the
    /// output buffer and update the option on-wire data with the contents of
    /// the option's buffer.
    ///
    /// \throw isc::Unexpected if options update failed.
    void rawPackOptions();

    /// \brief Reads contents of specified options from buffer
    ///
    /// The method reads options data from the copy of the buffer
    /// provided in constructor and stores the data in options
    /// objects that belong to options collection.
    ///
    /// The client class that constructs this object has to create an
    /// options collection prior to calling \ref rawUnpack
    /// method that in turn calls this method.
    ///
    /// If an option is not added to options collection, it will
    /// not be added by this method. This method will rather
    /// skip update of such an option even if it is present
    /// in the packet's buffer.
    ///
    /// \throw isc::Unexpected if options unpack failed.
    void rawUnpackOptions();

    size_t transid_offset_;      ///< transaction id offset

};

} // namespace perfdhcp
} // namespace isc

#endif // __PERF_PKT6_H
