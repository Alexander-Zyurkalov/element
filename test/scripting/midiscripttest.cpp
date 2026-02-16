#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include "luatest.hpp"

BOOST_AUTO_TEST_SUITE (MidiScriptTests)

namespace bdata = boost::unit_test::data;
using JuceCounterpart = std::function<juce::MidiMessage()>;

struct LuaMidiTestCase {
    std::string luaMethodCall;
    JuceCounterpart juceCounterpart;

    friend std::ostream& operator<<(std::ostream& os, const LuaMidiTestCase& testCase) {
        os << "{ luaMethodCall: \"" << testCase.luaMethodCall << "\", ... } ";
        return os;
    }
};

BOOST_DATA_TEST_CASE (
    luaSimpleBindigs,
    bdata::make ({
        LuaMidiTestCase {
            "return midi.noteon(1, 60, 100)",
            []() -> juce::MidiMessage { return juce::MidiMessage::noteOn (1, 60, (uint8_t) 100u); }
        },

        LuaMidiTestCase {
            "return midi.noteoff(1, 60)",
            []() -> juce::MidiMessage { return juce::MidiMessage::noteOff (1, 60); }
        },

    }),
    testData)
{
    LuaFixture fix;
    sol::state_view lua (fix.luaState());
    lua.script ("midi = require('el.midi')");

    int64_t packed = lua.script (testData.luaMethodCall);

    auto expected = testData.juceCounterpart();
    BOOST_CHECK_EQUAL (uint8_t (packed), expected.getRawData()[0]);
    BOOST_CHECK_EQUAL (uint8_t (packed >> 8), expected.getRawData()[1]);
    BOOST_CHECK_EQUAL (uint8_t (packed >> 16), expected.getRawData()[2]);
}

BOOST_AUTO_TEST_SUITE_END()