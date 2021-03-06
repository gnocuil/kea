// Copyright (C) 2011-2013  Internet Systems Consortium, Inc. ("ISC")
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

#include <dhcp6/ctrl_dhcp6_srv.h>
#include <dhcp6/dhcp6_log.h>
#include <log/logger_support.h>
#include <log/logger_manager.h>
#include <exceptions/exceptions.h>

#include <boost/lexical_cast.hpp>

#include <iostream>

using namespace isc::dhcp;
using namespace std;

/// This file contains entry point (main() function) for standard DHCPv6 server
/// component of Kea software suite. It parses command-line arguments and
/// instantiates ControlledDhcpv6Srv class that is responsible for establishing
/// connection with msgq (receiving commands and configuration) and also
/// creating Dhcpv6 server object as well.
///
/// For detailed explanation or relations between main(), ControlledDhcpv6Srv,
/// Dhcpv6Srv and other classes, see \ref dhcpv6Session.

namespace {
const char* const DHCP6_NAME = "kea-dhcp6";

const char* const DHCP6_LOGGER_NAME = "kea-dhcp6";

/// @brief Prints Kea Usage and exits
///
/// Note: This function never returns. It terminates the process.
void
usage() {
    cerr << "Kea DHCPv6 server, version " << VERSION << endl;
    cerr << endl;
    cerr << "Usage: " << DHCP6_NAME
         << " [-c cfgfile] [-v] [-V] [-d] [-p port_number]" << endl;
    cerr << "  -c file: specify configuration file" << endl;
    cerr << "  -v: print version number and exit." << endl;
    cerr << "  -V: print extended version and exit" << endl;
    cerr << "  -d: debug mode with extra verbosity (former -v)" << endl;
    cerr << "  -p number: specify non-standard port number 1-65535 "
         << "(useful for testing only)" << endl;
    exit(EXIT_FAILURE);
}
} // end of anonymous namespace

int
main(int argc, char* argv[]) {
    int ch;
    int port_number = DHCP6_SERVER_PORT; // The default. Any other values are
                                         // useful for testing only.
    bool verbose_mode = false; // Should server be verbose?

    // The standard config file
    std::string config_file("");

    while ((ch = getopt(argc, argv, "dvVp:c:")) != -1) {
        switch (ch) {
        case 'd':
            verbose_mode = true;
            break;

        case 'v':
            cout << Daemon::getVersion(false) << endl;
            return (EXIT_SUCCESS);

        case 'V':
            cout << Daemon::getVersion(true) << endl;
            return (EXIT_SUCCESS);

        case 'p': // port number
            try {
                port_number = boost::lexical_cast<int>(optarg);
            } catch (const boost::bad_lexical_cast &) {
                cerr << "Failed to parse port number: [" << optarg
                     << "], 1-65535 allowed." << endl;
                usage();
            }
            if (port_number <= 0 || port_number > 65535) {
                cerr << "Failed to parse port number: [" << optarg
                     << "], 1-65535 allowed." << endl;
                usage();
            }
            break;

        case 'c': // config file
            config_file = optarg;
            break;

        default:
            usage();
        }
    }

    // Check for extraneous parameters.
    if (argc > optind) {
        usage();
    }

    // Configuration file is required.
    if (config_file.empty()) {
        cerr << "Configuration file not specified." << endl;
        usage();
    }


    int ret = EXIT_SUCCESS;
    try {
        // Initialize logging.  If verbose, we'll use maximum verbosity.
        Daemon::loggerInit(DHCP6_LOGGER_NAME, verbose_mode);

        LOG_DEBUG(dhcp6_logger, DBG_DHCP6_START, DHCP6_START_INFO)
            .arg(getpid()).arg(port_number).arg(verbose_mode ? "yes" : "no");

        LOG_INFO(dhcp6_logger, DHCP6_STARTING).arg(VERSION);

        // Create the server instance.
        ControlledDhcpv6Srv server(port_number);

        // Remember verbose-mode
        server.setVerbose(verbose_mode);

        try {
            // Initialize the server, e.g. establish control session
            // if Bundy backend is used or read a configuration file
            // if Kea backend is used.
            server.init(config_file);

        } catch (const std::exception& ex) {

            try {
                // Let's log out what went wrong.
                isc::log::LoggerManager log_manager;
                log_manager.process();
                LOG_ERROR(dhcp6_logger, DHCP6_INIT_FAIL).arg(ex.what());
            } catch (...) {
                // The exeption thrown during the initialization could originate
                // from logger subsystem. Therefore LOG_ERROR() may fail as well.
                cerr << "Failed to initialize server: " << ex.what() << endl;
            }

            return (EXIT_FAILURE);
        }

        // And run the main loop of the server.
        server.run();

        LOG_INFO(dhcp6_logger, DHCP6_SHUTDOWN);

    } catch (const std::exception& ex) {

        // First, we print the error on stderr (that should always work)
        cerr << DHCP6_NAME << "Fatal error during start up: " << ex.what()
             << endl;

        // Let's also try to log it using logging system, but we're not
        // sure if it's usable (the exception may have been thrown from
        // the logger subsystem)
        LOG_FATAL(dhcp6_logger, DHCP6_SERVER_FAILED).arg(ex.what());
        ret = EXIT_FAILURE;
    }

    return (ret);
}
