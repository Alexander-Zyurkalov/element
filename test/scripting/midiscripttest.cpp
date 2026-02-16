#include <boost/test/unit_test.hpp>
#include "luatest.hpp"

BOOST_AUTO_TEST_SUITE (MidiScriptTests)

BOOST_AUTO_TEST_CASE (NoteOn)
{
    LuaFixture fix;
    sol::state_view lua (fix.luaState());
    lua.script("midi = require('el.midi')");

    // Get the packed integer from Lua
    int64_t packed = lua.script("return midi.noteon(1, 60, 100)");

    // Compare against JUCE
    auto expected = juce::MidiMessage::noteOn(1, 60, (uint8_t)100);
    BOOST_REQUIRE(uint8_t(packed)       == expected.getRawData()[0]);
    BOOST_REQUIRE(uint8_t(packed >> 8)  == expected.getRawData()[1]);
    BOOST_REQUIRE(uint8_t(packed >> 16) == expected.getRawData()[2]);
}
BOOST_AUTO_TEST_SUITE_END()