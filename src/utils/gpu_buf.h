#ifndef _GPU_BUF_H
#define _GPU_BUF_H

#include <GL/glut.h>

typedef enum
{
	GPU_BUFFER_TYPE_VERTEX,
	GPU_BUFFER_TYPE_INDEX,
	GPU_BUFFER_TYPE_UNIFORM,
	GPU_BUFFER_TYPE_STORAGE
} gpu_buffer_type_t;

typedef struct
{
	GLuint			target;
	GLuint			buffer;
	size_t			size;
	bool			owner;
} gpu_buffer_t;



bool ksGpuBuffer_Create( gpu_buffer_t * buffer, const gpu_buffer_t type,
							const size_t dataSize, const void * data );

void ksGpuBuffer_CreateReference( gpu_buffer_type_t * buffer, const gpu_buffer_t * other );

void ksGpuBuffer_Destroy( gpu_buffer_t * buffer );

#endif