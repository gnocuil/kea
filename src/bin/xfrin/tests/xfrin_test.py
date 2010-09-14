# Copyright (C) 2009  Internet Systems Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SYSTEMS CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SYSTEMS CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $Id$

import unittest
import socket
from xfrin import *

#
# Commonly used (mostly constant) test parameters
#
TEST_ZONE_NAME = "example.com"
TEST_RRCLASS = RRClass.IN()
TEST_DB_FILE = 'db_file'
TEST_MASTER_IPV4_ADDRESS = '127.0.0.1'
TEST_MASTER_IPV4_ADDRINFO = (socket.AF_INET, socket.SOCK_STREAM,
                             socket.IPPROTO_TCP, '',
                             (TEST_MASTER_IPV4_ADDRESS, 53))
TEST_MASTER_IPV6_ADDRESS = '::1'
TEST_MASTER_IPV6_ADDRINFO = (socket.AF_INET6, socket.SOCK_STREAM,
                             socket.IPPROTO_TCP, '',
                             (TEST_MASTER_IPV6_ADDRESS, 53))
# XXX: This should be a non priviledge port that is unlikely to be used.
# If some other process uses this port test will fail.
TEST_MASTER_PORT = '53535'

soa_rdata = Rdata(RRType.SOA(), TEST_RRCLASS,
                  'master.example.com. admin.example.com ' +
                  '1234 3600 1800 2419200 7200')
soa_rrset = RRset(Name(TEST_ZONE_NAME), TEST_RRCLASS, RRType.SOA(),
                  RRTTL(3600))
soa_rrset.add_rdata(soa_rdata)
example_axfr_question = Question(Name(TEST_ZONE_NAME), TEST_RRCLASS,
                                 RRType.AXFR())
example_soa_question = Question(Name(TEST_ZONE_NAME), TEST_RRCLASS,
                                 RRType.SOA())
default_questions = [example_axfr_question]
default_answers = [soa_rrset]

class XfrinTestException(Exception):
    pass

class MockModuleCCSession:
    def __init__(self, spec_file_name, config_handler, command_handler, cc_session = None):
        pass

    def start(self):
        pass

    def get_full_config(self):
        return ({"transfers_in":10})

    def check_command(self):
        return True

class MockXfrin(Xfrin):
    # This is a class attribute of a callable object that specifies a non
    # default behavior triggered in _cc_check_command().  Specific test methods
    # are expected to explicitly set this attribute before creating a
    # MockXfrin object (when it needs a non default behavior).
    # See the TestMain class.
    check_command_hook = None

    def _cc_setup(self):
        isc.config.ModuleCCSession = MockModuleCCSession
        super()._cc_setup()
    
    def _cc_check_command(self):
        self._shutdown_flag = 1
        if MockXfrin.check_command_hook:
            MockXfrin.check_command_hook()

class MockSocket():
    def __init__(self):
        self.sendqueue = bytearray()

    def recv(self, size):
        if size < 0:
            raise socket.error("error bufsize")
        if len(self.sendqueue) < size:
            size = len(self.sendqueue)
        result = self.sendqueue[:size]
        del self.sendqueue[:size]
        return result

    def send(self, data):
        try:
            self.sendqueue.extend(data)
        except TypeError as e:
            raise socket.error(e)
        return len(data)

    def close(self):
        pass

