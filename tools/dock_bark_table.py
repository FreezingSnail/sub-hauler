"""Shared dock-bark -> FX string order.

One source for the sixteen ``campaign::DockBark`` entries in declaration order
(index 0 is deliberately empty). ``tools/gen_dock_barks.py`` validates it
against the enum and uses it for the documentation preview; ``make gen`` bakes
the same order into the ``img_dock_barks`` stamp sheet, so the station board
and WORK clue select a frame with the raw ``DockBark`` value.
"""

DOCK_BARK_TEXT = (
    ("kDockBarkNone", None),
    ("kDockBarkFirst", "txt_first_clue"),
    ("kDockBarkBenthosLamps", "txt_benthos_lamps_on"),
    ("kDockBarkPumpSurvey", "txt_pump_survey_clue"),
    ("kDockBarkPumpSource", "txt_pump_source_clue"),
    ("kDockBarkLiftSurvey", "txt_survey_lift_clue"),
    ("kDockBarkLiftSource", "txt_lift_source_clue"),
    ("kDockBarkPumpPartReady", "txt_pump_part_ready"),
    ("kDockBarkLiftRestored", "txt_lift_restored"),
    ("kDockBarkBeaconNeedsSeal", "txt_beacon_needs_seal"),
    ("kDockBarkBeaconReady", "txt_beacon_ready"),
    ("kDockBarkBeaconPlan", "txt_beacon_clue"),
    ("kDockBarkBeaconLit", "txt_beacon_lit"),
    ("kDockBarkNetworkOn", "txt_network_on"),
    ("kDockBarkDeepRelay", "txt_deep_relay"),
    ("kDockBarkNoWork", "txt_no_work"),
)
