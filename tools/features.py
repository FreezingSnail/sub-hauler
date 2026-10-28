#!/usr/bin/env python3
"""Feature profiles, dependency validation, and compiler flag emission.

This file is the single source of profile data. The Makefile and docs consume
it; they do not keep their own copies of the flag values.

Usage:
    tools/features.py --profile lean --flags
    tools/features.py --profile lean --report
    tools/features.py --profile full --check
    tools/features.py --profile full --set MINES=0 --set PARTICLES=0 --flags
    tools/features.py --list
"""

from __future__ import annotations

import argparse
import sys

PREFIX = "SH_FEATURE_"

# Declaration order mirrors src/features.hpp. Every flag is emitted explicitly
# as a numeric 0/1 macro so a profile cannot inherit a stale default.
FEATURES = (
    "STATIONS",
    "JOBS",
    "FOG_CHART",
    "SONAR",
    "MINES",
    "CURRENTS",
    "SHOP",
    "HULL_DAMAGE",
    "PARTICLES",
    "MULTI_HULL",
    "SAVE_CHART",
    "EXPLORATION",
    "CAMPAIGN",
    "SAVE_VOYAGE",
    "MUSIC",
)

# Direct dependency checks, matching src/features.hpp. Keep insertion order in
# sync with the #if blocks so error reporting is deterministic.
DEPENDENCIES = {
    "JOBS": ("STATIONS",),
    "SONAR": ("FOG_CHART",),
    "SAVE_CHART": ("FOG_CHART",),
    "SHOP": ("STATIONS",),
    "EXPLORATION": ("JOBS",),
    "CAMPAIGN": ("JOBS", "STATIONS", "EXPLORATION"),
    "SAVE_VOYAGE": ("CAMPAIGN", "!SAVE_CHART"),
    "MUSIC": ("SAVE_VOYAGE",),
}

PROFILES = {
    "full": {
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
        "EXPLORATION": 1,
        "CAMPAIGN": 1,
        "SAVE_VOYAGE": 1,
        "MUSIC": 1,
    },
    "lean": {
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
        "EXPLORATION": 1,
        "CAMPAIGN": 0,
        "SAVE_VOYAGE": 0,
        "MUSIC": 0,
    },
    "core": {
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
        "EXPLORATION": 1,
        "CAMPAIGN": 0,
        "SAVE_VOYAGE": 0,
        "MUSIC": 0,
    },
    "min": {
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
        "EXPLORATION": 0,
        "CAMPAIGN": 0,
        "SAVE_VOYAGE": 0,
        "MUSIC": 0,
    },
}

PROFILE_ORDER = tuple(PROFILES)


class FeatureError(ValueError):
    """Invalid profile, override, or feature dependency."""


def profile_values(name):
    """Return a fresh flag map for a named profile."""
    try:
        return dict(PROFILES[name])
    except KeyError:
        choices = ", ".join(PROFILE_ORDER)
        raise FeatureError(
            "unknown profile '{}'; choose from {}".format(name, choices)
        ) from None


def normalize_feature(name):
    """Accept either MINES or SH_FEATURE_MINES and return the bare name."""
    name = name.strip()
    if name.startswith(PREFIX):
        name = name[len(PREFIX) :]
    if name not in FEATURES:
        raise FeatureError(
            "unknown feature '{}'; choose from {}".format(name, ", ".join(FEATURES))
        )
    return name


def parse_overrides(assignments):
    """Parse repeated NAME=0|1 assignments into a flag map."""
    overrides = {}
    for assignment in assignments:
        if "=" not in assignment:
            raise FeatureError(
                "invalid --set '{}'; expected NAME=0 or NAME=1".format(assignment)
            )
        raw_name, raw_value = assignment.split("=", 1)
        name = normalize_feature(raw_name)
        value = raw_value.strip()
        if value not in ("0", "1"):
            raise FeatureError(
                "{} must be 0 or 1, got '{}'".format(PREFIX + name, value)
            )
        overrides[name] = int(value)
    return overrides


def apply_overrides(values, overrides):
    """Return values with the custom flag map applied."""
    merged = dict(values)
    merged.update(overrides)
    return merged


def validate_features(values):
    """Validate a complete flag map against the header dependency rules."""
    unknown = [name for name in values if name not in FEATURES]
    if unknown:
        raise FeatureError(
            "unknown feature '{}'; choose from {}".format(
                unknown[0], ", ".join(FEATURES)
            )
        )

    missing = [name for name in FEATURES if name not in values]
    if missing:
        raise FeatureError(
            "missing feature '{}'; profiles must define every flag".format(missing[0])
        )

    for name in FEATURES:
        value = values[name]
        if value not in (0, 1) or isinstance(value, bool):
            raise FeatureError(
                "{} must be 0 or 1, got {!r}".format(PREFIX + name, value)
            )

    problems = []
    for feature, requirements in DEPENDENCIES.items():
        if not values[feature]:
            continue
        for requirement in requirements:
            if requirement.startswith("!"):
                name = requirement[1:]
                if values[name]:
                    problems.append(
                        "{} requires !{}".format(
                            PREFIX + feature, PREFIX + name
                        )
                    )
                continue
            if not values[requirement]:
                problems.append(
                    "{} requires {}".format(PREFIX + feature, PREFIX + requirement)
                )
    if problems:
        raise FeatureError("invalid feature combination: " + "; ".join(problems))


def feature_flags(values):
    """Return one line of -D flags for every feature, in declaration order."""
    validate_features(values)
    return " ".join(
        "-D{}{}={}".format(PREFIX, name, values[name]) for name in FEATURES
    )


def feature_report(profile, values):
    """Return a human-readable active/inactive summary for a profile."""
    validate_features(values)
    active = [name for name in FEATURES if values[name]]
    inactive = [name for name in FEATURES if not values[name]]
    lines = [
        "profile: {}".format(profile),
        "active ({}): {}".format(len(active), " ".join(active) or "(none)"),
        "inactive ({}): {}".format(len(inactive), " ".join(inactive) or "(none)"),
        "flags: {}".format(feature_flags(values)),
    ]
    return "\n".join(lines)


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Validate a feature profile and emit compiler flags."
    )
    parser.add_argument(
        "--profile",
        default="full",
        metavar="NAME",
        help="profile to use: {} (default: full)".format(", ".join(PROFILE_ORDER)),
    )
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument(
        "--flags", action="store_true", help="print -D flags for the Makefile"
    )
    actions.add_argument(
        "--report", action="store_true", help="print active and inactive flags"
    )
    actions.add_argument(
        "--check", action="store_true", help="validate only (default)"
    )
    actions.add_argument(
        "--list", action="store_true", help="list profile names"
    )
    parser.add_argument(
        "--set",
        dest="overrides",
        action="append",
        default=[],
        metavar="NAME=0|1",
        help="override one feature after loading the profile (repeatable)",
    )
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    try:
        if args.list:
            print("\n".join(PROFILE_ORDER))
            return 0

        values = apply_overrides(
            profile_values(args.profile), parse_overrides(args.overrides)
        )
        validate_features(values)

        if args.flags:
            print(feature_flags(values))
        elif args.report:
            print(feature_report(args.profile, values))
        else:
            print(
                "features: profile {} valid ({} flags)".format(
                    args.profile, len(FEATURES)
                )
            )
        return 0
    except FeatureError as exc:
        print("features.py: error: {}".format(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
