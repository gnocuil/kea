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

#ifndef __LOGGER_SUPPORT_H
#define __LOGGER_SUPPORT_H

#include <unistd.h>

#include <string>
#include <log/dll.h>
#include <log/logger.h>
#include <log/logger_unittest_support.h>

/// \file
/// \brief Logging initialization functions
///
/// Contains a set of functions relating to logging initialization that are
/// used by the production code.

namespace isc {
namespace log {

/// \brief Is logging initialized?
///
/// As some underlying logging implementations can behave unpredictably if they
/// have not been initialized when a logging function is called, their
/// initialization state is tracked.  The logger functions will check this flag
/// and throw an exception if logging is not initialized at that point.
///
/// \return true if logging has been initialized, false if not
B10_LIBLOG_API bool isLoggingInitialized();

/// \brief Set state of "logging initialized" flag
///
/// \param state State to set the flag to. (This is expected to be "true" - the
///        default - for all code apart from specific unit tests.)
B10_LIBLOG_API void setLoggingInitialized(bool state = true);

/// \brief Run-time initialization
///
/// Performs run-time initialization of the logger in particular supplying:
///
/// - Name of the root logger
/// - The severity (and if applicable, debug level) for the root logger.
/// - Name of a local message file, containing localisation of message text.
///
/// This function is likely to change over time as more debugging options are
/// held in the configuration database.
///
/// \param root Name of the root logger
/// \param severity Severity at which to log
/// \param dbglevel Debug severity (ignored if "severity" is not "DEBUG")
/// \param file Name of the local message file.
B10_LIBLOG_API
void initLogger(const std::string& root,
                isc::log::Severity severity = isc::log::INFO,
                int dbglevel = 0, const char* file = NULL);

} // namespace log
} // namespace isc

#endif // __LOGGER_SUPPORT_H
