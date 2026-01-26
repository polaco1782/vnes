#include "sound.h"
#include "apu.h"
#include <cstring>

Sound::Sound()
    : apu(nullptr)
{
    std::memset(samples, 0, sizeof(samples));
    sample_buffer.reserve(MAX_BUFFER_SIZE);
}

Sound::~Sound()
{
    stop();
}

void Sound::connect(APU* apu_device)
{
    apu = apu_device;
}

void Sound::start()
{
    initialize(CHANNEL_COUNT, SAMPLE_RATE);
    play();
}

void Sound::stop()
{
    sf::SoundStream::stop();
}

void Sound::pushSample(float sample)
{
    // Clamp and convert to 16-bit
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;
    
    sf::Int16 int_sample = static_cast<sf::Int16>(sample * 32767.0f);
    
    if (sample_buffer.size() < MAX_BUFFER_SIZE) {
        sample_buffer.push_back(int_sample);
    }
}

bool Sound::onGetData(Chunk& data)
{
    // Copy available samples
    size_t samples_to_copy = std::min(sample_buffer.size(), (size_t)BUFFER_SIZE);
    
    if (samples_to_copy > 0) {
        std::memcpy(samples, sample_buffer.data(), samples_to_copy * sizeof(sf::Int16));
        sample_buffer.erase(sample_buffer.begin(), sample_buffer.begin() + samples_to_copy);
    }
    
    // Fill remaining with silence if needed
    if (samples_to_copy < BUFFER_SIZE) {
        std::memset(samples + samples_to_copy, 0, (BUFFER_SIZE - samples_to_copy) * sizeof(sf::Int16));
    }
    
    data.samples = samples;
    data.sampleCount = BUFFER_SIZE;
    return true;
}

void Sound::onSeek(sf::Time timeOffset)
{
    (void)timeOffset;
}
