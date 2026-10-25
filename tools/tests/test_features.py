import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import features  # noqa: E402

FEATURES_PY = ROOT / "tools" / "features.py"


def run_features(*args):
    return subprocess.run(
        [sys.executable, str(FEATURES_PY), *args],
        capture_output=True,
        text=True,
    )


class ProfileValuesTest(unittest.TestCase):
    def test_full_profile_values(self):
        self.assertEqual(
            features.profile_values("full"),
            {
                "STATIONS": 1,
                "JOBS": 1,
                "FOG_CHART": 1,
                "SONAR": 1,
                "MINES": 1,
                "CURRENTS": 1,
                "SHOP": 1,
                "HULL_DAMAGE": 1,
                "PARTICLES": 1,
                "MULTI_HULL": 1,
                "SAVE_CHART": 0,
            },
        )

    def test_lean_profile_values(self):
        self.assertEqual(
            features.profile_values("lean"),
            {
                "STATIONS": 1,
                "JOBS": 1,
                "FOG_CHART": 1,
                "SONAR": 1,
                "MINES": 0,
                "CURRENTS": 0,
                "SHOP": 0,
                "HULL_DAMAGE": 1,
                "PARTICLES": 0,
                "MULTI_HULL": 1,
                "SAVE_CHART": 0,
            },
        )

    def test_core_profile_values(self):
        self.assertEqual(
            features.profile_values("core"),
            {
                "STATIONS": 1,
                "JOBS": 1,
                "FOG_CHART": 0,
                "SONAR": 0,
                "MINES": 0,
                "CURRENTS": 0,
                "SHOP": 0,
                "HULL_DAMAGE": 1,
                "PARTICLES": 0,
                "MULTI_HULL": 0,
                "SAVE_CHART": 0,
            },
        )

    def test_min_profile_values(self):
        self.assertEqual(
            features.profile_values("min"),
            {
                "STATIONS": 0,
                "JOBS": 0,
                "FOG_CHART": 0,
                "SONAR": 0,
                "MINES": 0,
                "CURRENTS": 0,
                "SHOP": 0,
                "HULL_DAMAGE": 0,
                "PARTICLES": 0,
                "MULTI_HULL": 0,
                "SAVE_CHART": 0,
            },
        )

    def test_every_profile_defines_every_flag_and_validates(self):
        for profile in features.PROFILE_ORDER:
            with self.subTest(profile=profile):
                values = features.profile_values(profile)
                self.assertEqual(tuple(values), features.FEATURES)
                self.assertIsNone(features.validate_features(values))

    def test_profile_values_are_fresh_copies(self):
        values = features.profile_values("full")
        values["MINES"] = 0
        self.assertEqual(features.profile_values("full")["MINES"], 1)

    def test_unknown_profile_is_rejected(self):
        with self.assertRaisesRegex(features.FeatureError, "unknown profile 'bogus'"):
            features.profile_values("bogus")


class DependencyTest(unittest.TestCase):
    def test_dependencies_are_reported_with_header_names(self):
        cases = [
            ("JOBS", "STATIONS"),
            ("SONAR", "FOG_CHART"),
            ("SAVE_CHART", "FOG_CHART"),
            ("SHOP", "STATIONS"),
        ]
        for feature, requirement in cases:
            with self.subTest(feature=feature, requirement=requirement):
                values = features.profile_values("full")
                values["SAVE_CHART"] = 0
                values[feature] = 1
                values[requirement] = 0
                with self.assertRaises(features.FeatureError) as raised:
                    features.validate_features(values)
                self.assertIn(
                    "SH_FEATURE_{} requires SH_FEATURE_{}".format(
                        feature, requirement
                    ),
                    str(raised.exception),
                )

    def test_missing_flag_is_rejected(self):
        values = features.profile_values("lean")
        del values["SONAR"]
        with self.assertRaisesRegex(features.FeatureError, "missing feature 'SONAR'"):
            features.validate_features(values)

    def test_non_binary_flag_is_rejected(self):
        values = features.profile_values("lean")
        values["MINES"] = 2
        with self.assertRaisesRegex(features.FeatureError, "must be 0 or 1"):
            features.validate_features(values)

    def test_boolean_flag_is_rejected(self):
        values = features.profile_values("lean")
        values["MINES"] = True
        with self.assertRaisesRegex(features.FeatureError, "must be 0 or 1"):
            features.validate_features(values)

    def test_override_accepts_prefixed_names(self):
        overrides = features.parse_overrides(
            ["MINES=0", "SH_FEATURE_PARTICLES=0"]
        )
        self.assertEqual(overrides, {"MINES": 0, "PARTICLES": 0})

    def test_override_rejects_unknown_feature(self):
        with self.assertRaisesRegex(features.FeatureError, "unknown feature 'WARP'"):
            features.parse_overrides(["WARP=1"])

    def test_override_rejects_non_binary_value(self):
        with self.assertRaisesRegex(features.FeatureError, "must be 0 or 1"):
            features.parse_overrides(["MINES=yes"])

    def test_override_rejects_malformed_assignment(self):
        with self.assertRaisesRegex(features.FeatureError, "invalid --set"):
            features.parse_overrides(["MINES"])


