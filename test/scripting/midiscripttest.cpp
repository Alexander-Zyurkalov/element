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
            []() -> juce::MidiMessage { return juce::MidiMessage::noteOn (1, 60, (uint8_t) 100); }
        },

        LuaMidiTestCase {
            "return midi.noteoff(1, 60)",
            []() -> juce::MidiMessage { return juce::MidiMessage::noteOff (1, 60); }
        },

        LuaMidiTestCase {
            "return midi.controller(1, 7, 120)",
            []() -> juce::MidiMessage { return juce::MidiMessage::controllerEvent (1, 7, 120); }
        },

//        LuaMidiTestCase {
//            "return midi.program(1, 5)",
//            []() -> juce::MidiMessage { return juce::MidiMessage::programChange (1, 5); }
//        },
//
        LuaMidiTestCase {
            "return midi.pitch(1, 8192)",
            []() -> juce::MidiMessage { return juce::MidiMessage::pitchWheel (1, 8192); }
        },

        LuaMidiTestCase {
            "return midi.aftertouch(1, 60, 64)",
            []() -> juce::MidiMessage { return juce::MidiMessage::aftertouchChange (1, 60, 64); }
        },

        LuaMidiTestCase {
            "return midi.allnotesoff(1)",
            []() -> juce::MidiMessage { return juce::MidiMessage::allNotesOff (1); }
        },

        LuaMidiTestCase {
            "return midi.allsoundsoff(1)",
            []() -> juce::MidiMessage { return juce::MidiMessage::allSoundOff (1); }
        },

//        LuaMidiTestCase {
//            "return midi.clock()",
//            []() -> juce::MidiMessage { return juce::MidiMessage::midiClock(); }
//        },

//        LuaMidiTestCase {
//            "return midi.start()",
//            []() -> juce::MidiMessage { return juce::MidiMessage::midiStart(); }
//        },

//        LuaMidiTestCase {
//            "return midi.stop()",
//            []() -> juce::MidiMessage { return juce::MidiMessage::midiStop(); }
//        },

//        LuaMidiTestCase {
//            "return midi.continue()",
//            []() -> juce::MidiMessage { return juce::MidiMessage::midiContinue(); }
//        }
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