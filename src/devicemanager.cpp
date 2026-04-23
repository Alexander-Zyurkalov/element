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
static bool isMIDIAlive (MIDIObjectRef source)
{
    SInt32 dummyId = 0;
    OSStatus status = MIDIObjectGetIntegerProperty (source, kMIDIPropertyUniqueID, &dummyId);
    return status == noErr;
}

static MIDIEndpointRef findSource (const juce::MidiDeviceInfo& deviceInfo)
{
    SInt32 targetUID = deviceInfo.identifier.containsChar (' ')
                           ? deviceInfo.identifier.fromLastOccurrenceOf (" ", false, false).getIntValue()
                           : deviceInfo.identifier.getIntValue();

    ItemCount count = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < count; ++i)
    {
        MIDIEndpointRef endpoint = MIDIGetSource (i);
        SInt32 uid = 0;
        MIDIObjectGetIntegerProperty (endpoint, kMIDIPropertyUniqueID, &uid);
        if (uid == targetUID)
            return endpoint;
    }
    return 0;
}
#endif

class DeviceManager::Private
#if ! JUCE_MAC
    : public juce::ChangeListener
#endif
{
public:
    Private (DeviceManager& o) : owner (o)
    {
        knownMidiInputDevices = juce::MidiInput::getAvailableDevices();

#if JUCE_MAC
        // Register a CoreMIDI client so the OS notifies us of device changes.
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
        // On other platforms, rely on AudioDeviceManager broadcasting changes.
        owner.addChangeListener (this);
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
#else
        owner.removeChangeListener (this);
#endif
    }

#if ! JUCE_MAC
    void changeListenerCallback (juce::ChangeBroadcaster* source) override
    {
        if (source == &owner)
           {
                auto currentDevices = juce::MidiInput::getAvailableDevices();

                for (const auto& oldDevice : knownMidiInputDevices)
                {
                    if (! currentDevices.contains (oldDevice))
                    {
                        DBG ("MIDI Device Disconnected: " + oldDevice.name);
                        refreshMidiDevices();

                        // Handle disconnect logic here
                    }
                }

                for (const auto& newDevice : currentDevices)
                {
                    if (! knownMidiInputDevices.contains (newDevice))
                    {
                        DBG ("MIDI Device Connected: " + newDevice.name);
                        refreshMidiDevices();
                        // Handle connect logic here
                    }
                }

                knownMidiInputDevices = std::move (currentDevices);

    }
#endif

#if JUCE_MAC
    static void midiNotifyProc (const MIDINotification* message, void* refCon)
    {
        // This runs on a CoreMIDI-internal thread. Bounce to the message thread.
        auto* self = static_cast<Private*> (refCon);
        if (self == nullptr)
            return;

        // We only care about add/remove/change of objects. Filter early to avoid
        // posting work for every setup-changed notification storm.
        switch (message->messageID)
        {
            case kMIDIMsgObjectAdded:
            case kMIDIMsgObjectRemoved:
            case kMIDIMsgSetupChanged:
                juce::MessageManager::callAsync ([self] {
                    self->refreshMidiDevices();
                });
                break;
            default:
                break;
        }
    }
#endif

    void refreshMidiDevices()
    {
        JUCE_ASSERT_MESSAGE_THREAD

        DBG("A device is not available");

    }

    DeviceManager& owner;
    AudioEnginePtr engine;
    juce::Array<juce::MidiDeviceInfo> knownMidiInputDevices;

#if JUCE_MAC
    MIDIClientRef midiClient { 0 };
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
