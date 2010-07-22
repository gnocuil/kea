// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

// $Id$

#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include "communicator.h"

/// \brief Basic Controller Class
///
/// This class is the base for all the controller classes.

class Controller {
public:

    /// \brief Constructor
    ///
    Controller() {}
    virtual ~Controller() {}

    /// \brief Runs the controller
    ///
    /// This method never returns.  It loops, reading packets, possibly
    /// processing them, and sending them on.
    ///
    /// \param input_communicator Communicator used to communicate with the
    /// downstream process (i.e. where packets originate from).
    /// \param output_communicator Communicator used to communicate with the
    /// upstream process (i.e. where packets are forwarded to).
    virtual void run(Communicator& downstream_communicator,
         Communicator& upstream_communicator) = 0;
};

#endif // __CONTROLLER_H
