//includes
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <portaudio.h>
#include <string.h>
//asio inclusion
#if PA_USE_ASIO
#include <pa_asio.h>
#endif
//macros & defines
#define CAL_DATE_LEN 11
#define CAL_SERIAL_NUM_LEN 8
#define NUM_CHANNELS 2
#define SENS_LEN 5
#define CALSTRING_LEN 52
#define TOTALDEVICEINFO_LEN 30
#define MAX_BLOCK_SIZE 16384
#define BYTES_PER_SAMPLE 3
#define BITS_PER_SAMPLE 24
#define SENSA 36679
#define SENSB 71972
//structure for wav header
typedef struct {
	char riffChunk[4];
	int fileSize;
	char waveChunk[4];
} tWavHdr;
//structure for wav format
typedef struct {
	char Chunk[4];
	int ChunkLen;
	short Type;
	short ChannelCount;
	int SampleRate;
	int DataPerSecond; 		//(Sample Rate * BitsPerSample * Channels)/8
	short BytesPerFrame; 	//(BitsPerSample * Channels)/8
	short BitsPerSample;
} tWavFmt;
//structure for calibration string
typedef struct {
	char calString[CALSTRING_LEN];
} tCalChunk;
//structure for wav data chunk
typedef struct {
	char chunkID[4];
	int size;
} tWavDataHdr;
//structure for the entire composition
typedef struct {
	tWavHdr header;
	tWavFmt fmt;
	tCalChunk cal;
	tWavDataHdr wavData;	//wav data
	char data[MAX_BLOCK_SIZE*NUM_CHANNELS*BYTES_PER_SAMPLE];	//acquired data
} tWav;

tWav gWave;	
//function prototypes
int GetSensor(int *pNumDevices, int *pCount_333D01, int *pFirst_333D01, char* deviceName); 
int initializeCalString(char *deviceName, char *calString);
int initializeWavHDR(int blockSize, int numChannels, int sampleRate, char *calString, tWav *p);
static int recordCallback(const void *inputBuffer, const void *outputBuffer, int blockSize, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData);

