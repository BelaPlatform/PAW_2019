#include <Bela.h>
#include <libraries/Scope/Scope.h> // Include the Scope library

// Scope object
Scope scope;

bool setup(BelaContext *context, void *userData)
{
	// Setup scope with 1 channel
	scope.setup(1, context->audioSampleRate);
	return true;
}

void render(BelaContext *context, void *userData)
{
	for (unsigned int n = 0; n < context->audioFrames; ++n) {
		// Read piezo on left audio channel
		float piezoInput = audioRead(context, 0, n);
		// Log into scope
		scope.log(piezoInput);
	}
}

void cleanup(BelaContext *context, void *userData)
{

}