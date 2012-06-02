#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <omp.h>
#include <iostream>
#include <fstream>

#include "cl.h"


void	PrintCLError( cl_int, char * = "", FILE * = stderr );

int			NUM_ELEMENTS = 8192*1024;
int			LOCAL_SIZE   = 32;
const char *		CL_FILE_NAME = { "first.cl" };
const float			TOL = 0.50f;

int
	main( int argc, char *argv[ ] )
{

	std::ofstream myfile;
	myfile.open ("results.out");

	for (NUM_ELEMENTS = 256*1024; NUM_ELEMENTS < 256*1024*12; NUM_ELEMENTS += 1024*256) {
		for (LOCAL_SIZE = 1; LOCAL_SIZE <= 1024; LOCAL_SIZE = LOCAL_SIZE*2) {
			// see if we can even open the opencl kernel program
			// (no point going on if we can't):

			FILE *fp = fopen( CL_FILE_NAME, "r" );
			if( fp == NULL )
			{
				fprintf( stderr, "Cannot open OpenCL source file '%s'\n", CL_FILE_NAME );
				return 1;
			}

			// 2. allocate the host memory buffers:

			float *hA = new float[ NUM_ELEMENTS ];
			float *hB = new float[ NUM_ELEMENTS ];
			float *hC = new float[ NUM_ELEMENTS ];

			// fill the host memory buffers:

			for( int i = 0; i < NUM_ELEMENTS; i++ )
			{
				hA[i] = hB[i] = sqrt(  (float)i  );
			}

			size_t dataSize = NUM_ELEMENTS * sizeof(float);

			cl_int status;		// returned status from opencl calls
			// test against CL_SUCCESS

			// get the platform id:

			cl_platform_id platform;
			status = clGetPlatformIDs( 1, &platform, NULL );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clGetPlatformIDs failed (2)\n" );

			// get the device id:

			cl_device_id device;
			status = clGetDeviceIDs( platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clGetDeviceIDs failed (2)\n" );

			// 3. create an opencl context:

			cl_context context = clCreateContext( NULL, 1, &device, NULL, NULL, &status );
			if( status != CL_SUCCESS )
				PrintCLError(status, "clEnqueueWriteBuffer: ");
			//fprintf( stderr, "clCreateContext failed\n" );

			// 4. create an opencl command queue:

			cl_command_queue cmdQueue = clCreateCommandQueue( context, device, 0, &status );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clCreateCommandQueue failed\n" );

			// 5. allocate the device memory buffers:

			cl_mem dA = clCreateBuffer( context, CL_MEM_READ_ONLY,  dataSize, NULL, &status );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clCreateBuffer failed (1)\n" );

			cl_mem dB = clCreateBuffer( context, CL_MEM_READ_ONLY,  dataSize, NULL, &status );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clCreateBuffer failed (2)\n" );

			cl_mem dC = clCreateBuffer( context, CL_MEM_WRITE_ONLY, dataSize, NULL, &status );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clCreateBuffer failed (3)\n" );

			// 6. enqueue the 2 commands to write the data from the host buffers to the device buffers:

			status = clEnqueueWriteBuffer( cmdQueue, dA, CL_FALSE, 0, dataSize, hA, 0, NULL, NULL );
			if( status != CL_SUCCESS )
				PrintCLError(status, "clEnqueueWriteBuffer: ");
			//fprintf( stderr, "clEnqueueWriteBuffer failed (1)\n" );

			status = clEnqueueWriteBuffer( cmdQueue, dB, CL_FALSE, 0, dataSize, hB, 0, NULL, NULL );
			if( status != CL_SUCCESS )
				PrintCLError(status, "clEnqueueWriteBuffer: ");
			//fprintf( stderr, "clEnqueueWriteBuffer failed (2)\n" );

			// 7. read the kernel code from a file:

			fseek( fp, 0, SEEK_END );
			size_t fileSize = ftell( fp );
			fseek( fp, 0, SEEK_SET );
			char *clProgramText = new char[ fileSize+1 ];		// leave room for '\0'
			size_t n = fread( clProgramText, 1, fileSize, fp );
			clProgramText[fileSize] = '\0';
			fclose( fp );
			if( n != fileSize )
				fprintf( stderr, "Expected to read %d bytes read from '%s' -- actually read %d.\n", fileSize, CL_FILE_NAME, n );

			// create the text for the kernel program:

			char *strings[1];
			strings[0] = clProgramText;
			cl_program program = clCreateProgramWithSource( context, 1, (const char **)strings, NULL, &status );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clCreateProgramWithSource failed\n" );
			delete [ ] clProgramText;

			// 8. compile and link the kernel code:

			char *options = { "" };
			status = clBuildProgram( program, 1, &device, options, NULL, NULL );
			if( status != CL_SUCCESS )
			{
				size_t size;
				clGetProgramBuildInfo( program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &size );
				cl_char *log = new cl_char[ size ];
				clGetProgramBuildInfo( program, device, CL_PROGRAM_BUILD_LOG, size, log, NULL );
				fprintf( stderr, "clBuildProgram failed:\n%s\n", log );
				delete [ ] log;
			}

			// 9. create the kernel object:

			cl_kernel kernel = clCreateKernel( program, "ArrayMult", &status );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clCreateKernel failed\n" );

			// 10. setup the arguments to the kernel object:

			status = clSetKernelArg( kernel, 0, sizeof(cl_mem), &dA );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clSetKernelArg failed (1)\n" );

			status = clSetKernelArg( kernel, 1, sizeof(cl_mem), &dB );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clSetKernelArg failed (2)\n" );

			status = clSetKernelArg( kernel, 2, sizeof(cl_mem), &dC );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clSetKernelArg failed (3)\n" );


			// 11. enqueue the kernel object for execution:

			size_t globalWorkSize[3] = { NUM_ELEMENTS, 1, 1 };
			size_t localWorkSize[3]  = { LOCAL_SIZE,   1, 1 };

			status = clEnqueueBarrier( cmdQueue );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clEnqueueBarrier (1) failed: %d\n", status );

			double time0 = omp_get_wtime( );

			status = clEnqueueNDRangeKernel( cmdQueue, kernel, 1, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL );
			if( status != CL_SUCCESS )
				PrintCLError(status, "clEnqueueNDRangeKernel: ");
			//fprintf( stderr, "clEnqueueNDRangeKernel failed: %d\n", status );

			status = clEnqueueBarrier( cmdQueue );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clEnqueueBarrier (2) failed: %d\n", status );

			double time1 = omp_get_wtime( );

			// 12. read the results buffer back from the device to the host:

			status = clEnqueueReadBuffer( cmdQueue, dC, CL_TRUE, 0, dataSize, hC, 0, NULL, NULL );
			if( status != CL_SUCCESS )
				fprintf( stderr, "clEnqueueReadBuffer failed\n" );

			// did it work?

			for( int i = 0; i < NUM_ELEMENTS; i++ )
			{
				if( fabs( hC[i] - (float)i ) > TOL )
				{
					fprintf( stderr, "%4d: %12.4f * %12.4f wrongly produced %12.4f (%12.4f)\n", i, hA[i], hB[i], hC[i], (float)i );
				}
			}

			//fprintf( stderr, "Elapsed time = %g seconds = %8.3f GigaMultsPerSecond\n", time1 - time0, (float)NUM_ELEMENTS/(time1-time0)/1000000000. );
			fprintf( stderr, "%d\t%d\t%8.3f\n", NUM_ELEMENTS, LOCAL_SIZE, (float)NUM_ELEMENTS/(time1-time0)/1000000000. );

			myfile <<  NUM_ELEMENTS << "\t" << LOCAL_SIZE << "\t" << (float)NUM_ELEMENTS/(time1-time0)/1000000000. << std::endl;
			//myfile << "green ";
			// 13. clean everything up:

			clReleaseKernel(        kernel   );
			clReleaseProgram(       program  );
			clReleaseCommandQueue(  cmdQueue );
			clReleaseContext(		context );
			clReleaseMemObject(     dA  );
			clReleaseMemObject(     dB  );
			clReleaseMemObject(     dC  );

			delete [ ] hA;
			delete [ ] hB;
			delete [ ] hC;
		} // end local
	} // end global

	myfile.close();
	return 0;
}



struct errorcode
{
	cl_int		statusCode;
	char *		meaning;
}
ErrorCodes[ ] =
{
	{ CL_SUCCESS,				""					},
	{ CL_DEVICE_NOT_FOUND,			"Device Not Found"			},
	{ CL_DEVICE_NOT_AVAILABLE,		"Device Not Available"			},
	{ CL_COMPILER_NOT_AVAILABLE,		"Compiler Not Available"		},
	{ CL_MEM_OBJECT_ALLOCATION_FAILURE,	"Memory Object Allocation Failure"	},
	{ CL_OUT_OF_RESOURCES,			"Out of resources"			},
	{ CL_OUT_OF_HOST_MEMORY,		"Out of Host Memory"			},
	{ CL_PROFILING_INFO_NOT_AVAILABLE,	"Profiling Information Not Available"	},
	{ CL_MEM_COPY_OVERLAP,			"Memory Copy Overlap"			},
	{ CL_IMAGE_FORMAT_MISMATCH,		"Image Format Mismatch"			},
	{ CL_IMAGE_FORMAT_NOT_SUPPORTED,	"Image Format Not Supported"		},
	{ CL_BUILD_PROGRAM_FAILURE,		"Build Program Failure"			},
	{ CL_MAP_FAILURE,			"Map Failure"				},
	{ CL_INVALID_VALUE,			"Invalid Value"				},
	{ CL_INVALID_DEVICE_TYPE,		"Invalid Device Type"			},
	{ CL_INVALID_PLATFORM,			"Invalid Platform"			},
	{ CL_INVALID_DEVICE,			"Invalid Device"			},
	{ CL_INVALID_CONTEXT,			"Invalid Context"			},
	{ CL_INVALID_QUEUE_PROPERTIES,		"Invalid Queue Properties"		},
	{ CL_INVALID_COMMAND_QUEUE,		"Invalid Command Queue"			},
	{ CL_INVALID_HOST_PTR,			"Invalid Host Pointer"			},
	{ CL_INVALID_MEM_OBJECT,		"Invalid Memory Object"			},
	{ CL_INVALID_IMAGE_FORMAT_DESCRIPTOR,	"Invalid Image Format Descriptor"	},
	{ CL_INVALID_IMAGE_SIZE,		"Invalid Image Size"			},
	{ CL_INVALID_SAMPLER,			"Invalid Sampler"			},
	{ CL_INVALID_BINARY,			"Invalid Binary"			},
	{ CL_INVALID_BUILD_OPTIONS,		"Invalid Build Options"			},
	{ CL_INVALID_PROGRAM,			"Invalid Program"			},
	{ CL_INVALID_PROGRAM_EXECUTABLE,	"Invalid Program Executable"		},
	{ CL_INVALID_KERNEL_NAME,		"Invalid Kernel Name"			},
	{ CL_INVALID_KERNEL_DEFINITION,		"Invalid Kernel Definition"		},
	{ CL_INVALID_KERNEL,			"Invalid Kernel"			},
	{ CL_INVALID_ARG_INDEX,			"Invalid Argument Index"		},
	{ CL_INVALID_ARG_VALUE,			"Invalid Argument Value"		},
	{ CL_INVALID_ARG_SIZE,			"Invalid Argument Size"			},
	{ CL_INVALID_KERNEL_ARGS,		"Invalid Kernel Arguments"		},
	{ CL_INVALID_WORK_DIMENSION,		"Invalid Work Dimension"		},
	{ CL_INVALID_WORK_GROUP_SIZE,		"Invalid Work Group Size"		},
	{ CL_INVALID_WORK_ITEM_SIZE,		"Invalid Work Item Size"		},
	{ CL_INVALID_GLOBAL_OFFSET,		"Invalid Global Offset"			},
	{ CL_INVALID_EVENT_WAIT_LIST,		"Invalid Event Wait List"		},
	{ CL_INVALID_EVENT,			"Invalid Event"				},
	{ CL_INVALID_OPERATION,			"Invalid Operation"			},
	{ CL_INVALID_GL_OBJECT,			"Invalid GL Object"			},
	{ CL_INVALID_BUFFER_SIZE,		"Invalid Buffer Size"			},
	{ CL_INVALID_MIP_LEVEL,			"Invalid MIP Level"			},
	{ CL_INVALID_GLOBAL_WORK_SIZE,		"Invalid Global Work Size"		},
};

void
	PrintCLError( cl_int errorCode, char * prefix, FILE *fp )
{
	if( errorCode == CL_SUCCESS )
		return;

	const int numErrorCodes = sizeof( ErrorCodes ) / sizeof( struct errorcode );
	char * meaning = "";
	for( int i = 0; i < numErrorCodes; i++ )
	{
		if( errorCode == ErrorCodes[i].statusCode )
		{
			meaning = ErrorCodes[i].meaning;
			break;
		}
	}

	fprintf( fp, "%s %s\n", prefix, meaning );
	exit(0);
}