class MockXfrinConnection(XfrinConnection):
    def __init__(self, conn_socket, zone_name, rrclass, db_file, 
                 master_addr):
        super().__init__(conn_socket, zone_name, rrclass, db_file,
                         master_addr)
        self.query_data = b''
        self.reply_data = b''
        self.mock_test_data = b"hello bind10"
        self.force_time_out = False
        self.force_close = False
        self.qlen = None
        self.qid = None
        self.response_generator = None
        self.closed = False

    def connect_to_master(self):
        return self._socket

    def _select(self):
        if self.force_time_out:
            return 0
        if self.force_close:
            self.close()
            self._conn_socket.close()
        return 1

    def recv(self, size):
        if self.closed:
            raise socket.error('recv attempt on a closed socket')
        data = self.reply_data[:size]
        self.reply_data = self.reply_data[size:]
        if len(data) < size:
            raise XfrinTestException('cannot get reply data')
        return data

    def close(self):
        self.closed = True
        super().close()

    def send(self, data):
        if self.qlen != None and len(self.query_data) >= self.qlen:
            # This is a new query.  reset the internal state.
            self.qlen = None
            self.qid = None
            self.query_data = b''
        self.query_data += data

        # when the outgoing data is sufficiently large to contain the length
        # and the QID fields (4 octets or more), extract these fields.
        # The length will be reset the internal query data to support multiple
        # queries in a single test.
        # The QID will be used to construct a matching response.
        if len(self.query_data) >= 4 and self.qid == None:
            self.qlen = socket.htons(struct.unpack('H',
                                                   self.query_data[0:2])[0])
            self.qid = socket.htons(struct.unpack('H', self.query_data[2:4])[0])
            # if the response generator method is specified, invoke it now.
            if self.response_generator != None:
                self.response_generator()
        return len(data)

    def create_response_data(self, response = True, bad_qid = False,
                             rcode = Rcode.NOERROR(),
                             questions = default_questions,
                             answers = default_answers):
        resp = Message(Message.RENDER)
        qid = self.qid
        if bad_qid:
            qid += 1
        resp.set_qid(qid)
        resp.set_opcode(Opcode.QUERY())
        resp.set_rcode(rcode)
        if response:
            resp.set_header_flag(MessageFlag.QR())
        [resp.add_question(q) for q in questions]
        [resp.add_rrset(Section.ANSWER(), a) for a in answers]

        renderer = MessageRenderer()
        resp.to_wire(renderer)
        reply_data = struct.pack('H', socket.htons(renderer.get_length()))
        reply_data += renderer.get_data()

        return reply_data

