// Host test entry point. `make test` compiles this with the native compiler.
#include "audio_reference_test.hpp"
#include "adpcm_test.hpp"
#include "cave_test.hpp"
#include "collision_test.hpp"
#include "camera_motion_test.hpp"
#include "game_test.hpp"
#include "music_logic_test.hpp"
#include "pause_test.hpp"
#include "pixel_layer_test.hpp"
#include "pilot_view_test.hpp"
#include "roster_test.hpp"
#include "roster_view_test.hpp"
#include "test.hpp"
#include "menu_panel_test.hpp"
#include "world_test.hpp"
#include "exploration_test.hpp"

#include "particles_test.hpp"
#include "currents_test.hpp"

#include "chart_test.hpp"
#include "chart_view_test.hpp"
#include "voyage_save_test.hpp"
#include "voyage_flow_test.hpp"
#include "voyage_view_test.hpp"

#include "station_test.hpp"
#include "shop_test.hpp"
#include "jobs_test.hpp"
#include "work_view_test.hpp"
#include "campaign_route_test.hpp"
#include "campaign_test.hpp"
#include "campaign_view_test.hpp"
#include "story_view_test.hpp"
#include "ending_view_test.hpp"
#include "loop_test.hpp"

int main() {
    TestRunner runner;
    MenuPanelSuite(runner);
    audio_reference_test::suite(runner);
    adpcm_test::suite(runner);
    CaveSuite(runner);
    CollisionSuite(runner);
    CameraMotionSuite(runner);
    PauseSuite(runner);
    GameSuite(runner);
    MusicLogicSuite(runner);
    PilotViewSuite(runner);
    PixelLayerSuite(runner);
    RosterSuite(runner);
    TitleSuite(runner);
    WorldSuite(runner);
    ExplorationSuite(runner);
    ParticlesSuite(runner);
    CurrentsSuite(runner);
    ChartSuite(runner);
    ChartViewSuite(runner);
    VoyageSaveSuite(runner);
    VoyageFlowSuite(runner);
    VoyageViewSuite(runner);
    StationSuite(runner);
    ShopSuite(runner);
    JobsSuite(runner);
    WorkViewSuite(runner);
    CampaignSuite(runner);
    CampaignViewSuite(runner);
    StoryViewSuite(runner);
    CampaignRouteSuite(runner);
    EndingViewSuite(runner);
    LoopSuite(runner);
    return runner.printSummary();
}
