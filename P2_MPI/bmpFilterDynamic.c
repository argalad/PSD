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
	unsigned char* outputBuffer;					/** Output buffer for filtered pixels */
	unsigned char* inputBuffer;						/** Input buffer to allocate original pixels */
	unsigned char* auxPtr;							/** Auxiliary pointer */
	unsigned int rowSize;							/** Number of pixels per row */
	unsigned int rowsPerProcess;					/** Number of rows to be processed (at most) by each worker */
	unsigned int rowsSentToWorker;					/** Number of rows to be sent to a worker process */
	unsigned int receivedRows;						/** Total number of received rows */
	unsigned int threshold;							/** Threshold */
	unsigned int currentRow;						/** Current row being processed */
	unsigned int readBytes;							/** Number of bytes read from input file */
	unsigned int writeBytes;						/** Number of bytes written to output file */
	unsigned int numPixels;							/** Number of neighbour pixels (including current pixel) */
	unsigned int* processIDs;
	tPixelVector vector;							/** Vector of neighbour pixels */
	int imageDimensions[2];							/** Dimensions of input image */
	double timeStart, timeEnd;						/** Time stamps to calculate the filtering time */
	int size, rank, tag;							/** Number of process, rank and tag */
	MPI_Status status;								/** Status information for received messages */
	////////////////////////////////////////////////////////////////////////////////////////////////////
	int* indexTable;								/** Index table */
	int i,j;										/** Aux variables*/
	int processedRows;								/** Number of currently processed rows */
	int totalRows;									/** Number of total rows*/
	unsigned char* auxPtr2;							/** Auxiliary pointer 2*/
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Init
	MPI_Init (&argc, &argv);
	MPI_Comm_size (MPI_COMM_WORLD, &size);
	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
	tag = 1;
	srand (time(NULL));

	// Check the number of processes
	if (size < 2) 
	{

		if (rank == 0)
			printf("This program must be launched with (at least) 2 processes\n");

		MPI_Finalize ();
		exit (0);
	}

	// Check arguments
	if (argc != 5) 
	{

		if (rank == 0)
			printf("Usage: ./bmpFilterDynamic sourceFile destinationFile threshold numRows\n");

		MPI_Finalize ();
		exit (0);
	}

	// Get input arguments...
	sourceFileName = argv[1];
	destinationFileName = argv[2];
	threshold = atoi(argv[3]);
	rowsPerProcess = atoi(argv[4]);

	// Allocate memory for process IDs vector
	processIDs = (unsigned int*) malloc (size * sizeof (unsigned int));

	// Master process
	if (rank == 0) {

		// Process starts
		timeStart = MPI_Wtime();

		// Read headers from input file
		readHeaders (sourceFileName, &imgFileHeaderInput, &imgInfoHeaderInput);
		readHeaders (sourceFileName, &imgFileHeaderOutput, &imgInfoHeaderOutput);

		// Write header to the output file
		writeHeaders (destinationFileName, &imgFileHeaderOutput, &imgInfoHeaderOutput);

		// Calculate row size for input and output images(in bytes)
		rowSize = (((imgInfoHeaderInput.biBitCount * imgInfoHeaderInput.biWidth) + 31) / 32) * 4;

		// Show info before processing
		if (SHOW_LOG_MESSAGES) 
		{
			printf("[MASTER] Applying filter to image %s (%dx%d) with threshold %d. Generating image %s\n", sourceFileName,
				rowSize, imgInfoHeaderInput.biHeight, threshold, destinationFileName);
			printf("Number of workers:%d -> Each worker calculates (at most) %d rows\n", size - 1, rowsPerProcess);
		}

		// Show headers...
		if (SHOW_BMP_HEADERS) 
		{
			printf("Source BMP headers:\n");
			printBitmapHeaders (&imgFileHeaderInput, &imgInfoHeaderInput);
			printf("Destination BMP headers:\n");
			printBitmapHeaders (&imgFileHeaderOutput, &imgInfoHeaderOutput);
		}

		// Open source image
		if ((inputFile = open (sourceFileName, O_RDONLY)) < 0) 
		{
			printf("ERROR: Source file cannot be opened: %s\n", sourceFileName);
			exit(1);
		}

		// Open target image
		if ((outputFile = open (destinationFileName, O_WRONLY | O_APPEND, 0777)) < 0) 
		{
			printf("ERROR: Target file cannot be open to append data: %s\n", destinationFileName);
			exit(1);
		}

		// Allocate memory to copy the bytes between the header and the image data(info)
		outputBuffer = (unsigned char*)malloc((imgFileHeaderInput.bfOffBits - BIMAP_HEADERS_SIZE) * sizeof(unsigned char));

		// Copy bytes between headers and pixels
		lseek (inputFile, BIMAP_HEADERS_SIZE, SEEK_SET);
		if ((readBytes = read (inputFile, outputBuffer, imgFileHeaderInput.bfOffBits - BIMAP_HEADERS_SIZE)) == -1)
		{
			perror ("Error while reading from file.\n");
			exit (1);
		}
		
		if ((writeBytes = write (outputFile, outputBuffer, imgFileHeaderInput.bfOffBits - BIMAP_HEADERS_SIZE)) == -1)
		{
			perror ("Error while writing to file.\n");
			exit (1);
		}
		free (outputBuffer);

		// Copy image data into buffer
		inputBuffer = (unsigned char*) malloc (rowSize * imgInfoHeaderInput.biHeight * sizeof (unsigned char*));
		lseek (inputFile, imgFileHeaderInput.bfOffBits, SEEK_SET);
		if ((readBytes = read (inputFile, inputBuffer, rowSize * imgInfoHeaderInput.biHeight)) == -1)
		{
			perror ("Error while reading from file.\n");
			exit (1);
		}

		//Distribute work to other processes
		// Init
		auxPtr = inputBuffer;
		currentRow = 0;
		receivedRows = 0;
		processedRows = 0;
		indexTable = (int*) malloc (size * sizeof (int)); // Index table (alloc and init)
		for (i = 0; i < size; i++)
			indexTable[i] = 0;

		totalRows = imgInfoHeaderInput.biHeight;

		// Check whether at least each worker receives NROWS
		if ((rowsPerProcess*(size - 1)) > totalRows) 
		{
			printf ("[MASTER] Wrong configuration for NROWS (%d). SIZE (%d) < numWorkers(%d)*NROWS(%d)\n", rowsPerProcess, totalRows, size - 1, rowsPerProcess);
			printf ("At least, each worker must receive %d rows to be processed\n", rowsPerProcess);
			MPI_Abort (MPI_COMM_WORLD,-1);
			exit (-1);
		}
		
		// Distribute rows...
		for (i = 1; i < size; i++) 
		{
			//Send size of row
			MPI_Send(&rowSize, 1, MPI_INT, i, tag, MPI_COMM_WORLD);

			// Send the number of rows to be processed
			rowsSentToWorker = rowsPerProcess;
			MPI_Send(&rowsSentToWorker, 1, MPI_INT, i, tag, MPI_COMM_WORLD);

			// Send the rows data
			indexTable[i] = currentRow;
			MPI_Send(auxPtr, rowsSentToWorker * rowSize, MPI_UNSIGNED_CHAR, i, tag, MPI_COMM_WORLD);
			if (DEBUG_FILTERING)
				printf("[Master] sending %d rows to worker %d. -----------Current row =%d\n", rowsPerProcess, i, currentRow);

			// Update pointer and index
			currentRow += rowsSentToWorker;
			auxPtr += (rowsSentToWorker * rowSize);
		}
		
		// Allocate memory to copy the image bytes
		outputBuffer = (unsigned char*) malloc (rowSize * imgInfoHeaderInput.biHeight);

		// While there are remaining rows...
		while (processedRows < totalRows)
		{

			// Receive number of rows
			MPI_Recv (&rowsSentToWorker, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);

			// Set destination buffer
			auxPtr2 = outputBuffer + (indexTable[status.MPI_SOURCE]* rowSize);

			// Receive result data
			MPI_Recv (auxPtr2, rowsSentToWorker * rowSize, MPI_UNSIGNED_CHAR, status.MPI_SOURCE, tag, MPI_COMM_WORLD, &status);

			// Update processed rows
			processedRows += rowsSentToWorker;
			
			if (DEBUG_FILTERING)
				printf("[MASTER] Receiving filtered data (%d rows) from worker %d.\n", rowsSentToWorker,status.MPI_SOURCE);

			// Send remaining rows...
			if (currentRow < totalRows)
			{
				// Send the number of rows to be processed
				if ((currentRow+ rowsPerProcess) > totalRows)
					rowsSentToWorker = totalRows - currentRow;
				else
					rowsSentToWorker = rowsPerProcess;
				
				if (DEBUG_FILTERING)
					printf ("[Master] sending %d rows to worker %d. -----------Current row =%d\n", rowsSentToWorker, status.MPI_SOURCE, currentRow);
				//Send size of row
				MPI_Send (&rowSize, 1, MPI_INT, status.MPI_SOURCE, tag, MPI_COMM_WORLD);

				//Send number of rows
				MPI_Send (&rowsSentToWorker, 1, MPI_INT, status.MPI_SOURCE, tag, MPI_COMM_WORLD);

				// Send the rows data
				indexTable[status.MPI_SOURCE] = currentRow;
				MPI_Send (auxPtr, rowsSentToWorker* rowSize, MPI_UNSIGNED_CHAR, status.MPI_SOURCE, tag, MPI_COMM_WORLD);

				// Update pointer and index
				currentRow += rowsSentToWorker;
				
				auxPtr += (rowsSentToWorker * rowSize);
			}
			else
			{
				//Send size of row
				MPI_Send (&rowSize, 1, MPI_INT, status.MPI_SOURCE, tag, MPI_COMM_WORLD);
				rowsSentToWorker = 0;
				MPI_Send (&rowsSentToWorker, 1, MPI_INT, status.MPI_SOURCE, tag, MPI_COMM_WORLD);
			}
		}
		
		//Writing image data to outputFile
		lseek (outputFile, imgFileHeaderInput.bfOffBits, SEEK_SET);
		if ((writeBytes = write (outputFile, outputBuffer, rowSize * imgInfoHeaderInput.biHeight)) == -1)
		{
			perror ("Error while writing to file.\n");
			exit (1);
		}

		// Close files
		close (inputFile);
		close (outputFile);

		// Free buffers
		free (inputBuffer);
		free (outputBuffer);
		free (processIDs);
		free (indexTable);

		// Process ends
		timeEnd = MPI_Wtime();

		// Show processing time
		printf("Filtering time: %f\n", timeEnd - timeStart);
	}


	// Worker process
	else 
	{
		do
		{
			// Receive size of row
			MPI_Recv (&rowSize, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);

			// Receive number of rows
			MPI_Recv (&rowsSentToWorker, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);

			inputBuffer = (unsigned char*)malloc(rowsSentToWorker * rowSize);

			// Data arrives to each process
			if (DEBUG_FILTERING)
				printf("[Process %d] Processing %d rows\n", rank, rowsSentToWorker);

			if (rowsSentToWorker >0)
			{

				// Receive input data
				MPI_Recv (inputBuffer, rowsSentToWorker* rowSize, MPI_UNSIGNED_CHAR, 0, tag, MPI_COMM_WORLD, &status);
				auxPtr = inputBuffer;
				outputBuffer = (unsigned char*)malloc(rowsSentToWorker * rowSize);
				// Filtering data
				for (i = 0; i < rowsSentToWorker; i++)
				{
					for (j = 0; j < rowSize; j++)
					{
						if (j == 0)
						{
							numPixels = 2;
							vector[0] = auxPtr[i * rowSize + j];
							vector[1] = auxPtr[i * rowSize + j + 1];
						}
						else if (j == rowSize - 1)
						{
							numPixels = 2;
							vector[0] = auxPtr[i * rowSize + j];
							vector[1] = auxPtr[i * rowSize + j - 1];
						}
						else
						{
							numPixels = 3;
							vector[0] = auxPtr[i * rowSize + j - 1];
							vector[1] = auxPtr[i * rowSize + j];
							vector[2] = auxPtr[i * rowSize + j + 1];
						}

						outputBuffer[i * rowSize + j] = calculatePixelValue(vector, numPixels, threshold, 0);
					}
				}

				// Send results
				MPI_Send (&rowsSentToWorker, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
				MPI_Send (outputBuffer, rowsSentToWorker* rowSize, MPI_UNSIGNED_CHAR, 0, tag, MPI_COMM_WORLD);
				if (DEBUG_FILTERING)
					printf("[Process %d] Filtered data sent!\n",rank);
			}

		} while (rowsSentToWorker >0);
	}

	// Finish MPI environment
	MPI_Finalize();


}