int main(int argc, char *argv[]) {
	//initialize variables
	int numDevices = 0;
	int defaultDisplayed;
	int count_333D01 = 0;
	int first_333D01;
	int i = 0;
	int j = 0;
	int k = 0;
	int error = 0;
	char deviceName[TOTALDEVICEINFO_LEN];
	char calString[CALSTRING_LEN];
	char fileName[1000];
	char tempFileName[1000];
	char userFileName[1000], *p;
	int sampleRateArray[7] = {8000, 11025, 16000, 22050, 32000, 44100, 48000};
	int blockSizeArray[9] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
	int blockSize;
	int numChannels = NUM_CHANNELS;
	int sampleRate;
	int temp;
	int numFiles;
	int secDelay;
	int period;
	int numAcquisitions = 0;

	PaStream* stream;
	PaStreamParameters inputParameters;
	PaError err = paNoError;
	//make sure all strings are empty
	memset(tempFileName, 0x00, 1000);
	memset(fileName, 0x00, 1000);
	memset(userFileName, 0x00, 1000);
	//initialize variables in case there aren't inputs
	strcpy(userFileName, "test");
	sampleRate = 8000;
	blockSize = MAX_BLOCK_SIZE;
	numFiles = 1;
	period = 0;
	numAcquisitions = 0;
	//loop for input parameters
	for (i = 1; i < argc+1; i = i+2) {
		//make sure the iteration isn't greater than the number of values
		if (i >= argc) {

			break;
		} 

		//make sure there is a dash
		if (argv[i][0] != '-') {

			printf("You didn't include the \"-\" mark.\nMake sure you included all parameter specifiers, even if blank.\n\nThe following are all of the specifiers needed:\n-f, -b, -s, -p, -n, -r\n"); 
			return 0;
		}
		//loop through for initializations 
		if ( i == argc -1 || argv[i+1][0] == '-') {

			if (i < argc) {

				i = i -1;
			} 
			//switch for defaults
			switch(argv[i+1][1]) {
				
				case 'f' :
					memcpy(userFileName, "test", 5);
					break;
				case 'b' :
					blockSize = MAX_BLOCK_SIZE;
					break;
				case 's' :
					sampleRate = 8000;
					break;				
				case 'p' :
					period = 0;
					break;			
				case 'n' :
					numFiles = 1;
					break;			
				case 'r' :
					numAcquisitions = 0;
					break;			
				/*case 'A' :
					break;			
				case 'B' :
					break;				
				case 'S' :
					break;				
				case 'd' :
					break;				
				case 'm' :
					break;				*/
				case '?' :
					printf("The following are specifiers for parameters:\n-f File Name\n-b Block Size\n-s Sample Rate\n-p Period/Delay\n-n Number Of Files\n-r Number of Acquisitions\n");								
					return 0;
				default :
					printf("Incorrect inputs. Please check your arguments.\n");
					return 0;
			}
		} else {
			//switch for inputs 
			switch(argv[i][1]) {

				case 'f' :
					memcpy(&userFileName, argv[i+1], strlen(argv[i+1]));
					memcpy(&userFileName[strlen(argv[i+1])], "\0\0\0\0", 4);
					break;
				case 'b' :
					//make sure numbers aren't absurd
					while (blockSize <= 0) {

						printf("Please input a non-zero & non-negative block size: ");
						scanf("%s", &blockSize);
					}
				
					blockSize = atoi(argv[i+1]);

					k = 0;

					temp = blockSize;

					while (k < (sizeof(blockSizeArray)/sizeof(blockSizeArray[0])) && blockSize > blockSizeArray[k]){
		
						k++;
					}

					if(k > sizeof(blockSizeArray)/sizeof(blockSizeArray[0])-1) {

						blockSize = blockSizeArray[k-1];
					} else {

						blockSize = blockSizeArray[k];
					}
	
					if (blockSize != temp) {
						printf("Your block size was changed from %d to %d because that is the closest matching conventional value.\n", temp, blockSize);
						}	
					break;
				case 's' :
					//make sure numbers aren't absurd
					while (sampleRate <= 0) {

						printf("Please input a non-zero & non-negative sample rate: ");
						scanf("%s", &sampleRate);
					}

					sampleRate = atoi(argv[i+1]);

					k = 0;

					temp = sampleRate;

					while (k <= (sizeof(sampleRateArray)/sizeof(sampleRateArray[0])) && sampleRate > sampleRateArray[k]){

						k++;
					}

					if(k > sizeof(sampleRateArray)/sizeof(sampleRateArray[0])-1) {

						sampleRate = sampleRateArray[k-1];
					} else {

						sampleRate = sampleRateArray[k];
					}
					k = 0;
	
					if (sampleRate != temp) {
						printf("Your sample rate was changed from %d to %d because that is the closest matching conventional value.\n", temp, sampleRate);
					}
					break;
				case 'p' :
					period = atoi(argv[i+1]);
					break;
				case 'n' :
					numFiles = atoi(argv[i+1]);
					break;
				case 'r' :
					numAcquisitions = atoi(argv[i+1]);
					break;
				/*case 'A' :
					memcpy(sensChannelA, argv[i+1], strlen(argv[i+1]));
					break;
				case 'B' :
					memcpy(sensChannelB, argv[i+1], strlen(argv[i+1]));
					break;
				case 'S' :
					memcpy(serialNumber, argv[i+1], strlen(argv[i+1]));
					break;
				case 'd' :
					memcpy(calDate, argv[i+1], strlen(argv[i+1]));
					break;
				case 'm' :
					memcpy(modelNumber, argv[i+1], strlen(argv[i+1]));
					break; */
				case '?' :
					//used to find out what each modifier means
					printf("The following are specifiers for parameters:\n-f File Name\n-b Block Size\n-s Sample Rate\n-p Period/Delay\n-n Number Of Files\n-r Number of Acquisitions\n");
					return 0;
				default :
					printf("Incorrect inputs. Please check your arguments.\n");
					return 0;
			}		
		}
	}	

	printf("\n\n\nYour file name is: %s\n", userFileName);		//Print out parameters
	printf("Your final sample rate is: %d\n", sampleRate);	
	printf("Your final block size is: %d\n", blockSize);
	printf("Your period is: %d\n", period);
	printf("Your file count is: %d\n", numFiles);
	printf("Your total number of acquisitions is: %d\n", numAcquisitions);

	deviceName[TOTALDEVICEINFO_LEN] = '\0';		//reinitialize variables
	err = Pa_Initialize();
	if(err != paNoError) {		//error check

		printf("ERROR: Pa_initialize returned %x\n", err);
	}

	error = GetSensor(&numDevices, &count_333D01, &first_333D01, deviceName);
	if(error == -1){
		printf("You gots a problem!\n");
		return -1;
	}

	error = initializeCalString(deviceName, calString);	//make calibration string

	initializeWavHDR(blockSize, numChannels, sampleRate, calString, &gWave);	//initialize wav header

	memset(&inputParameters, 0, sizeof(inputParameters));	//structure input/output sensor specifics
	inputParameters.device = first_333D01;
	inputParameters.channelCount = numChannels;
	inputParameters.sampleFormat = paInt24;
	inputParameters.suggestedLatency = Pa_GetDeviceInfo(first_333D01)->defaultLowInputLatency;
	inputParameters.hostApiSpecificStreamInfo = NULL;

	strcpy(tempFileName, userFileName);		//create temporary copy of file name

	j = 1;	//set to values to be used/iterated in the future
	i = 0;

	while(i < numAcquisitions || numAcquisitions == 0) {	//make loop to either go forever or specified amount

	error = Pa_OpenStream(&stream, &inputParameters, NULL, sampleRate, blockSize, paClipOff, recordCallback, &gWave);		//open the stream and start recording

		if( err != paNoError) goto done;		//error checking	

		err = Pa_StartStream(stream);

		if( err != paNoError) goto done;

		while( (err = Pa_IsStreamActive(stream)) ==1) {

			Pa_Sleep(500);
			fflush(stdout);
		}
	
		if( err < 0) goto done;
	
		err = Pa_CloseStream(stream);

		if( err != paNoError) goto done;

		if (j > numFiles) {

			j = 1;
		}	

		sprintf(userFileName, "%s%03d", tempFileName, j);	//add file name and iterate through amount of file names specified		
		
		j++;	//iterated for number of file names
		
		strcat(userFileName, ".wav");		//add .wav extension	

		//write to file
		FILE *fp;
		fp = fopen(userFileName, "wb");
		fwrite(&gWave, gWave.header.fileSize+20, 1, fp);
		fclose(fp);

		printf("File name %s has been created\n", userFileName);
		//reset file name and replace with temp
		*userFileName = '\0';

		strcpy(userFileName, tempFileName);
		//an input of 0 should be a massive number to make the program go forever
		if (numAcquisitions == 0) {
		
			numAcquisitions = 10000;
			
		} else if (i < numAcquisitions -1) {
 
			if (numAcquisitions == 10000) {

				numAcquisitions == 0;
			}

			sleep(period);	//have program sleep
		}
		
		i++;	//iterate for loop
	}

	done:
		Pa_Terminate();		//close program
	printf("\n\n\n");
	return 0;
}