class TestXfrinConnection(unittest.TestCase):
    def setUp(self):
        self.conn_sockets = socket.socketpair()
        self.mock_xfrsockets = socket.socketpair()
        if os.path.exists(TEST_DB_FILE):
            os.remove(TEST_DB_FILE)
        self.conn = MockXfrinConnection(self.conn_sockets[1], 'example.com.',
                                        TEST_RRCLASS, TEST_DB_FILE,
                                        TEST_MASTER_IPV4_ADDRINFO)
        # replace the XFR socket with our local mock
        self.conn._socket = self.mock_xfrsockets[1]
        self.axfr_after_soa = False
        self.soa_response_params = {
            'questions': [example_soa_question],
            'bad_qid': False,
            'response': True,
            'rcode': Rcode.NOERROR(),
            'axfr_after_soa': self._create_normal_response_data
            }

    def tearDown(self):
        self.conn.close()
        self.conn_sockets[0].close()
        self.conn_sockets[1].close()
        self.mock_xfrsockets[0].close()
        self.mock_xfrsockets[1].close()
        if os.path.exists(TEST_DB_FILE):
            os.remove(TEST_DB_FILE)

    def test_connect(self):
        self.assertRaises(Exception, self.conn.connect,
                          (TEST_MASTER_IPV4_ADDRESS,53))

    def test_send(self):
        self.conn._socket.close()
        self.conn._socket = MockSocket()
        self.assertEqual(len(self.conn.mock_test_data),
                         super(MockXfrinConnection,
                               self.conn).send(self.conn.mock_test_data))

    def test_send_exception(self):
        self.conn._socket.close()
        self.conn._socket = MockSocket()
        self.assertRaises(socket.error,
                          super(MockXfrinConnection, self.conn).send,
                          "not binary data")

    def test_recv(self):
        self.conn._socket.close()
        self.conn._socket = MockSocket()
        super(MockXfrinConnection, self.conn).send(self.conn.mock_test_data)
        self.assertEqual(self.conn.mock_test_data,
                         super(MockXfrinConnection,
                               self.conn).recv(len(self.conn.mock_test_data)))

    def test_recv_nodata(self):
        self.conn._socket.close()
        self.conn._socket = MockSocket()
        self.assertEqual(b"", super(MockXfrinConnection, self.conn).recv(20))

    def test_recv_exception(self):
        self.conn._socket.close()
        self.conn._socket = MockSocket()
        super(MockXfrinConnection, self.conn).send(self.conn.mock_test_data)
        self.assertRaises(socket.error,
                          super(MockXfrinConnection, self.conn).recv, -1)

    def test_select_readok(self):
        self.mock_xfrsockets[0].send(self.conn.mock_test_data)
        self.conn._idle_timeout = 3 # timeout shouldn't occur
        self.assertEqual(1, super(MockXfrinConnection, self.conn)._select())
        self.assertEqual(self.conn.mock_test_data,
                         self.conn._socket.recv(len(self.conn.mock_test_data)))

    def test_select_timeout(self):
        self.conn._idle_timeout = 0.1
        self.assertEqual(0, super(MockXfrinConnection, self.conn)._select())

    def test_select_shutdown(self):
        self.conn._idle_timeout = 3 # timeout shouldn't occur
        self.conn_sockets[0].send(b"shutdown")
        self.assertRaises(XfrinException, super(MockXfrinConnection,
                                                self.conn)._select)
    def test_init_ip6(self):
        # This test simply creates a new XfrinConnection object with an
        # IPv6 address, tries to bind it to an IPv6 wildcard address/port
        # to confirm an AF_INET6 socket has been created.  A naive application
        # tends to assume it's IPv4 only and hardcode AF_INET.  This test
        # uncovers such a bug.
        c = MockXfrinConnection({}, 'example.com.', TEST_RRCLASS, TEST_DB_FILE,
                                 TEST_MASTER_IPV6_ADDRINFO)
        c._socket.bind(('::', 0))
        c.close()

    def test_init_chclass(self):
        c = XfrinConnection({}, 'example.com.', RRClass.CH(), TEST_DB_FILE,
                             TEST_MASTER_IPV4_ADDRINFO)
        axfrmsg = c._create_query(RRType.AXFR())
        self.assertEqual(axfrmsg.get_question()[0].get_class(),
                         RRClass.CH())
        c.close()

    def test_response_with_invalid_msg(self):
        self.conn.reply_data = b'aaaxxxx'
        self.assertRaises(XfrinTestException, self._handle_xfrin_response)

    def test_response_without_end_soa(self):
        self.conn._send_query(RRType.AXFR())
        self.conn.reply_data = self.conn.create_response_data()
        self.assertRaises(XfrinTestException, self._handle_xfrin_response)

    def test_response_bad_qid(self):
        self.conn._send_query(RRType.AXFR())
        self.conn.reply_data = self.conn.create_response_data(bad_qid = True)
        self.assertRaises(XfrinException, self._handle_xfrin_response)

    def test_response_non_response(self):
        self.conn._send_query(RRType.AXFR())
        self.conn.reply_data = self.conn.create_response_data(response = False)
        self.assertRaises(XfrinException, self._handle_xfrin_response)

    def test_response_error_code(self):
        self.conn._send_query(RRType.AXFR())
        self.conn.reply_data = self.conn.create_response_data(
            rcode=Rcode.SERVFAIL())
        self.assertRaises(XfrinException, self._handle_xfrin_response)

    def test_response_multi_question(self):
        self.conn._send_query(RRType.AXFR())
        self.conn.reply_data = self.conn.create_response_data(
            questions=[example_axfr_question, example_axfr_question])
        self.assertRaises(XfrinException, self._handle_xfrin_response)

    def test_response_empty_answer(self):
        self.conn._send_query(RRType.AXFR())
        self.conn.reply_data = self.conn.create_response_data(answers=[])
        # Should an empty answer trigger an exception?  Even though it's very
        # unusual it's not necessarily invalid.  Need to revisit.
        self.assertRaises(XfrinException, self._handle_xfrin_response)

    def test_response_non_response(self):
        self.conn._send_query(RRType.AXFR())
        self.conn.reply_data = self.conn.create_response_data(response = False)
        self.assertRaises(XfrinException, self._handle_xfrin_response)

    def test_soacheck(self):
        # we need to defer the creation until we know the QID, which is
        # determined in _check_soa_serial(), so we use response_generator.
        self.conn.response_generator = self._create_soa_response_data
        self.assertEqual(self.conn._check_soa_serial(), XFRIN_OK)

    def test_soacheck_with_bad_response(self):
        self.conn.response_generator = self._create_broken_response_data
        self.assertRaises(MessageTooShort, self.conn._check_soa_serial)

    def test_soacheck_badqid(self):
        self.soa_response_params['bad_qid'] = True
        self.conn.response_generator = self._create_soa_response_data
        self.assertRaises(XfrinException, self.conn._check_soa_serial)

    def test_soacheck_non_response(self):
        self.soa_response_params['response'] = False
        self.conn.response_generator = self._create_soa_response_data
        self.assertRaises(XfrinException, self.conn._check_soa_serial)

    def test_soacheck_error_code(self):
        self.soa_response_params['rcode'] = Rcode.SERVFAIL()
        self.conn.response_generator = self._create_soa_response_data
        self.assertRaises(XfrinException, self.conn._check_soa_serial)

    def test_response_timeout(self):
        self.conn.response_generator = self._create_normal_response_data
        self.conn.force_time_out = True
        self.assertRaises(XfrinException, self._handle_xfrin_response)

    def test_response_remote_close(self):
        self.conn.response_generator = self._create_normal_response_data
        self.conn.force_close = True
        self.assertRaises(socket.error, self._handle_xfrin_response)

    def test_response_bad_message(self):
        self.conn.response_generator = self._create_broken_response_data
        self.conn._send_query(RRType.AXFR())
        self.assertRaises(Exception, self._handle_xfrin_response)

    def test_response(self):
        # normal case.
        self.conn.response_generator = self._create_normal_response_data
        self.conn._send_query(RRType.AXFR())
        # two SOAs, and only these have been transfered.  the 2nd SOA is just
        # a marker, so only 1 RR has been provided in the iteration.
        self.assertEqual(self._handle_xfrin_response(), 1)

    def test_do_xfrin(self):
        self.conn.response_generator = self._create_normal_response_data
        self.assertEqual(self.conn.do_xfrin(False), XFRIN_OK)

    def test_do_xfrin_empty_response(self):
        # skipping the creation of response data, so the transfer will fail.
        self.assertEqual(self.conn.do_xfrin(False), XFRIN_FAIL)

    def test_do_xfrin_bad_response(self):
        self.conn.response_generator = self._create_broken_response_data
        self.assertEqual(self.conn.do_xfrin(False), XFRIN_FAIL)

    def test_do_xfrin_dberror(self):
        # DB file is under a non existent directory, so its creation will fail,
        # which will make the transfer fail.
        self.conn._db_file = "not_existent/" + TEST_DB_FILE
        self.assertEqual(self.conn.do_xfrin(False), XFRIN_FAIL)

    def test_do_soacheck_and_xfrin(self):
        self.conn.response_generator = self._create_soa_response_data
        self.assertEqual(self.conn.do_xfrin(True), XFRIN_OK)

    def test_do_soacheck_broken_response(self):
        self.conn.response_generator = self._create_broken_response_data
        # XXX: TODO: this test failed here, should xfr not raise an
        # exception but simply drop and return FAIL?
        #self.assertEqual(self.conn.do_xfrin(True), XFRIN_FAIL)
        self.assertRaises(MessageTooShort, self.conn.do_xfrin, True)

    def test_do_soacheck_badqid(self):
        # the QID mismatch would internally trigger a XfrinException exception,
        # and covers part of the code that other tests can't.
        self.soa_response_params['bad_qid'] = True
        self.conn.response_generator = self._create_soa_response_data
        self.assertEqual(self.conn.do_xfrin(True), XFRIN_FAIL)

    def _handle_xfrin_response(self):
        # This helper methods iterates over all RRs (excluding the ending SOA)
        # transferred, and simply returns the number of RRs.  The return value
        # may be used an assertion value for test cases.
        rrs = 0
        for rr in self.conn._handle_xfrin_response():
            rrs += 1
        return rrs

    def _create_normal_response_data(self):
        # This helper method creates a simple sequence of DNS messages that
        # forms a valid XFR transaction.  It consists of two messages, each
        # containing just a single SOA RR.
        self.conn.reply_data = self.conn.create_response_data()
        self.conn.reply_data += self.conn.create_response_data()

    def _create_soa_response_data(self):
        # This helper method creates a DNS message that is supposed to be
        # used a valid response to SOA queries prior to XFR.
        # If axfr_after_soa is True, it resets the response_generator so that
        # a valid XFR messages will follow.
        self.conn.reply_data = self.conn.create_response_data(
            bad_qid=self.soa_response_params['bad_qid'],
            response=self.soa_response_params['response'],
            rcode=self.soa_response_params['rcode'],
            questions=self.soa_response_params['questions'])
        if self.soa_response_params['axfr_after_soa'] != None:
            self.conn.response_generator = self.soa_response_params['axfr_after_soa']

    def _create_broken_response_data(self):
        # This helper method creates a bogus "DNS message" that only contains
        # 4 octets of data.  The DNS message parser will raise an exception.
        bogus_data = b'xxxx'
        self.conn.reply_data = struct.pack('H', socket.htons(len(bogus_data)))
        self.conn.reply_data += bogus_data

