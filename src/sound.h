#ifndef SOUND_H
#define SOUND_H

#include "types.h"
#include <SFML/Audio.hpp>
#include <vector>
#include <mutex>

class Sound : public sf::SoundStream {
public:
    Sound();
    ~Sound();
    
    void start();
    void stop();
    
    // Called from emulation thread to push samples
    void pushSample(float sample);

    // has been initialized with a valid sound device
    bool initialized;
    
private:
    // SoundStream interface
    virtual bool onGetData(Chunk& data) override;
    virtual void onSeek(sf::Time timeOffset) override;

    static const unsigned int SAMPLE_RATE = 44100;
    static const unsigned int CHANNEL_COUNT = 1; // Mono
    static const unsigned int BUFFER_SIZE = 2048;

    s16 samples[BUFFER_SIZE];

    std::vector<s16> sample_buffer;
    size_t buffer_read = 0;
    size_t buffer_write = 0;
    size_t buffer_count = 0;
    s16 last_sample = 0;
    std::mutex buffer_mutex;

    // DC-block filter state
    float dc_prev_input = 0.0f;
    float dc_prev_output = 0.0f;
    static constexpr float DC_BLOCK_R = 0.995f;

    static const size_t MAX_BUFFER_SIZE = SAMPLE_RATE; // 1 second buffer
};

#endif // SOUND_H
