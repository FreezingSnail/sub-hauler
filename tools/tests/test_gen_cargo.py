import sys
import unittest
from dataclasses import replace
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import gen_cargo  # noqa: E402
from gen_cargo import CargoError, Hull, Piece  # noqa: E402


REQUIRED_HOLDS = {
    "MINNOW": {
        "cols": 6,
        "rows": 3,
        "blocked": {(0, 0), (5, 0), (0, 2), (5, 2)},
        "load": 12,
        "required": 3,
        "bonus": 1,
        "max_area": 4,
        "max_w": 3,
        "max_h": 2,
        "record_bytes": 12,
        "offset": 0,
    },
    "HAULER": {
        "cols": 11,
        "rows": 3,
        "blocked": {(0, 0), (5, 0), (5, 1), (5, 2), (10, 0)},
        "load": 26,
        "required": 6,
        "bonus": 1,
        "max_area": 6,
        "max_w": 4,
        "max_h": 3,
        "record_bytes": 21,
        "offset": 768,
    },
    "LEVIATHAN": {
        "cols": 14,
        "rows": 4,
        "blocked": {
            (0, 0),
            (1, 0),
            (6, 0),
            (6, 1),
            (6, 2),
            (6, 3),
            (7, 0),
            (7, 1),
            (7, 2),
            (7, 3),
            (12, 0),
            (13, 0),
        },
        "load": 40,
        "required": 6,
        "bonus": 2,
        "max_area": 8,
        "max_w": 4,
        "max_h": 4,
        "record_bytes": 24,
        "offset": 2112,
    },
}


def test_hull() -> Hull:
    """Small synthetic hull for validator-rule tests."""
    return Hull(
        name="TEST",
        cols=2,
        rows=2,
        blocked=frozenset(),
        load=2,
        required=1,
        bonus=0,
        max_area=2,
        max_w=2,
        max_h=2,
        seed=1,
    )


class HullContractTest(unittest.TestCase):
    def test_hold_masks_match_the_reference_roster(self):
        self.assertEqual(tuple(h.name for h in gen_cargo.HULLS), tuple(REQUIRED_HOLDS))
        for hull in gen_cargo.HULLS:
            expected = REQUIRED_HOLDS[hull.name]
            self.assertEqual(hull.cols, expected["cols"], hull.name)
            self.assertEqual(hull.rows, expected["rows"], hull.name)
            self.assertEqual(hull.blocked, expected["blocked"], hull.name)
            self.assertEqual(hull.load, expected["load"], hull.name)
            self.assertEqual(hull.required, expected["required"], hull.name)
            self.assertEqual(hull.bonus, expected["bonus"], hull.name)
            self.assertEqual(hull.max_area, expected["max_area"], hull.name)
            self.assertEqual(hull.max_w, expected["max_w"], hull.name)
            self.assertEqual(hull.max_h, expected["max_h"], hull.name)
            self.assertEqual(hull.pieces, expected["required"] + expected["bonus"])
            self.assertEqual(hull.record_bytes, expected["record_bytes"], hull.name)
            self.assertEqual(gen_cargo.hull_offset(hull), expected["offset"])

    def test_table_size_matches_the_64_arrangement_contract(self):
        expected = sum(
            gen_cargo.ARRANGEMENTS_PER_HULL * hull.record_bytes
            for hull in gen_cargo.HULLS
        )
        self.assertEqual(expected, 3648)

    def test_checked_rejects_broken_hull_specs(self):
        base = test_hull()
        with self.assertRaises(CargoError):
            gen_cargo.checked(replace(base, blocked=frozenset({(2, 0)})))
        with self.assertRaises(CargoError):
            gen_cargo.checked(replace(base, load=5))
        with self.assertRaises(CargoError):
            gen_cargo.checked(replace(base, max_w=5))
        with self.assertRaises(CargoError):
            gen_cargo.checked(replace(base, required=0, bonus=0, load=1))


class GeneratedBankTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bank = gen_cargo.generate_bank()
        cls.header = gen_cargo.build_header(len(cls.bank.blob))

    def test_bank_size_and_hull_blocks(self):
        self.assertEqual(len(self.bank.blob), 3648)
        for stats in self.bank.stats:
            expected = REQUIRED_HOLDS[stats.name]
            self.assertEqual(stats.offset, expected["offset"])
            self.assertEqual(stats.record_bytes, expected["record_bytes"])
            self.assertEqual(stats.arrangements, 64)
            self.assertEqual(stats.pieces, expected["required"] + expected["bonus"])

    def test_every_arrangement_is_complete_and_valid(self):
        arrangements = 0
        records = 0
        for hull in gen_cargo.HULLS:
            pieces_list = self.bank.arrangements[hull.name]
            self.assertEqual(len(pieces_list), 64)
            for pieces in pieces_list:
                gen_cargo.validate_arrangement(hull, pieces)
                self.assertEqual(
                    sum(gen_cargo.mask_area(piece.mask) for piece in pieces),
                    hull.load,
                )
                self.assertEqual(
                    sum(1 for piece in pieces if not piece.bonus), hull.required
                )
                self.assertEqual(sum(1 for piece in pieces if piece.bonus), hull.bonus)
                arrangements += 1
                records += len(pieces)
        self.assertEqual(arrangements, 192)
        self.assertEqual(records, 1216)

    def test_packed_blob_decodes_to_the_same_arrangements(self):
        for stats in self.bank.stats:
            hull = gen_cargo.HULL_BY_NAME[stats.name]
            for index in range(64):
                start = stats.offset + index * stats.record_bytes
                data = self.bank.blob[start : start + stats.record_bytes]
                decoded = gen_cargo.decode_arrangement(hull, data)
                self.assertEqual(decoded, self.bank.arrangements[stats.name][index])
                gen_cargo.validate_arrangement(hull, decoded)
                reencoded = gen_cargo.encode_arrangement(hull, decoded)
                self.assertEqual(reencoded, data)

    def test_no_duplicate_arrangements_per_hull(self):
        for hull in gen_cargo.HULLS:
            signatures = {
                gen_cargo._signature(pieces)
                for pieces in self.bank.arrangements[hull.name]
            }
            self.assertEqual(len(signatures), 64, hull.name)

    def test_generation_is_deterministic_on_disk(self):
        first_bin = ROOT / "build" / "tests" / "cargo-a.bin"
        first_header = ROOT / "build" / "tests" / "cargo-a.hpp"
        second_bin = ROOT / "build" / "tests" / "cargo-b.bin"
        second_header = ROOT / "build" / "tests" / "cargo-b.hpp"
        gen_cargo.write_outputs(first_bin, first_header)
        gen_cargo.write_outputs(second_bin, second_header)
        self.assertEqual(first_bin.read_bytes(), second_bin.read_bytes())
        self.assertEqual(first_header.read_text(), second_header.read_text())
        self.assertEqual(first_bin.read_bytes(), self.bank.blob)

    def test_header_documents_format_version_and_offsets(self):
        text = self.header
        for needle in (
            "CARGO_FORMAT_VERSION = 1",
            "CARGO_ARRANGEMENTS_PER_HULL = 64",
            "CARGO_TABLE_BYTES = 3648",
            "CARGO_PIECE_BYTES = 3",
            "CARGO_MINNOW_OFFSET = 0",
            "CARGO_MINNOW_RECORD_BYTES = 12",
            "CARGO_HAULER_OFFSET = 768",
            "CARGO_HAULER_RECORD_BYTES = 21",
            "CARGO_LEVIATHAN_OFFSET = 2112",
            "CARGO_LEVIATHAN_RECORD_BYTES = 24",
            "CARGO_POS_Y_SHIFT = 4",
            "CARGO_POS_SPARE = 0x80",
        ):
            self.assertIn(needle, text)
        self.assertIn("Do not edit", text)

    def test_pos_kind_roundtrip(self):
        for x in range(gen_cargo.MAX_X + 1):
            for y in range(gen_cargo.MAX_Y + 1):
                for bonus in (False, True):
                    piece = Piece(0x1, x, y, bonus)
                    self.assertEqual(
                        gen_cargo.decode_pos(gen_cargo.encode_pos(piece)),
                        (x, y, bonus),
                    )


class RejectedArrangementTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bank = gen_cargo.generate_bank()
        cls.hull = gen_cargo.HULL_BY_NAME["MINNOW"]
        cls.pieces = cls.bank.arrangements["MINNOW"][0]

    def mutate(self, index, **changes):
        pieces = list(self.pieces)
        pieces[index] = replace(pieces[index], **changes)
        return pieces

    def assert_rejected(self, pieces, needle):
        with self.assertRaises(CargoError) as caught:
            gen_cargo.validate_arrangement(self.hull, pieces)
        self.assertIn(needle, str(caught.exception))

    def test_rejects_disconnected_mask(self):
        self.assert_rejected(self.mutate(0, mask=0x1001), "disconnected")

    def test_rejects_unnormalized_mask(self):
        self.assert_rejected(self.mutate(0, mask=0x222), "not normalized")

    def test_rejects_oversized_area(self):
        self.assert_rejected(self.mutate(0, mask=0x1F), "area")

    def test_rejects_oversized_bbox(self):
        self.assert_rejected(self.mutate(0, mask=0x111), "bbox")

    def test_rejects_overlap(self):
        pieces = list(self.pieces)
        pieces[1] = replace(pieces[1], x=pieces[0].x, y=pieces[0].y)
        self.assert_rejected(pieces, "overlap")

    def test_rejects_out_of_bounds_piece(self):
        self.assert_rejected(self.mutate(0, x=self.hull.cols), "out of bounds")

    def test_rejects_blocked_cell(self):
        pieces = list(self.pieces)
        pieces[-1] = Piece(mask=0x1, x=5, y=2, bonus=True)
        self.assert_rejected(pieces, "blocked")

    def test_rejects_wrong_piece_count(self):
        self.assert_rejected(self.pieces[:-1], "expected 4 pieces")

    def test_rejects_wrong_required_bonus_split(self):
        self.assert_rejected(self.mutate(0, bonus=True), "got 2/2")

    def test_rejects_required_after_bonus(self):
        pieces = list(self.pieces)
        pieces[0], pieces[-1] = pieces[-1], pieces[0]
        self.assert_rejected(pieces, "required piece appears after")

    def test_rejects_exact_load_mismatch(self):
        hull = test_hull()
        short = [Piece(mask=0x1, x=0, y=1, bonus=False)]
        with self.assertRaises(CargoError) as caught:
            gen_cargo.validate_arrangement(hull, short)
        self.assertIn("expected load 2", str(caught.exception))

    def test_rejects_unsupported_floating_cargo(self):
        hull = replace(test_hull(), cols=1, load=1, max_w=1)
        floating = [Piece(mask=0x1, x=0, y=0, bonus=False)]
        with self.assertRaises(CargoError) as caught:
            gen_cargo.validate_arrangement(hull, floating)
        self.assertIn("unsupported", str(caught.exception))

    def test_rejects_record_with_reserved_pos_bit(self):
        hull = gen_cargo.HULL_BY_NAME["MINNOW"]
        record = bytearray(
            gen_cargo.encode_arrangement(hull, self.pieces)
        )
        record[2] |= gen_cargo.POS_SPARE
        with self.assertRaises(CargoError) as caught:
            gen_cargo.decode_arrangement(hull, bytes(record))
        self.assertIn("reserved", str(caught.exception))

    def test_rejects_truncated_record(self):
        hull = gen_cargo.HULL_BY_NAME["MINNOW"]
        record = gen_cargo.encode_arrangement(hull, self.pieces)
        with self.assertRaises(CargoError):
            gen_cargo.decode_arrangement(hull, record[:-1])


if __name__ == "__main__":
    unittest.main()
