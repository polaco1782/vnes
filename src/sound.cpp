#include "sound.h"
#include "apu.h"
#include <cstring>

Sound::Sound()
    : apu(nullptr)
    , sample_accumulator(0.0f)
    , samples_accumulated(0)
    , clock_counter(0.0f)
{
    std::memset(samples, 0, sizeof(samples));
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

bool Sound::onGetData(Chunk& data)
{
    if (!apu) {
        // Fill with silence
        std::memset(samples, 0, sizeof(samples));
        data.samples = samples;
        data.sampleCount = BUFFER_SIZE;
        return true;
    }
    
    // Generate samples from APU
    for (unsigned int i = 0; i < BUFFER_SIZE; i++) {
        // Get current APU output (simplified - just return a mix of all channels)
        // In a real implementation, you'd call APU methods to get the actual output
        float output = apu->getOutput();
        
        // Convert to 16-bit signed integer
        output = output * 32767.0f; // Scale to int16 range
        if (output > 32767.0f) output = 32767.0f;
        if (output < -32768.0f) output = -32768.0f;
        
        samples[i] = static_cast<sf::Int16>(output);
    }
    
    data.samples = samples;
    data.sampleCount = BUFFER_SIZE;
    return true;
}

void Sound::onSeek(sf::Time timeOffset)
{
    // Not implemented - seeking not needed for real-time audio
    (void)timeOffset;
}
