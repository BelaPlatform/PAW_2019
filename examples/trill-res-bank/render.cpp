#include <Bela.h>

#include "Resonators.h"
#include "Model.h"
#include <libraries/Trill/Trill.h>
#include <libraries/OnePole/OnePole.h>

#define NUM_TOUCH 5 // Number of touches on Trill sensor

// Trill object declaration
Trill touchSensor;

// Location of touches on Trill Bar
float gTouchLocation[NUM_TOUCH] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
// Size of touches on Trill bar
float gTouchSize[NUM_TOUCH] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
// Number of active touches
int gNumActiveTouches = 0;
// Touch size range on which the re-mapping will be done
int gTouchSizeRange[2] = { 1000, 7000 };


// Sleep time for auxiliary task (in ms)
int gTaskSleepTime = 100;

// Resonator bank object declaration
ResonatorBank resBank;
ResonatorBankOptions resBankOptions = {}; // will initialise to default
ModelLoader model; // ModelLoader is deliberately decoupled from Resonators

// Frequency range for resonator 
float gFreqRange[2] = {400.0, 1000.0};
// Decay range for resonator
float gDecayRange[2] = {0.5, 1.5}; 
// Base frequency for the resonator
float gResFreq = gFreqRange[0];
// Base decay for the resonator
float gResDecay = gDecayRange[0];
// Number of millisecons after which resonator will be updated
float gResonatorUpdateRate = 0.01;


// One Pole filter object declaration
OnePole lowpass;
// Frequency of one pole filter
float gLpCutOff = 5;

/*
* Function to be run on an auxiliary task that reads data from the Trill sensor.
* Here, a loop is defined so that the task runs recurrently for as long as the
* audio thread is running.
*/
void I2Cloop(void*)
{
	// loop
	while(!gShouldStop)
	{
		// Read locations from Trill sensor (from I2C bus)
		touchSensor.readLocations();
		// Remap location and size so that they are expressed in a 0-1 range
		for(int i = 0; i <  touchSensor.numberOfTouches(); i++) {
			// Location:
			gTouchLocation[i] = map(touchSensor.touchLocation(i), 0, 3200, 0, 1);
			gTouchLocation[i] = constrain(gTouchLocation[i], 0, 1);
			// Size:
			gTouchSize[i] = map(touchSensor.touchSize(i), gTouchSizeRange[0], gTouchSizeRange[1], 0, 1);
			gTouchSize[i] = constrain(gTouchSize[i], 0, 1);
		}
		// Get number of active touches	
		gNumActiveTouches = touchSensor.numberOfTouches();
		// For all innactive touches, set location and size to 0
		for(int i = gNumActiveTouches; i <  NUM_TOUCH; i++) {
			gTouchLocation[i] = 0.0;
			gTouchSize[i] = 0.0;
		}
		// Sleep for ... milliseconds
		usleep(gTaskSleepTime);
	}
}

bool setup (BelaContext *context, void *userData) {
	
	// Resonator setup:
	// Load model from file
	model.load("models/marimba.json");
	// Calculate model size (number of resonators)
	resBankOptions.total = model.getSize();
	// Setup resonator bank
	resBank.setup(resBankOptions, context->audioSampleRate, context->audioFrames);
	// Pass the model parameters to resonator bank (tuned to frequency)
	resBank.setBank(model.getShiftedToFreq(gResFreq));
	// Update resonator bank
	resBank.update();


	// Touch sensor setup: I2C bus, address, mode, threshold, prescaler 
	if(touchSensor.setup(1, 0x18, Trill::NORMAL, 50, 1) != 0) {
		fprintf(stderr, "Unable to initialise touch sensor\n");
		return false;
	}

	// Set and schedule auxiliary task for reading sensor data from the I2C bus
	AuxiliaryTask readI2CTask = Bela_createAuxiliaryTask(I2Cloop, 50, "I2C-read", NULL);
	Bela_scheduleAuxiliaryTask(readI2CTask);

	// Setup low pass filter for smoothing frequency
	lowpass.setup(gLpCutOff, context->audioSampleRate); 

	return true;
}

void render (BelaContext *context, void *userData) { 

	// Set sample counter
	static unsigned int count = 0;

	for (unsigned int n = 0; n < context->audioFrames; ++n) {
		// Read left audio input (piezo)
		float in = audioRead(context, n, 0); 

		// If there are any active touches
		if(gNumActiveTouches > 0) {
			// Map touch location to frequency
			gResFreq =  map(gTouchLocation[0], 0, 1, gFreqRange[0], gFreqRange[1]);
			// Map touch size to decay
			gResDecay =  map(gTouchSize[0], 0, 1, gDecayRange[0], gDecayRange[1]);
		}
		// Filter frequency changes to minimise artifacts
		float freq = lowpass.process(gResFreq);

		// If enough samples have elapsed ...
		if(count > gResonatorUpdateRate * context->audioSampleRate) {
			// Shift resonator bank in frequency
			model.shiftToFreq(freq);
			// Scale decay
			resBank.setBank(model.getScaledDecay(gResDecay));
			// Update resonator bank
			resBank.update();

			// Reset count
			count = 0;
		}
		// Increment counter
		count++;

		// Render resonator bank output based on piezo signal
		float  out = resBank.render(in);
		// Write to output channels
		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++)
			audioWrite(context, n, ch, out);
	}

}

void cleanup (BelaContext *context, void *userData) { }
