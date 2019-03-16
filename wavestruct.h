#define SAMPLERATE 44100 //Hz
#define CHANNELS 2 // 1:monoral, 2:stereo
#define BITSPERSAMPLE 16 // bits per sample

typedef struct {
	char riffID[4];
	unsigned long fileSize;
	char waveID[4];
} RIFFHeader;

typedef struct {
	char chunkID[4];
	long chunkSize; 
	short wFormatTag;
	unsigned short wChannels;
	unsigned long dwSamplesPerSec;
	unsigned long dwAvgBytesPerSec;
	unsigned short wBlockAlign;
	unsigned short wBitsPerSample;
/* Note: there may be additional fields here, depending upon wFormatTag. */ 
} FormatChunk;

typedef struct {
	char chunkID[4];
	long chunkSize; 
} DataChunk;