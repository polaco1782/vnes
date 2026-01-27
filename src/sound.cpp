#include "sound.h"
#include "apu.h"
#include <cstring>
#include <algorithm>

Sound::Sound()
    : apu(nullptr)
{
    std::memset(samples, 0, sizeof(samples));
    sample_buffer.resize(MAX_BUFFER_SIZE);
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
    // DC-block filter to reduce low-frequency offsets
    float filtered = sample - dc_prev_input + (DC_BLOCK_R * dc_prev_output);
    dc_prev_input = sample;
    dc_prev_output = filtered;

    // Clamp and convert to 16-bit
    if (filtered > 1.0f) filtered = 1.0f;
    if (filtered < -1.0f) filtered = -1.0f;
    
    sf::Int16 int_sample = static_cast<sf::Int16>(filtered * 32767.0f);

    std::lock_guard<std::mutex> lock(buffer_mutex);
    sample_buffer[buffer_write] = int_sample;
    buffer_write = (buffer_write + 1) % MAX_BUFFER_SIZE;
    if (buffer_count < MAX_BUFFER_SIZE) {
        buffer_count++;
    } else {
        // Overwrite oldest sample when buffer is full
        buffer_read = (buffer_read + 1) % MAX_BUFFER_SIZE;
    }
}

bool Sound::onGetData(Chunk& data)
{
    size_t samples_to_copy = 0;

    {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        samples_to_copy = std::min(buffer_count, (size_t)BUFFER_SIZE);
        for (size_t i = 0; i < samples_to_copy; i++) {
            samples[i] = sample_buffer[buffer_read];
            buffer_read = (buffer_read + 1) % MAX_BUFFER_SIZE;
        }
        buffer_count -= samples_to_copy;
        if (samples_to_copy > 0) {
            last_sample = samples[samples_to_copy - 1];
        }
    }

    // Fill remaining with last sample to avoid hard clicks
    if (samples_to_copy < BUFFER_SIZE) {
        std::fill(samples + samples_to_copy, samples + BUFFER_SIZE, last_sample);
    }
    
    data.samples = samples;
    data.sampleCount = BUFFER_SIZE;
    return true;
}

void Sound::onSeek(sf::Time timeOffset)
{
    (void)timeOffset;
}
