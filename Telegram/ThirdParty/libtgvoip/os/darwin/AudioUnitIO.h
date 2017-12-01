//
// libtgvoip is free and unencumbered public domain software.
// For more information, see http://unlicense.org or the UNLICENSE file
// you should have received with this source code distribution.
//

#ifndef LIBTGVOIP_AUDIOUNITIO_H
#define LIBTGVOIP_AUDIOUNITIO_H

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include "../../threading.h"
#include <string>

namespace tgvoip{ namespace audio{
class AudioInputAudioUnit;
class AudioOutputAudioUnit;

class AudioUnitIO{
public:
	AudioUnitIO();
	~AudioUnitIO();
	void Configure(uint32_t sampleRate, uint32_t bitsPerSample, uint32_t channels);
	void AttachInput(AudioInputAudioUnit* i);
	void AttachOutput(AudioOutputAudioUnit* o);
	void DetachInput();
	void DetachOutput();
	void EnableInput(bool enabled);
	void EnableOutput(bool enabled);
	bool IsFailed();
	static AudioUnitIO* Get();
	static void Release();
#if TARGET_OS_OSX
	void SetCurrentDevice(bool input, std::string deviceID);
#endif
	
private:
	static OSStatus BufferCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData);
	void BufferCallback(AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 bus, UInt32 numFrames, AudioBufferList* ioData);
	void StartIfNeeded();
#if TARGET_OS_OSX
	static OSStatus DefaultDeviceChangedCallback(AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses, void *inClientData);
	std::string currentInputDevice;
	std::string currentOutputDevice;
#endif
	AudioComponentInstance unit;
	AudioInputAudioUnit* input;
	AudioOutputAudioUnit* output;
	AudioBufferList inBufferList;
	bool inputEnabled;
	bool outputEnabled;
	bool failed;
	bool started;
	static int refCount;
	static AudioUnitIO* sharedInstance;
};
}}

#endif /* LIBTGVOIP_AUDIOUNITIO_H */
