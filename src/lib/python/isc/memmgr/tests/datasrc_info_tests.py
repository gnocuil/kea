# Copyright (C) 2013  Internet Systems Consortium.
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

import json
import os
import unittest

from isc.dns import *
import isc.config
import isc.datasrc
import isc.log
from isc.server_common.datasrc_clients_mgr import DataSrcClientsMgr
from isc.memmgr.datasrc_info import *

class TestSegmentInfo(unittest.TestCase):
    def setUp(self):
        self.__mapped_file_dir = os.environ['TESTDATA_PATH']
        self.__sgmt_info = SegmentInfo.create('mapped', 0, RRClass.IN,
                                              'sqlite3',
                                              {'mapped_file_dir':
                                                   self.__mapped_file_dir})

    def __check_sgmt_reset_param(self, user_type, expected_ver):
        """Common check on the return value of get_reset_param() for
        MappedSegmentInfo.

        Unless it's expected to return None, it should be a map that
        maps "mapped-file" to the expected version of mapped-file.

        """
        if expected_ver is None:
            self.assertIsNone(self.__sgmt_info.get_reset_param(user_type))
            return
        param = json.loads(self.__sgmt_info.get_reset_param(user_type))
        self.assertEqual(self.__mapped_file_dir +
                         '/zone-IN-0-sqlite3-mapped.' + str(expected_ver),
                         param['mapped-file'])

    def test_initial_params(self):
        self.__check_sgmt_reset_param(SegmentInfo.WRITER, 0)
        self.__check_sgmt_reset_param(SegmentInfo.READER, None)

    def test_swtich_versions(self):
        self.__sgmt_info.switch_versions()
        self.__check_sgmt_reset_param(SegmentInfo.WRITER, 1)
        self.__check_sgmt_reset_param(SegmentInfo.READER, 0)

        self.__sgmt_info.switch_versions()
        self.__check_sgmt_reset_param(SegmentInfo.WRITER, 0)
        self.__check_sgmt_reset_param(SegmentInfo.READER, 1)

    def test_init_others(self):
        # For local type of segment, information isn't needed and won't be
        # created.
        self.assertIsNone(SegmentInfo.create('local', 0, RRClass.IN,
                                             'sqlite3', {}))

        # Unknown type of segment will result in an exception.
        self.assertRaises(SegmentInfoError, SegmentInfo.create, 'unknown', 0,
                          RRClass.IN, 'sqlite3', {})

    def test_missing_methods(self):
        # Bad subclass of SegmentInfo that doesn't implement mandatory methods.
        class TestSegmentInfo(SegmentInfo):
            pass

        self.assertRaises(SegmentInfoError,
                          TestSegmentInfo().get_reset_param,
                          SegmentInfo.WRITER)
        self.assertRaises(SegmentInfoError, TestSegmentInfo().switch_versions)

class MockClientList:
    """A mock ConfigurableClientList class.

    Just providing minimal shortcut interfaces needed for DataSrcInfo class.

    """
    def __init__(self, status_list):
        self.__status_list = status_list

    def get_status(self):
        return self.__status_list

class TestDataSrcInfo(unittest.TestCase):
    def setUp(self):
        self.__mapped_file_dir = os.environ['TESTDATA_PATH']
        self.__mgr_config = {'mapped_file_dir': self.__mapped_file_dir}
        self.__sqlite3_dbfile = os.environ['TESTDATA_PATH'] + '/' + 'zone.db'
        self.__clients_map = {
            # mixture of 'local' and 'mapped' and 'unused' (type =None)
            # segments
            RRClass.IN: MockClientList([('datasrc1', 'local', None),
                                        ('datasrc2', 'mapped', None),
                                        ('datasrc3', None, None)]),
            RRClass.CH: MockClientList([('datasrc2', 'mapped', None),
                                        ('datasrc1', 'local', None)]) }

    def tearDown(self):
        if os.path.exists(self.__sqlite3_dbfile):
            os.unlink(self.__sqlite3_dbfile)

    def __check_sgmt_reset_param(self, sgmt_info, writer_file):
        # Check if the initial state of (mapped) segment info object has
        # expected values.
        self.assertIsNone(sgmt_info.get_reset_param(SegmentInfo.READER))
        param = json.loads(sgmt_info.get_reset_param(SegmentInfo.WRITER))
        self.assertEqual(writer_file, param['mapped-file'])

    def test_init(self):
        """Check basic scenarios of constructing DataSrcInfo."""

        # This checks that all data sources of all RR classes are covered,
        # "local" segments are ignored, info objects for "mapped" segments
        # are created and stored in segment_info_map.
        datasrc_info = DataSrcInfo(42, self.__clients_map, self.__mgr_config)
        self.assertEqual(42, datasrc_info.gen_id)
        self.assertEqual(self.__clients_map, datasrc_info.clients_map)
        self.assertEqual(2, len(datasrc_info.segment_info_map))
        sgmt_info = datasrc_info.segment_info_map[(RRClass.IN, 'datasrc2')]
        self.__check_sgmt_reset_param(sgmt_info, self.__mapped_file_dir +
                                      '/zone-IN-42-datasrc2-mapped.0')
        sgmt_info = datasrc_info.segment_info_map[(RRClass.CH, 'datasrc2')]
        self.__check_sgmt_reset_param(sgmt_info, self.__mapped_file_dir +
                                      '/zone-CH-42-datasrc2-mapped.0')

        # A case where clist.get_status() returns an empty list; shouldn't
        # cause disruption
        self.__clients_map = { RRClass.IN: MockClientList([])}
        datasrc_info = DataSrcInfo(42, self.__clients_map, self.__mgr_config)
        self.assertEqual(42, datasrc_info.gen_id)
        self.assertEqual(0, len(datasrc_info.segment_info_map))

        # A case where clients_map is empty; shouldn't cause disruption
        self.__clients_map = {}
        datasrc_info = DataSrcInfo(42, self.__clients_map, self.__mgr_config)
        self.assertEqual(42, datasrc_info.gen_id)
        self.assertEqual(0, len(datasrc_info.segment_info_map))

    def test_production(self):
        """Check the behavior closer to a production environment.

        Instead of using a mock classes, just for confirming we didn't miss
        something.

        """
        # This test uses real "mmaped" segment and doesn't work without
        # shared memory support
        if os.environ['HAVE_SHARED_MEMORY'] != 'yes':
            return

        datasrc_config = {
            "classes": {
                "IN": [{"type": "sqlite3", "cache-enable": True,
                        "cache-type": "mapped", "cache-zones": [],
                        "params": {"database_file": self.__sqlite3_dbfile}}]
                }
            }
        cmgr = DataSrcClientsMgr(use_cache=True)
        cmgr.reconfigure(datasrc_config)

        genid, clients_map = cmgr.get_clients_map()
        datasrc_info = DataSrcInfo(genid, clients_map, self.__mgr_config)

        self.assertEqual(1, datasrc_info.gen_id)
        self.assertEqual(clients_map, datasrc_info.clients_map)
        self.assertEqual(1, len(datasrc_info.segment_info_map))
        sgmt_info = datasrc_info.segment_info_map[(RRClass.IN, 'sqlite3')]
        self.assertIsNone(sgmt_info.get_reset_param(SegmentInfo.READER))

if __name__ == "__main__":
    isc.log.init("bind10-test")
    isc.log.resetUnitTestRootLogger()
    unittest.main()