class FlagOutputTest(unittest.TestCase):
    def test_flags_are_all_explicit_and_numeric(self):
        flags = features.feature_flags(features.profile_values("full")).split()
        self.assertEqual(len(flags), len(features.FEATURES))
        self.assertEqual(
            flags,
            [
                "-DSH_FEATURE_STATIONS=1",
                "-DSH_FEATURE_JOBS=1",
                "-DSH_FEATURE_FOG_CHART=1",
                "-DSH_FEATURE_SONAR=1",
                "-DSH_FEATURE_MINES=1",
                "-DSH_FEATURE_CURRENTS=1",
                "-DSH_FEATURE_SHOP=1",
                "-DSH_FEATURE_HULL_DAMAGE=1",
                "-DSH_FEATURE_PARTICLES=1",
                "-DSH_FEATURE_MULTI_HULL=1",
                "-DSH_FEATURE_SAVE_CHART=0",
            ],
        )

    def test_report_lists_active_and_inactive(self):
        report = features.feature_report(
            "lean", features.profile_values("lean")
        )
        self.assertIn("profile: lean", report)
        self.assertIn(
            "active (6): STATIONS JOBS FOG_CHART SONAR HULL_DAMAGE MULTI_HULL",
            report,
        )
        self.assertIn(
            "inactive (5): MINES CURRENTS SHOP PARTICLES SAVE_CHART", report
        )

    def test_min_report_has_no_active_features(self):
        report = features.feature_report("min", features.profile_values("min"))
        self.assertIn("active (0): (none)", report)
        self.assertIn(
            "inactive (11): STATIONS JOBS FOG_CHART SONAR "
            "MINES CURRENTS SHOP HULL_DAMAGE PARTICLES MULTI_HULL SAVE_CHART",
            report,
        )


class CommandLineTest(unittest.TestCase):
    def test_check_accepts_every_profile(self):
        for profile in features.PROFILE_ORDER:
            with self.subTest(profile=profile):
                result = run_features("--profile", profile, "--check")
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("profile {} valid".format(profile), result.stdout)

    def test_flags_prints_one_line_for_make(self):
        result = run_features("--profile", "lean", "--flags")
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.strip().splitlines()
        self.assertEqual(len(lines), 1)
        self.assertIn("-DSH_FEATURE_MINES=0", lines[0])
        self.assertIn("-DSH_FEATURE_FOG_CHART=1", lines[0])
        self.assertNotIn("\n", result.stdout.strip())

    def test_list_prints_profiles(self):
        result = run_features("--list")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            result.stdout.split(), list(features.PROFILE_ORDER)
        )

    def test_unknown_profile_exits_nonzero(self):
        result = run_features("--profile", "bogus", "--flags")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unknown profile 'bogus'", result.stderr)

    def test_invalid_dependency_exits_nonzero(self):
        result = run_features(
            "--profile", "full", "--set", "STATIONS=0", "--check"
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("SH_FEATURE_JOBS requires SH_FEATURE_STATIONS", result.stderr)

    def test_valid_custom_override_succeeds(self):
        result = run_features(
            "--profile", "full", "--set", "MINES=0", "--set", "PARTICLES=0", "--flags"
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("-DSH_FEATURE_MINES=0", result.stdout)
        self.assertIn("-DSH_FEATURE_PARTICLES=0", result.stdout)


class HeaderParityTest(unittest.TestCase):
    def test_script_matches_feature_header(self):
        header = (ROOT / "src" / "features.hpp").read_text()
        definition_order = tuple(
            dict.fromkeys(re.findall(r"#define SH_FEATURE_([A-Z0-9_]+)", header))
        )
        header_dependencies = {}
        for feature, requirement in re.findall(
            r"#if SH_FEATURE_([A-Z0-9_]+) && !SH_FEATURE_([A-Z0-9_]+)",
            header,
        ):
            header_dependencies.setdefault(feature, []).append(requirement)
        self.assertEqual(definition_order, features.FEATURES)
        self.assertEqual(
            {name: tuple(requirements) for name, requirements in header_dependencies.items()},
            features.DEPENDENCIES,
        )


if __name__ == "__main__":
    unittest.main()