class MockThread:
    def is_alive(self):
        return True

class TestXfrin(unittest.TestCase):
    def setUp(self):
        self.xfr = MockXfrin()
        self.args = {}
        self.args['zone_name'] = TEST_ZONE_NAME
        self.args['port'] = TEST_MASTER_PORT
        self.args['master'] = TEST_MASTER_IPV4_ADDRESS
        self.args['db_file'] = TEST_DB_FILE

    def tearDown(self):
        self.xfr.shutdown()

    def _do_parse(self):
        return self.xfr._parse_cmd_params(self.args)

    def test_cc_check_command(self):
        self.assertEqual(None, self.xfr._cc_check_command())


    def test_parse_cmd_params(self):
        name, master_addrinfo, db_file = self._do_parse()
        self.assertEqual(master_addrinfo[4][1], int(TEST_MASTER_PORT))
        self.assertEqual(name, TEST_ZONE_NAME)
        self.assertEqual(master_addrinfo[4][0], TEST_MASTER_IPV4_ADDRESS)
        self.assertEqual(db_file, TEST_DB_FILE)

    def test_parse_cmd_params_default_port(self):
        del self.args['port']
        master_addrinfo = self._do_parse()[1]
        self.assertEqual(master_addrinfo[4][1], 53)

    def test_parse_cmd_params_ip6master(self):
        self.args['master'] = TEST_MASTER_IPV6_ADDRESS
        master_addrinfo = self._do_parse()[1]
        self.assertEqual(master_addrinfo[4][0], TEST_MASTER_IPV6_ADDRESS)

    def test_parse_cmd_params_nozone(self):
        # zone name is mandatory.
        del self.args['zone_name']
        self.assertRaises(XfrinException, self._do_parse)

    def test_parse_cmd_params_nomaster(self):
        # master address is mandatory.
        del self.args['master']
        self.assertRaises(XfrinException, self._do_parse)

    def test_parse_cmd_params_bad_ip4(self):
        self.args['master'] = '3.3.3.3.3'
        self.assertRaises(XfrinException, self._do_parse)

    def test_parse_cmd_params_bad_ip6(self):
        self.args['master'] = '1::1::1'
        self.assertRaises(XfrinException, self._do_parse)

    def test_parse_cmd_params_bad_port(self):
        self.args['port'] = '-1'
        self.assertRaises(XfrinException, self._do_parse)

        self.args['port'] = '65536'
        self.assertRaises(XfrinException, self._do_parse)

        self.args['port'] = 'http'
        self.assertRaises(XfrinException, self._do_parse)
     
    def test_config_handler_noupdate(self):
        old_value = self.xfr._max_transfers_in
        self.xfr.config_handler({})
        self.assertEqual(old_value, self.xfr._max_transfers_in)

    def test_config_handler(self):
        self.xfr.config_handler({"transfers_in":5})
        self.assertEqual(5, self.xfr._max_transfers_in)

    def test_command_handler_shutdown(self):
        self.assertEqual(self.xfr.command_handler("shutdown",
                                                  None)['result'][0], 0)
        # shutdown command doesn't expect an argument, but accepts it if any.
        self.assertEqual(self.xfr.command_handler("shutdown",
                                                  "unused")['result'][0], 0)

    def test_command_handler_retransfer(self):
        self.assertEqual(self.xfr.command_handler("retransfer",
                                                  self.args)['result'][0], 0)

    def test_command_handler_retransfer_badcommand(self):
        self.args['master'] = 'invalid'
        self.assertEqual(self.xfr.command_handler("retransfer",
                                                  self.args)['result'][0], 1)

    def test_command_handler_retransfer_quota(self):
        for i in range(self.xfr._max_transfers_in - 1):
            self.xfr._zones_to_threads[str(i) + TEST_ZONE_NAME] = MockThread()
        # there can be one more outstanding transfer.
        self.assertEqual(self.xfr.command_handler("retransfer",
                                                  self.args)['result'][0], 0)
        # make sure the # xfrs would excceed the quota
        self.xfr._zones_to_threads[str(self.xfr._max_transfers_in) + TEST_ZONE_NAME] = MockThread()
        # this one should fail
        self.assertEqual(self.xfr.command_handler("retransfer",
                                                  self.args)['result'][0], 1)

    def test_command_handler_retransfer_inprogress(self):
        self.xfr._zones_to_threads[TEST_ZONE_NAME] = MockThread()
        self.assertEqual(self.xfr.command_handler("retransfer",
                                                  self.args)['result'][0], 1)

    def test_command_handler_retransfer_nomodule(self):
        dns_module = sys.modules['libdns_python'] # this must exist
        del sys.modules['libdns_python']
        self.assertEqual(self.xfr.command_handler("retransfer",
                                                  self.args)['result'][0], 1)
        # sys.modules is global, so we must recover it
        sys.modules['libdns_python'] = dns_module

    def test_command_handler_refresh(self):
        # at this level, refresh is no different than retransfer.
        # just confirm the successful case with a different family of address.
        self.args['master'] = TEST_MASTER_IPV6_ADDRESS
        self.assertEqual(self.xfr.command_handler("refresh",
                                                  self.args)['result'][0], 0)

    def test_command_handler_unknown(self):
        self.assertEqual(self.xfr.command_handler("xxx", None)['result'][0], 1)

def raise_interrupt():
    raise KeyboardInterrupt()

def raise_ccerror():
    raise isc.cc.session.SessionError('test error')

def raise_excpetion():
    raise Exception('test exception')

class TestMain(unittest.TestCase):
    def setUp(self):
        MockXfrin.check_command_hook = None

    def tearDown(self):
        MockXfrin.check_command_hook = None

    def test_startup(self):
        main(MockXfrin, False)

    def test_startup_interrupt(self):
        MockXfrin.check_command_hook = raise_interrupt
        main(MockXfrin, False)

    def test_startup_ccerror(self):
        MockXfrin.check_command_hook = raise_ccerror
        main(MockXfrin, False)

    def test_startup_generalerror(self):
        MockXfrin.check_command_hook = raise_excpetion
        main(MockXfrin, False)

if __name__== "__main__":
    try:
        unittest.main()
    except KeyboardInterrupt as e:
        print(e)
