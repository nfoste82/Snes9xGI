import unittest
from types import SimpleNamespace as N
from alttp_surviving_provenance import transfer
class TransferTest(unittest.TestCase):
 def fixture(self,word=1):return N(room=2,cells=[[N(word=word)],[N(word=3)]])
 def test_equal(self):
  owners,changed=transfer(self.fixture(),self.fixture(),N(owners=[[4],[5]]));self.assertEqual(owners,[[4],[5]]);self.assertEqual(changed,[])
 def test_changed_including_priority_invalidates(self):
  owners,changed=transfer(self.fixture(),self.fixture(8193),N(owners=[[4],[5]]));self.assertEqual(owners,[[65535],[5]]);self.assertEqual(len(changed),1)
 def test_room_rejected(self):
  live=self.fixture();live.room=3
  with self.assertRaises(ValueError):transfer(self.fixture(),live,N(owners=[[4],[5]]))
