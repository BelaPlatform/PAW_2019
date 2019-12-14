#include <Bela.h>
#include <libraries/Scope/Scope.h>
#include <SampleLoader.h>
#include <SampleData.h>

#define NUM_CHANNELS 1

// Sample file name
std::string gFilename = "sample.wav";

// Sample data
SampleData gSampleData[NUM_CHANNELS];

int gReadPtr;	// Position of last read sample from file

// Scope object
Scope scope;

float gPiezoInput; // Piezo sensor input

// For onset detection
float peakValue = 0;
float thresholdToTrigger = 0.8;
float amountBelowPeak = 0.6;
float rolloffRate = 0.00005;
int triggered = 0;


bool setup(BelaContext *context, void *userData)
{
	// Load samples
	for(int ch=0;ch<NUM_CHANNELS;ch++) {
        	gSampleData[ch].sampleLen = getNumFrames(gFilename);
    		gSampleData[ch].samples = new float[gSampleData[ch].sampleLen];
        	getSamples(gFilename,gSampleData[ch].samples,ch,0,gSampleData[ch].sampleLen);
	}
	
	// Initialise read pointer
	gReadPtr = -1;

	// setup the scope with 4 channels at the audio sample rate
	scope.setup(4, context->audioSampleRate);

	return true;
}

void render(BelaContext *context, void *userData)
{
	float piezoRectified;
	float out = 0;

	for(unsigned int n = 0; n < context->audioFrames; n++) {

		// Read left audio channel (piezo disk)
		gPiezoInput = audioRead(context, n, 0);
		
		piezoRectified = gPiezoInput;

		// Full wave rectification
		if(piezoRectified < 0)
			piezoRectified *= -1.0f;
		
		// Onset Detection
		// Record the highesst peak
		if(piezoRectified >= peakValue) {
			peakValue = piezoRectified;
			triggered = 0;
		// Force peak value to decay over time
		// to be able to detect next peak
		} else if(peakValue >= rolloffRate) {
			peakValue -= rolloffRate;
		}
		
	     	// Threshold peak to trigger	
	  	if(piezoRectified < peakValue - amountBelowPeak && peakValue >= thresholdToTrigger && !triggered) {
			// Indicate that we've triggered
			// Wait for next peak before triggering again
			triggered = 1;
			// Start sampel playback
			gReadPtr = 0;
	  	}

		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
            
			// If triggered...
			if(gReadPtr != -1)
				// ...read each sample...
				out = gSampleData[channel%NUM_CHANNELS].samples[gReadPtr++];	

			// Wrap read pointer when the end of the sample is read
			if(gReadPtr >= gSampleData[channel%NUM_CHANNELS].sampleLen)
				gReadPtr = -1;
		    	
			audioWrite(context, n, channel, 0.2*out);
		}
		// log the piezo input, peakValue from onset detection and audio output on the scope
		scope.log(gPiezoInput, piezoRectified, peakValue, out);
	}
	
}


void cleanup(BelaContext *context, void *userData)
{
    for(int ch=0;ch<NUM_CHANNELS;ch++)
    	delete[] gSampleData[ch].samples;
}


/**
\example sample-piezo-trigger/render.cpp

Piezo strike to WAV file playback
---------------------------------

As a piezo disk behaves like a microphone it outputs both negative and positive values. A 
step we have to take before detecting strikes is to fullwave rectify the signal, this gives us only
positive values.

Next we perform the onset detection. We do this by looking for a downwards trend in the sensor data
after a rise. Once we've identified this we can say that a peak has occured and trigger the sample
to play. We do this by setting `gReadPtr = 0;`.

This type of onset detection is by no means perfect. Really we should lowpass filter the 
piezo signal before performing the onset detection algorithm and implement some kind of 
debounce on the stikes to avoid multiple strikes being detected for a single strike.
**/
