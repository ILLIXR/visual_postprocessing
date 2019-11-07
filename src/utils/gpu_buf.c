#include "gpu_buf.h"
#include <GL/glut.h>

bool ksGpuBuffer_Create( gpu_buffer_type_t * buffer, const gpu_buffer_type_t type,
							const size_t dataSize, const void * data )
{

	buffer->target =	( ( type == GPU_BUFFER_TYPE_VERTEX ) ?	GL_ARRAY_BUFFER :
						( ( type == GPU_BUFFER_TYPE_INDEX ) ?	GL_ELEMENT_ARRAY_BUFFER :
						( ( type == GPU_BUFFER_TYPE_UNIFORM ) ?	GL_UNIFORM_BUFFER :
						( ( type == GPU_BUFFER_TYPE_STORAGE ) ?	GL_SHADER_STORAGE_BUFFER :
																	0 ) ) ) );
	buffer->size = dataSize;

	glGenBuffers( 1, &buffer->buffer );
	glBindBuffer( buffer->target, buffer->buffer);
	glBufferData( buffer->target, dataSize, data, GL_STATIC_DRAW );
	glBindBuffer( buffer->target, 0 );

	buffer->owner = true;

	return true;
}

void ksGpuBuffer_CreateReference( gpu_buffer_type_t * buffer, const gpu_buffer_t * other )
{
	buffer->target = other->target;
	buffer->size = other->size;
	buffer->buffer = other->buffer;
	buffer->owner = false;
}

void ksGpuBuffer_Destroy( gpu_buffer_t * buffer )
{

	if ( buffer->owner )
	{
		glDeleteBuffers( 1, &buffer->buffer );
	}
	memset( buffer, 0, sizeof( ksGpuBuffer ) );
}