int GetSensor(int *pNumDevices, int *pCount_333D01, int *pFirst_333D01, char* deviceName) {

	int device = 0;	
	*pFirst_333D01 = -1;
 	const PaDeviceInfo *deviceInfo;
	//find number of devices attached
	*pNumDevices = Pa_GetDeviceCount();
	//loop through devices
	for(device = 0; device < *pNumDevices; device++){

		deviceInfo = Pa_GetDeviceInfo(device);

		if(deviceInfo != NULL){

 			if(strstr(deviceInfo->name, "333D01 1")!=NULL) {
				//find all the 333D01s
				(*pCount_333D01)++;

				if(*pFirst_333D01 == -1){
					//keep the first 333D01
					*pFirst_333D01 = device;
				}
			}
		} else {
			return -1;
		}
	}
	printf("The count for 333D01 is: %d\n\n\n", *pCount_333D01);
	//333D01 errors
	if(*pCount_333D01 == 0){

		printf("Sensor not found error\n", "333D01 sensor not found. \nPlease attach a sensor.");
	} else if(*pCount_333D01 > 1){

		printf("Multiple Sensors Warning\n", "This application will only test the first 333D01 sensor found in the audio device list.\n");
	} else {
		deviceInfo = Pa_GetDeviceInfo(*pFirst_333D01);
		strncpy(deviceName, deviceInfo->name, TOTALDEVICEINFO_LEN);
	}
	return 0;
}

