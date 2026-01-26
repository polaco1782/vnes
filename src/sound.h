#ifndef SOUND_H
#define SOUND_H

#include "types.h"
#include <SFML/Audio.hpp>
#include <vector>

class APU;

class Sound : public sf::SoundStream {
public:
    Sound();
    ~Sound();
    
    void connect(APU* apu_device);
    void start();
    void stop();
    
    // Called from emulation thread to push samples
    void pushSample(float sample);
    
private:
    // SoundStream interface
    virtual bool onGetData(Chunk& data) override;
    virtual void onSeek(sf::Time timeOffset) override;
    
    APU* apu;
    
    static const unsigned int SAMPLE_RATE = 44100;
    static const unsigned int CHANNEL_COUNT = 1; // Mono
    static const unsigned int BUFFER_SIZE = 2048;
    
    sf::Int16 samples[BUFFER_SIZE];
    std::vector<sf::Int16> sample_buffer;

    static const size_t MAX_BUFFER_SIZE = SAMPLE_RATE; // 1 second buffer
};

#endif // SOUND_H
