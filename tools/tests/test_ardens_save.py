import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ardens_save

BUILD = ROOT / "build" / "tests" / "ardens-save"


class ArdensSaveTest(unittest.TestCase):
    def test_hash_matches_reference_image(self):
        program = bytes((i * 7 + 3) & 0xFF for i in range(32 * 1024))
        fxdata = bytes([0, 1, 2, 3, 4, 255, 0, 170])
        self.assertEqual(ardens_save.game_hash(program, fxdata, 4096), 0x39C3DFC12F729BE1)

    def test_ihex_parser_applies_records(self):
        source = "\n".join(
            [
                ":020000000102FB",
                ":020000040000FA",
                ":01001000AA45",
                ":00000001FF",
            ]
        )
        program = ardens_save.parse_ihex(source)
        self.assertEqual(program[0:2], b"\x01\x02")
        self.assertEqual(program[0x10], 0xAA)
        self.assertEqual(len(program), 32 * 1024)

    def test_ihex_parser_rejects_an_out_of_range_program(self):
        with self.assertRaises(ardens_save.SaveError):
            ardens_save.parse_ihex(":020000040001F9\n:01000000AA55\n")

    def test_migrate_rewrites_only_the_hash_field(self):
        BUILD.mkdir(parents=True, exist_ok=True)
        source = BUILD / "absim_0000000000000001.save"
        payload = b"opaque-payload"
        source.write_bytes(struct.pack("<Q", 1) + payload)

        target = ardens_save.migrate_save(source, BUILD, 0x0123456789ABCDEF, force=True)

        self.assertEqual(target.name, "absim_0123456789abcdef.save")
        self.assertEqual(ardens_save.read_hash(target.read_bytes()), 0x0123456789ABCDEF)
        self.assertEqual(target.read_bytes()[8:], payload)
        self.assertEqual(ardens_save.read_hash(source.read_bytes()), 1)
        self.assertEqual(source.read_bytes()[8:], payload)


if __name__ == "__main__":
    unittest.main()