int initializeCalString(char *deviceName, char *calString){
	//beginning of cal string
	char calHDR[] = {'C', 'A', 'L', '1', 0x2c, 0, 0, 0};

	memcpy(calString, calHDR, sizeof(calHDR));	//adds beginning of cal string
	memcpy(&calString[8], &deviceName[0], 6);	//model number
	calString[14] = ' ';				//space
	calString[15] = ' ';				//space
	memcpy(&calString[16], &deviceName[8], 6);	//serial number
	calString[22] = ' ';				//space
	calString[23] = ' ';				//space
	calString[24] = ' ';				//space
	calString[25] = ' ';				//space
	memcpy(&calString[26], &deviceName[7], 23);	//the rest of the sensor name
	calString[49] = ' ';				//space
	calString[50] = ' ';				//space
	calString[51] = '\0';				//null terminator
	
	return 0;
}

int initializeWavHDR(int blockSize, int numChannels, int sampleRate, char *calString, tWav *p){

	p->header.riffChunk[0] = 'R';
	p->header.riffChunk[1] = 'I';
	p->header.riffChunk[2] = 'F';
	p->header.riffChunk[3] = 'F';
	p->header.fileSize = blockSize*numChannels*BYTES_PER_SAMPLE+sizeof(tWavHdr)+sizeof(tCalChunk)+sizeof(tWavDataHdr)+sizeof(tWavFmt)-8;	//because this should only reference chunkLen without the name of chunk and size
	p->header.waveChunk[0] = 'W';
	p->header.waveChunk[1] = 'A';
	p->header.waveChunk[2] = 'V';
	p->header.waveChunk[3] = 'E';
	p->fmt.Chunk[0] = 'f';
	p->fmt.Chunk[1] = 'm';
	p->fmt.Chunk[2] = 't';
	p->fmt.Chunk[3] = ' ';
	p->fmt.ChunkLen = sizeof(tWavFmt)-8;	//minus 8 because it does not include name of chunk and size
	p->fmt.Type = 1;		//PCM format
	p->fmt.ChannelCount = numChannels;
	p->fmt.SampleRate = sampleRate;
	p->fmt.DataPerSecond = (sampleRate*BITS_PER_SAMPLE*numChannels)/8;
	p->fmt.BytesPerFrame = (BITS_PER_SAMPLE*numChannels)/8;
	p->fmt.BitsPerSample = BITS_PER_SAMPLE;
	
	memcpy(&p->cal, calString, sizeof(tCalChunk));

	p->wavData.chunkID[0] = 'd';	//data chunk
	p->wavData.chunkID[1] = 'a';
	p->wavData.chunkID[2] = 't';
	p->wavData.chunkID[3] = 'a';
	p->wavData.size = blockSize*numChannels*BYTES_PER_SAMPLE;
	return 0;
}	

static int recordCallback(const void *inputBuffer, const void *outputBuffer, int blockSize, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {

	tWav *pWav = (tWav*)userData;	//make pointer for tWav 
	memcpy(pWav->data, inputBuffer, pWav->wavData.size);	//store all recorded data in our defined structure

	return paComplete;		
}

































