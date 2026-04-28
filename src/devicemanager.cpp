// Copyright 2014-2023 Kushview, LLC <info@kushview.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <element/devices.hpp>
#include "engine/jack.hpp"

namespace element {

const int DeviceManager::maxAudioChannels = 128;

class DeviceManager::Private
{
public:
    Private (DeviceManager& o) : owner (o) {}

    DeviceManager& owner;
    AudioEnginePtr engine;

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
