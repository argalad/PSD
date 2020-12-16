#include "bmpBlackWhite.h"
#include "mpi.h"

/** Show log messages */
#define SHOW_LOG_MESSAGES 1

/** Enable output for filtering information */
#define DEBUG_FILTERING 0

/** Show information of input and output bitmap headers */
#define SHOW_BMP_HEADERS 0


int main(int argc, char** argv)
{

	tBitmapFileHeader imgFileHeaderInput;			/** BMP file header for input image */
	tBitmapInfoHeader imgInfoHeaderInput;			/** BMP info header for input image */
	tBitmapFileHeader imgFileHeaderOutput;			/** BMP file header for output image */
	tBitmapInfoHeader imgInfoHeaderOutput;			/** BMP info header for output image */
	char* sourceFileName;							/** Name of input image file */
	char* destinationFileName;						/** Name of output image file */
	int inputFile, outputFile;						/** File descriptors */
	unsigned char *outputBuffer;					/** Output buffer for filtered pixels */
	unsigned char *inputBuffer;						/** Input buffer to allocate original pixels */
	unsigned char *auxPtr;							/** Auxiliary pointer */
	unsigned int rowSize;							/** Number of pixels per row */
	unsigned int rowsPerProcess;					/** Number of rows to be processed (at most) by each worker */
	unsigned int rowsSentToWorker;					/** Number of rows to be sent to a worker process */
	unsigned int threshold;							/** Threshold */
	unsigned int currentRow;						/** Current row being processed */
	unsigned int currentPixel;						/** Current pixel being processed */
	unsigned int outputPixel;						/** Output pixel */
	unsigned int readBytes;							/** Number of bytes read from input file */
	unsigned int writeBytes;						/** Number of bytes written to output file */
	unsigned int totalBytes;						/** Total number of bytes to send/receive a message */
	unsigned int numPixels;							/** Number of neighbour pixels (including current pixel) */
	unsigned int currentWorker;						/** Current worker process */
	tPixelVector vector;							/** Vector of neighbour pixels */
	int imageDimensions[2];							/** Dimensions of input image */
	double timeStart, timeEnd;						/** Time stamps to calculate the filtering time */
	int size, rank, tag;							/** Number of process, rank and tag */
	MPI_Status status;								/** Status information for received messages */


	// Init
	MPI_Init (&argc, &argv);
	MPI_Comm_size (MPI_COMM_WORLD, &size);
	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
	tag = 1;
	srand (time (NULL));

	// Check the number of processes
	if (size < 3)
	{
		if (rank == 0)
			printf ("This program must be launched with (at least) 3 processes\n");

		MPI_Finalize ();
		exit (0);
	}

	// Check arguments
	if (argc != 4)
	{
		if (rank == 0)
			printf ("Usage: ./bmpFilterStatic sourceFile destinationFile threshold\n");

		MPI_Finalize ();
		exit (0);
	}

	// Get input arguments...
	sourceFileName = argv[1];
	destinationFileName = argv[2];
	threshold = atoi (argv[3]);

	// Master process
	if (rank == 0)
	{
		// Process starts
		timeStart = MPI_Wtime ();

		// Read headers from input file
		readHeaders (sourceFileName, &imgFileHeaderInput, &imgInfoHeaderInput);
		readHeaders (sourceFileName, &imgFileHeaderOutput, &imgInfoHeaderOutput);

		// Write header to the output file
		writeHeaders (destinationFileName, &imgFileHeaderOutput, &imgInfoHeaderOutput);

		// Calculate row size for input and output images
		rowSize = (((imgInfoHeaderInput.biBitCount * imgInfoHeaderInput.biWidth) + 31) / 32 ) * 4;
		
		// Show headers...
		if (SHOW_BMP_HEADERS)
		{
			printf ("Source BMP headers:\n");
			printBitmapHeaders (&imgFileHeaderInput, &imgInfoHeaderInput);
			printf ("Destination BMP headers:\n");
			printBitmapHeaders (&imgFileHeaderOutput, &imgInfoHeaderOutput);
		}

		// Open source image
		if ((inputFile = open (sourceFileName, O_RDONLY)) < 0)
		{
			printf ("ERROR: Source file cannot be opened: %s\n", sourceFileName);
			exit (1);
		}

		// Open target image
		if ((outputFile = open (destinationFileName, O_WRONLY | O_APPEND, 0777)) < 0)
		{
			printf ("ERROR: Target file cannot be open to append data: %s\n", destinationFileName);
			exit (1);
		}

		// Allocate memory to copy the bytes between the header and the image data
		outputBuffer = (unsigned char*) malloc ((imgFileHeaderInput.bfOffBits-BIMAP_HEADERS_SIZE) * sizeof(unsigned char));

		// Copy bytes between headers and pixels
		lseek (inputFile, BIMAP_HEADERS_SIZE, SEEK_SET);
		read (inputFile, outputBuffer, imgFileHeaderInput.bfOffBits-BIMAP_HEADERS_SIZE);
		write (outputFile, outputBuffer, imgFileHeaderInput.bfOffBits-BIMAP_HEADERS_SIZE);
		free (outputBuffer);

		// Calculamos las filas asignadas a cada worker
		rowsSentToWorker = imgInfoHeaderInput.biHeight / (size - 1);

		// Calculamos si las filas no son divisibles por los workers
		if (imgInfoHeaderInput.biHeight % (size - 1) > 0)
			rowsPerProcess = rowsSentToWorker + (imgInfoHeaderInput.biHeight % (size - 1));
		else
			rowsPerProcess = rowsSentToWorker;

		// Copy image data into buffer
		inputBuffer = (unsigned char*) malloc (rowSize * imgInfoHeaderInput.biHeight * sizeof (unsigned char*));
		lseek (inputFile, imgFileHeaderInput.bfOffBits, SEEK_SET);
		read (inputFile, inputBuffer, rowSize * imgInfoHeaderInput.biHeight);

		auxPtr = inputBuffer;
		currentRow = 0;
		
		// Data delivery
		int i;
		for (i = 1; i < size; i++)
		{
			MPI_Send (&rowSize, 1, MPI_INT, i, tag, MPI_COMM_WORLD);
			// Al primer worker se le envían más filas si no fueran divisibles.
			if (rowsPerProcess - rowsSentToWorker > 0)
			{
				if (DEBUG)
					printf ("[Master] sending %d rows to worker %d\n", rowsPerProcess, i);

				MPI_Send (auxPtr, rowSize * rowsPerProcess, MPI_UNSIGNED_CHAR, i, tag, MPI_COMM_WORLD);
				auxPtr += rowSize * rowsPerProcess;
				rowsPerProcess = rowsSentToWorker;
			}
			else
			{
				if (DEBUG)
					printf ("[Master] sending %d rows to worker %d\n", rowsPerProcess, i);

				MPI_Send (&rowsPerProcess, 1, MPI_INT, i, tag, MPI_COMM_WORLD);
				MPI_Send (auxPtr, rowSize * rowsSentToWorker, MPI_UNSIGNED_CHAR, i, tag, MPI_COMM_WORLD);
				auxPtr += rowSize * rowsSentToWorker;
			}
		}

		outputBuffer = (unsigned char*) malloc (rowSize * imgInfoHeaderInput.biHeight);

		// Data reception
		for (i = 1; i < size; i++)
		{	
			if (i == 1)
			{
				MPI_Recv (outputBuffer, rowSize * (rowsSentToWorker + (imgInfoHeaderInput.biHeight % (size - 1))), 
					MPI_UNSIGNED_CHAR, i, tag, MPI_COMM_WORLD, &status);
				outputBuffer += rowSize * (rowsSentToWorker + (imgInfoHeaderInput.biHeight % (size - 1)));
			}
			else
			{
				MPI_Recv (outputBuffer, rowSize * rowsSentToWorker, 
					MPI_UNSIGNED_CHAR, i, tag, MPI_COMM_WORLD, &status);
				outputBuffer += rowSize * rowsSentToWorker;
			}
		}


		free(inputBuffer);
		free(outputBuffer);
		// Close files
		close (inputFile);
		close (outputFile);

		// Process ends
		timeEnd = MPI_Wtime ();

		// Show processing time
		printf ("Filtering time: %f\n",timeEnd-timeStart);
	}


	// Worker process
	else
	{
		// Size of a row
		MPI_Recv (&rowSize, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
		// Number of rows to receive
		MPI_Recv (&rowsSentToWorker, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);

		inputBuffer = (unsigned char*) malloc (rowSize * rowsSentToWorker);

		MPI_Recv (inputBuffer, rowSize * rowsSentToWorker, MPI_CHAR, 0, tag, MPI_COMM_WORLD, &status);
		auxPtr = inputBuffer;
		
		outputBuffer = (unsigned char*) malloc (rowSize * rowsSentToWorker);

		int i, j, numElemPerRow;
		numElemPerRow = rowSize / sizeof (unsigned char);
		for (i = 0; i < rowsSentToWorker; i++)
		{
			for (j = 0; j < numElemPerRow; j++)
			{
				if (j == 0)
				{
					numPixels = 2;
					vector[0] = auxPtr[j];
					vector[1] = auxPtr[j + 1];
				}
				else if (j == numElemPerRow - 1)
				{
					numPixels = 2;
					vector[0] = auxPtr[j];
					vector[1] = auxPtr[j - 1];
				}
				else
				{
					numPixels = 3;
					vector[0] = auxPtr[j - 1];
					vector[1] = auxPtr[j];
					vector[2] = auxPtr[j + 1];
				}

				outputBuffer[j] = calculatePixelValue (vector, numPixels, threshold, debug);
			}
			auxPtr += rowSize;
			outputBuffer += rowSize;
		}

		// Send finish
		MPI_Send ()
		MPI_Send (outputBuffer, rowSize * rowsSentToWorker, MPI_CHAR, i, tag, MPI_COMM_WORLD);
	}

	// Finish MPI environment
	MPI_Finalize ();
}
