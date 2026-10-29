#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_SHOP

#include "../src/game.hpp"
#include "../src/shop.hpp"
#include "../src/shop_view.hpp"
#if SH_FEATURE_HULL_DAMAGE
#include "../src/damage.hpp"
#endif
#include "test.hpp"

// Pure shop transactions plus the host-checkable board layout. The device draw
// path is compiled only under __AVR__, matching the station/cargo view tests.
namespace shop_test {

inline void dockedState(game::State &state, uint16_t credits) {
    game::reset(state);
    state.docked = 0;
    state.credits = credits;
}

}   // namespace shop_test

inline void ShopSuite(TestRunner &runner) {
    using namespace shop_test;

    {
        Test test("shop table: two items, max level 3, positive flat prices");
        test.assertEqual(shop::kItemCount, 2, "two shop rows");
        test.assertEqual(shop::kMaxLevel, 3, "three upgrade levels");
        test.assertEqual(shop::priceFor(shop::kAir), 60, "air tank price");
        test.assertEqual(shop::priceFor(shop::kHull), 70, "hull plate price");
        test.assertTrue(shop::priceFor(shop::kAir) > 0, "air is not free");
        runner.add(test);
    }

    {
        Test test("purchase deducts credits and updates each level");
        game::State state;
        dockedState(state, 500);

        test.assertEqual(shop::canBuy(state, shop::kAir), shop::kBought, "air is buyable");
        test.assertEqual(shop::buy(state, shop::kAir), shop::kBought, "air purchase succeeds");
        test.assertEqual(state.credits, 440, "air price is deducted");
        test.assertEqual(state.airLevel, 1, "air level advances");
        test.assertEqual(state.air, game::airCapacity(state), "air purchase fills the upgraded tank");

        test.assertEqual(shop::buy(state, shop::kHull), shop::kBought, "hull purchase succeeds");
        test.assertEqual(state.credits, 370, "hull price is deducted");
        test.assertEqual(state.hullLevel, 1, "hull level advances");
#if SH_FEATURE_HULL_DAMAGE
        test.assertEqual(state.hull, game::hullCapacity(state), "hull purchase repairs to the upgraded max");
#endif
        runner.add(test);
    }

    {
        Test test("insufficient credits is atomic");
        game::State state;
        dockedState(state, shop::priceFor(shop::kAir) - 1);
        state.air = 7;

        test.assertEqual(shop::canBuy(state, shop::kAir), shop::kNoCredits, "one credit short is rejected");
        test.assertEqual(shop::buy(state, shop::kAir), shop::kNoCredits, "buy returns the same failure");
        test.assertEqual(state.credits, shop::priceFor(shop::kAir) - 1, "credits are unchanged");
        test.assertEqual(state.airLevel, 0, "level is unchanged");
        test.assertEqual(state.air, 7, "air is not refilled on failure");
        runner.add(test);
    }

    {
        Test test("exact price boundary buys and leaves zero credits");
        game::State state;
        dockedState(state, shop::priceFor(shop::kHull));
        test.assertEqual(shop::buy(state, shop::kHull), shop::kBought, "credits exactly equal to price are enough");
        test.assertEqual(state.credits, 0, "balance reaches zero");
        test.assertEqual(state.hullLevel, 1, "level still advances");

        game::State shortState;
        dockedState(shortState, shop::priceFor(shop::kHull) - 1);
        test.assertEqual(shop::buy(shortState, shop::kHull), shop::kNoCredits, "one below the price is rejected");
        test.assertEqual(shortState.credits, shop::priceFor(shop::kHull) - 1, "credits stay below the price");
        runner.add(test);
    }

    {
        Test test("max level is sold out and never exceeds three");
        game::State state;
        dockedState(state, 1000);
        for (uint8_t i = 0; i < shop::kMaxLevel; ++i)
            test.assertEqual(shop::buy(state, shop::kAir), shop::kBought, "each air level buys");

        const uint16_t credits = state.credits;
        test.assertEqual(state.airLevel, shop::kMaxLevel, "air reaches the level cap");
        test.assertEqual(state.air, game::kAirCapacity + 3 * game::kAirCapacityPerLevel, "capacity includes all levels");
        test.assertEqual(shop::canBuy(state, shop::kAir), shop::kSoldOut, "a maxed item is sold out");
        test.assertEqual(shop::buy(state, shop::kAir), shop::kSoldOut, "buy at max is rejected");
        test.assertEqual(state.airLevel, shop::kMaxLevel, "level does not exceed the cap");
        test.assertEqual(state.credits, credits, "sold-out purchase spends nothing");
        runner.add(test);
    }

    {
        Test test("air and hull capacities follow the purchased levels");
        game::State state;
        game::reset(state);
        test.assertEqual(state.airLevel, 0, "reset clears air level");
        test.assertEqual(state.hullLevel, 0, "reset clears hull level");
        test.assertEqual(game::airCapacity(state), game::kAirCapacity, "base air capacity at level 0");
#if SH_FEATURE_HULL_DAMAGE
        test.assertEqual(game::hullCapacity(state), game::kHullMax, "base hull capacity at level 0");
#endif

        state.airLevel = 2;
        test.assertEqual(game::airCapacity(state), game::kAirCapacity + 2 * game::kAirCapacityPerLevel, "air tank adds 30 per level");
#if SH_FEATURE_HULL_DAMAGE
        state.hullLevel = 2;
        test.assertEqual(game::hullCapacity(state), game::kHullMax + 2 * game::kHullCapacityPerLevel, "hull plate adds one per level");
        state.hull = 1;
        damage::repairFull(state);
        test.assertEqual(state.hull, game::hullCapacity(state), "repair full uses the upgraded hull cap");
#endif
        runner.add(test);
    }

    {
        Test test("retired sonar leaves no shop row, price or level");
        test.assertEqual(shop::kItemCount, 2, "table stops after hull plate");
        test.assertEqual(shop::priceFor(static_cast<shop::Item>(shop::kItemCount)), 0, "past the table has no price");
        game::State state;
        dockedState(state, 500);
        test.assertEqual(shop::levelFor(state, static_cast<shop::Item>(shop::kItemCount)), shop::kMaxLevel, "past the table has no level");
        test.assertEqual(shop::canBuy(state, static_cast<shop::Item>(shop::kItemCount)), shop::kInvalidItem, "past the table cannot be bought");
        test.assertEqual(shop::selectNext(shop::kHull), shop::kFirstItem, "HULL PLATE wraps to AIR TANK");
        test.assertEqual(shop::selectPrevious(shop::kFirstItem), shop::kHull, "AIR TANK wraps to HULL PLATE");
        runner.add(test);
    }

    {
        Test test("shop transactions require a berth and a valid item");
        game::State state;
        game::reset(state);
        state.credits = 500;
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "reset leaves free flight");
        test.assertEqual(shop::canBuy(state, shop::kAir), shop::kNotDocked, "free flight cannot buy");
        test.assertEqual(shop::buy(state, shop::kAir), shop::kNotDocked, "free-flight buy is rejected");
        test.assertEqual(state.airLevel, 0, "rejected buy leaves the level alone");

        state.docked = 0;
        test.assertEqual(shop::buy(state, static_cast<shop::Item>(shop::kItemCount)), shop::kInvalidItem,
                         "an item index past the table is rejected");
        runner.add(test);
    }

    {
        Test test("shop selection wraps over two rows");
        test.assertEqual(shop::selectNext(0), 1, "down from air");
        test.assertEqual(shop::selectNext(1), 0, "down from hull wraps");
        test.assertEqual(shop::selectPrevious(0), 1, "up from air wraps");
        test.assertEqual(shop::selectPrevious(1), 0, "up from hull");
        runner.add(test);
    }

    {
        Test test("shop board rows fit above the hint at 128x64");
        test.assertEqual(shop_view::kRowY0, 22, "rows start at y=22");
        test.assertEqual(shop_view::kRowStep, 7, "rows are 7 px apart");
        test.assertEqual(shop_view::kRowInvertH, 7, "invert bar is 7 px tall");
        test.assertEqual(shop_view::kHintY, 57, "hint starts at y=57");
        test.assertEqual(shop_view::rowY(0), 22, "air row");
        test.assertEqual(shop_view::rowY(1), 29, "hull row");
        test.assertTrue(shop_view::rowY(0) - shop_view::kRowInsetY >= shop_view::kStatusY + 7, "first row clears status");
        test.assertTrue(shop_view::rowY(shop::kItemCount - 1) + shop_view::kRowInvertH <= shop_view::kHintY, "last row clears the hint");
        test.assertTrue(shop_view::kHintY + 7 <= 64, "hint fits inside 64 rows");
        test.assertTrue(shop_view::kCreditsX + 8 * 6 <= 128, "credit balance fits at the right edge");
        test.assertTrue(shop_view::kPriceX + 3 * 6 <= 128, "MAX fits inside 128 columns");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_SHOP
