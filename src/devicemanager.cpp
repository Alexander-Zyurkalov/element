// Copyright 2014-2023 Kushview, LLC <info@kushview.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <element/devices.hpp>
#include "engine/jack.hpp"

#if JUCE_MAC
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace element {

const int DeviceManager::maxAudioChannels = 128;

#if JUCE_MAC
namespace {

juce::String cfStringToJuce (CFStringRef cfStr)
{
    if (cfStr == nullptr)
        return {};

    char buffer[512] = {};
    if (CFStringGetCString (cfStr, buffer, sizeof (buffer), kCFStringEncodingUTF8))
        return juce::String::fromUTF8 (buffer);

    return {};
}

juce::String getEndpointName (MIDIObjectRef obj)
{
    CFStringRef name = nullptr;
    if (MIDIObjectGetStringProperty (obj, kMIDIPropertyDisplayName, &name) == noErr && name != nullptr)
    {
        auto result = cfStringToJuce (name);
        CFRelease (name);
        return result;
    }
    return "<unknown>";
}

} // namespace
#endif

class DeviceManager::Private
{
public:
    Private (DeviceManager& o) : owner (o)
    {
#if JUCE_MAC
        OSStatus status = MIDIClientCreate (CFSTR ("ElementMIDIClient"),
                                            &Private::midiNotifyProc,
                                            this,
                                            &midiClient);
        if (status != noErr)
        {
            DBG ("Failed to create CoreMIDI client, status = " << (int) status);
            midiClient = 0;
        }
#else
        knownMidiInputDevices = juce::MidiInput::getAvailableDevices();

        midiListConnection = juce::MidiDeviceListConnection::make ([this] {
            refreshMidiDevices();
        });
#endif
    }

    ~Private()
    {
#if JUCE_MAC
        if (midiClient != 0)
        {
            MIDIClientDispose (midiClient);
            midiClient = 0;
        }
#endif
    }

#if JUCE_MAC
    static void midiNotifyProc (const MIDINotification* message, void* refCon)
    {
        auto* self = static_cast<Private*> (refCon);
        if (self == nullptr)
            return;

        if (message->messageID != kMIDIMsgObjectAdded
            && message->messageID != kMIDIMsgObjectRemoved)
            return;

        auto* addRemove = reinterpret_cast<const MIDIObjectAddRemoveNotification*> (message);

        if (addRemove->childType != kMIDIObjectType_Source)
            return;

        const bool added = (message->messageID == kMIDIMsgObjectAdded);
        const auto name = getEndpointName (addRemove->child);

        juce::MessageManager::callAsync ([added, name] {
            if (added)
                DBG ("MIDI Device needs refresh (connected): " + name);
            else
                DBG ("MIDI Device needs refresh (disconnected): " + name);
        });
    }
#endif

#if ! JUCE_MAC
    void refreshMidiDevices()
    {
        JUCE_ASSERT_MESSAGE_THREAD

        auto currentDevices = juce::MidiInput::getAvailableDevices();

        for (const auto& oldDevice : knownMidiInputDevices)
            if (! currentDevices.contains (oldDevice))
                DBG ("MIDI Device needs refresh (disconnected): " + oldDevice.name);

        for (const auto& newDevice : currentDevices)
            if (! knownMidiInputDevices.contains (newDevice))
                DBG ("MIDI Device needs refresh (connected): " + newDevice.name);

    }
#endif

    DeviceManager& owner;
    AudioEnginePtr engine;

#if JUCE_MAC
    MIDIClientRef midiClient { 0 };
#else
    juce::Array<juce::MidiDeviceInfo> knownMidiInputDevices;
    juce::MidiDeviceListConnection midiListConnection;
#endif

#if ELEMENT_USE_JACK
    JackClient jack { "Element", 2, "main_in_", 2, "main_out_" };
#endif

    juce::ReferenceCountedArray<DeviceManager::LevelMeter> levelsIn, levelsOut;
};

DeviceManager::DeviceManager()
{
    impl = std::make_unique<Private> (*this);
}

DeviceManager::~DeviceManager()
{
    closeAudioDevice();
    attach (nullptr);
}

void DeviceManager::attach (AudioEnginePtr engine)
{
    if (impl->engine == engine)
        return;

    auto old = impl->engine;

    if (old != nullptr)
    {
        removeAudioCallback (&old->getAudioIODeviceCallback());
    }

    if (engine)
    {
        addAudioCallback (&engine->getAudioIODeviceCallback());
    }
    else
    {
        closeAudioDevice();
    }

    impl->engine = engine;
}

static void addIfNotNull (juce::OwnedArray<juce::AudioIODeviceType>& list,
                          juce::AudioIODeviceType* const device)
{
    if (device != nullptr)
        list.add (device);
}

void DeviceManager::createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>& list)
{
#if JUCE_ALSA
    addIfNotNull (list, juce::AudioIODeviceType::createAudioIODeviceType_ALSA());
#endif
#if ELEMENT_USE_JACK
    addIfNotNull (list, Jack::createAudioIODeviceType (impl->jack));
#endif

    addIfNotNull (list, juce::AudioIODeviceType::createAudioIODeviceType_ASIO());
    addIfNotNull (list, juce::AudioIODeviceType::createAudioIODeviceType_WASAPI (juce::WASAPIDeviceMode::exclusive));
    addIfNotNull (list, juce::AudioIODeviceType::createAudioIODeviceType_WASAPI (juce::WASAPIDeviceMode::sharedLowLatency));
    addIfNotNull (list, juce::AudioIODeviceType::createAudioIODeviceType_DirectSound());

    addIfNotNull (list, juce::AudioIODeviceType::createAudioIODeviceType_CoreAudio());

    addIfNotNull (list, juce::AudioIODeviceType::createAudioIODeviceType_iOSAudio());

    addIfNotNull (list, juce::AudioIODeviceType::createAudioIODeviceType_OpenSLES());
    addIfNotNull (list, juce::AudioIODeviceType::createAudioIODeviceType_Android());
}

void DeviceManager::getAudioDrivers (juce::StringArray& drivers)
{
    const juce::OwnedArray<juce::AudioIODeviceType>& types (getAvailableDeviceTypes());
    for (int i = 0; i < types.size(); ++i)
        drivers.add (types.getUnchecked (i)->getTypeName());
}

void DeviceManager::selectAudioDriver (const String& name)
{
    setCurrentAudioDeviceType (name, true);
}

#if KV_JACK_AUDIO
kv::JackClient& DeviceManager::getJackClient()
{
    return impl->jack;
}
#endif

} // namespace element
