#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include "luatest.hpp"

BOOST_AUTO_TEST_SUITE (MidiScriptTests)

namespace bdata = boost::unit_test::data;
BOOST_DATA_TEST_CASE (
    NoteOn,
    bdata::make ({ "return midi.noteon(1, 60, 100)" })
        ^ bdata::make ({ []() -> juce::MidiMessage { return juce::MidiMessage::noteOn (1, 60, (uint8_t) 100u); } }),
    script, juceCounterpart)
{
    LuaFixture fix;
    sol::state_view lua (fix.luaState());
    lua.script ("midi = require('el.midi')");

    int64_t packed = lua.script (script);

    auto expected = juceCounterpart();
    BOOST_CHECK_EQUAL (uint8_t (packed), expected.getRawData()[0]);
    BOOST_CHECK_EQUAL (uint8_t (packed >> 8), expected.getRawData()[1]);
    BOOST_CHECK_EQUAL (uint8_t (packed >> 16), expected.getRawData()[2]);
}

BOOST_AUTO_TEST_SUITE_END()