#ifndef SOUND_H
#define SOUND_H

#include "types.h"
#include <SFML/Audio.hpp>

class APU;

class Sound : public sf::SoundStream {
public:
    Sound();
    ~Sound();
    
    void connect(APU* apu_device);
    void start();
    void stop();
    
private:
    // SoundStream interface
    virtual bool onGetData(Chunk& data) override;
    virtual void onSeek(sf::Time timeOffset) override;
    
    APU* apu;
    
    static const unsigned int SAMPLE_RATE = 44100;
    static const unsigned int CHANNEL_COUNT = 1; // Mono
    static const unsigned int BUFFER_SIZE = 2048;
    
    sf::Int16 samples[BUFFER_SIZE];
    
    // Sample accumulation for downsampling
    float sample_accumulator;
    int samples_accumulated;
    
    // APU runs at ~1.789773 MHz, we need to downsample to 44.1 kHz
    static constexpr float APU_CLOCK_RATE = 1789773.0f;
    static constexpr float SAMPLE_INTERVAL = APU_CLOCK_RATE / SAMPLE_RATE;
    float clock_counter;
};

#endif // SOUND_H
