// Host test entry point. `make test` compiles this with the native compiler.
#include "audio_reference_test.hpp"
#include "adpcm_test.hpp"
#include "cave_test.hpp"
#include "collision_test.hpp"
#include "camera_motion_test.hpp"
#include "damage_test.hpp"
#include "features.hpp"
#include "game_test.hpp"
#include "music_logic_test.hpp"
#include "streamfx_loop_test.hpp"
#include "pause_test.hpp"
#include "pixel_layer_test.hpp"
#include "pilot_view_test.hpp"
#include "roster_test.hpp"
#include "roster_view_test.hpp"
#include "test.hpp"
#include "menu_panel_test.hpp"
#include "world_test.hpp"
#include "exploration_test.hpp"
#include "dev_mode_test.hpp"

#if SH_FEATURE_MINES
#include "hazards_test.hpp"
#endif
#if SH_FEATURE_PARTICLES
#include "particles_test.hpp"
#endif
#if SH_FEATURE_CURRENTS
#include "currents_test.hpp"
#endif

#if SH_FEATURE_FOG_CHART
#include "chart_test.hpp"
#include "chart_view_test.hpp"
#endif
#include "chart_save_test.hpp"
#include "voyage_save_test.hpp"
#include "voyage_flow_test.hpp"
#include "voyage_view_test.hpp"

#if SH_FEATURE_STATIONS
#include "station_test.hpp"
#endif
#if SH_FEATURE_SHOP
#include "shop_test.hpp"
#endif
#if SH_FEATURE_JOBS
#include "jobs_test.hpp"
#include "work_view_test.hpp"
#endif
#if SH_FEATURE_CAMPAIGN
#include "campaign_route_test.hpp"
#include "campaign_test.hpp"
#include "campaign_view_test.hpp"
#include "ending_view_test.hpp"
#endif
#if SH_FEATURE_STATIONS && SH_FEATURE_JOBS
#include "loop_test.hpp"
#endif

int main() {
    TestRunner runner;
    MenuPanelSuite(runner);
#if SH_DEV_MODE
    DevModeSuite(runner);
#endif
    audio_reference_test::suite(runner);
    adpcm_test::suite(runner);
    CaveSuite(runner);
    CollisionSuite(runner);
    CameraMotionSuite(runner);
    DamageSuite(runner);
    PauseSuite(runner);
    GameSuite(runner);
    MusicLogicSuite(runner);
    streamfx_loop_test::suite(runner);
    PilotViewSuite(runner);
    PixelLayerSuite(runner);
    RosterSuite(runner);
#if SH_FEATURE_MULTI_HULL
    TitleSuite(runner);
#endif
    WorldSuite(runner);
#if SH_FEATURE_EXPLORATION
    ExplorationSuite(runner);
#endif
#if SH_FEATURE_MINES
    HazardsSuite(runner);
#endif
#if SH_FEATURE_PARTICLES
    ParticlesSuite(runner);
#endif
#if SH_FEATURE_CURRENTS
    CurrentsSuite(runner);
#endif
#if SH_FEATURE_FOG_CHART
    ChartSuite(runner);
    ChartViewSuite(runner);
#endif
    ChartSaveSuite(runner);
    VoyageSaveSuite(runner);
    VoyageFlowSuite(runner);
    VoyageViewSuite(runner);
#if SH_FEATURE_STATIONS
    StationSuite(runner);
#endif
#if SH_FEATURE_SHOP
    ShopSuite(runner);
#endif
#if SH_FEATURE_JOBS
    JobsSuite(runner);
    WorkViewSuite(runner);
#endif
#if SH_FEATURE_CAMPAIGN
    CampaignSuite(runner);
    CampaignViewSuite(runner);
    CampaignRouteSuite(runner);
    EndingViewSuite(runner);
#endif
#if SH_FEATURE_STATIONS && SH_FEATURE_JOBS
    LoopSuite(runner);
#endif
    return runner.printSummary();
}
