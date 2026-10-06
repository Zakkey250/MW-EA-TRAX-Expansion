"""Synthetic block regression; no game or music assets are required."""
from pathlib import Path
import io,struct,sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'runtime'))
import native_bank as native
from build_cache import copy_stream

def block(tag,body):return struct.pack('<4sI',tag,len(body)+8)+body
class HeaderTests(unittest.TestCase):
 def test_positive_signed_boundaries(self):
  for value in [128,255,32768,65535,8388608,10501895,16777215]:
   size=(value.bit_length()+7)//8
   body=b'PT\0\0'+bytes([0x85,size])+value.to_bytes(size,'big')+b'\xff'
   repaired,expected=native.repair_frame_header(body)
   self.assertEqual(expected,value)
   length=repaired[5];actual=int.from_bytes(repaired[6:6+length],'big',signed=True)
   self.assertEqual(actual,value)
   src=block(b'SCHl',body)+block(b'SCDl',struct.pack('<I',value))+block(b'SCEl',b'')
   out=io.BytesIO();copy_stream(io.BytesIO(src),out,0)
   self.assertIn(native.positive_patch(0x85,value),out.getvalue())
 def test_frame_mismatch_refused(self):
  body=b'PT\0\0\x85\x01\x80\xff'
  src=block(b'SCHl',body)+block(b'SCDl',struct.pack('<I',127))+block(b'SCEl',b'')
  with self.assertRaises(ValueError):copy_stream(io.BytesIO(src),io.BytesIO(),0)
 def test_four_byte_negative_sentinel_unchanged(self):
  body=b'PT\0\0\x85\x04\xff\xff\xff\xff\xff'
  self.assertEqual(native.repair_frame_header(body),(body,None))
 def test_truncated_header_refused(self):
  with self.assertRaises(ValueError):native.repair_frame_header(b'PT\0\0\x85\x03\x80')
if __name__=='__main__':unittest.main()
