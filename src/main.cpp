///////////////////////////////////////////////////////////////////////////////
// main.cpp
// ========
// testing Frame Buffer Object (FBO) for "Render To Texture" with MSAA
// OpenGL draws the scene directly to a texture object.
//
// GL_EXT_framebuffer_object extension is promoted to a core feature of OpenGL
// version 3.0 (GL_ARB_framebuffer_object)
//
//  AUTHOR: Song Ho Ahn (song.ahn@gmail.com)
// CREATED: 2008-05-16
// UPDATED: 2016-11-14
///////////////////////////////////////////////////////////////////////////////



// in order to get function prototypes from glext.h, define GL_GLEXT_PROTOTYPES before including glext.h
#define GL_GLEXT_PROTOTYPES

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/freeglut.h>
#endif

#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <iomanip>
#include <cstdlib>
#include "glext.h"
#include "glInfo.h"                             // glInfo struct
#include "Timer.h"
#include "utils/algebra.h"
#include "utils/hmd.h"

using std::stringstream;
using std::string;
using std::cout;
using std::endl;
using std::ends;

#define OPENGL_VERSION_MAJOR    4
#define OPENGL_VERSION_MINOR    3
#define GLSL_VERSION            "430 core"
#define GLSL_EXTENSIONS         "#extension GL_EXT_shader_io_blocks : enable\n"


// GLUT CALLBACK functions ////////////////////////////////////////////////////
void displayCB();
void reshapeCB(int w, int h);
void timerCB(int millisec);
void idleCB();
void mouseCB(int button, int stat, int x, int y);
void mouseMotionCB(int x, int y);

// CALLBACK function when exit() called ///////////////////////////////////////
void exitCB();

// function declearations /////////////////////////////////////////////////////
void initGL();
int  initGLUT(int argc, char **argv);
bool initSharedMem();
void clearSharedMem();
void drawString(const char *str, int x, int y, float color[4], void *font);
void drawString3D(const char *str, float pos[3], float color[4], void *font);

// FBO utils
bool checkFramebufferStatus(GLuint fbo);
void printFramebufferInfo(GLuint fbo);
std::string convertInternalFormatToString(GLenum format);
std::string getTextureParameters(GLuint id);
std::string getRenderbufferParameters(GLuint id);


// constants
const int   SCREEN_WIDTH    = 448*2;
const int   SCREEN_HEIGHT   = 320*2;
const float CAMERA_DISTANCE = 6.0f;
const int   TEXT_WIDTH      = 8;
const int   TEXT_HEIGHT     = 13;
const int   TEXTURE_WIDTH   = 256;  // NOTE: texture size cannot be larger than
const int   TEXTURE_HEIGHT  = 256;  // the rendering window size in non-FBO mode
const int   NUM_EYES        = 2;
const int   NUM_COLOR_CHANNELS = 3;

// global variables
GLuint fboId;                       // ID of FBO
GLuint textureId;                   // ID of texture
GLuint rboColorId, rboDepthId;      // IDs of Renderbuffer objects
void *font = GLUT_BITMAP_8_BY_13;
int screenWidth;
int screenHeight;
bool mouseLeftDown;
bool mouseRightDown;
float mouseX, mouseY;
float cameraAngleX;
float cameraAngleY;
float cameraDistance;
bool fboSupported;
bool fboUsed;
int fboSampleCount;
int drawMode;
Timer timer, t1;
float playTime;                     // to compute rotation angle
float renderToTextureTime;          // elapsed time for render-to-texture


// Global HMD and body information
hmd_info_t hmd_info;
body_info_t body_info;

// Distortion shaders and shader program handles
GLuint tw_vertex_shader;
GLuint tw_frag_shader;
GLuint tw_shader_program;

// Eye sampler array
GLuint eye_sampler_0;
GLuint eye_sampler_1;

// Eye index uniform
GLuint tw_eye_index_unif;

// Global VAO
GLuint vao;

// Position and UV attribute locations
GLuint distortion_pos_attr;
GLuint distortion_uv0_attr;
GLuint distortion_uv1_attr;
GLuint distortion_uv2_attr;

// Distortion mesh information
GLuint num_distortion_vertices;
GLuint num_distortion_indices;

// Distortion mesh CPU buffers and GPU VBO handles
mesh_coord3d_t* distortion_positions;
GLuint distortion_positions_vbo;
GLuint* distortion_indices;
GLuint distortion_indices_vbo;
uv_coord_t* distortion_uv0;
GLuint distortion_uv0_vbo;
uv_coord_t* distortion_uv1;
GLuint distortion_uv1_vbo;
uv_coord_t* distortion_uv2;
GLuint distortion_uv2_vbo;

// Handles to the start and end timewarp
// transform matrices (3x4 uniforms)
GLuint tw_start_transform_unif;
GLuint tw_end_transform_unif;

// Basic perspective projection matrix
ksMatrix4x4f basicProjection;

// Basic shaders and basic shader program
GLuint basic_vertex_shader;
GLuint basic_frag_shader;
GLuint basic_shader_program;

// Position and UV attribute locations
GLuint basic_pos_attr;
GLuint basic_uv_attr;

const char* const timeWarpSpatialVertexProgramGLSL =
        "#version " GLSL_VERSION "\n"
        "uniform highp mat3x4 TimeWarpStartTransform;\n"
        "uniform highp mat3x4 TimeWarpEndTransform;\n"
        "in highp vec3 vertexPosition;\n"
        "in highp vec2 vertexUv1;\n"
        "out mediump vec2 fragmentUv1;\n"
        "out gl_PerVertex { vec4 gl_Position; };\n"
        "out mediump vec2 viz;\n"
        "void main( void )\n"
        "{\n"
        "   gl_Position = vec4( vertexPosition, 1.0 );\n"
        "\n"
        "   float displayFraction = vertexPosition.x * 0.5 + 0.5;\n"    // landscape left-to-right
        "\n"
        "   vec3 startUv1 = vec4( vertexUv1, -1.0, 1.0 ) * TimeWarpStartTransform;\n"
        "   vec3 endUv1 = vec4( vertexUv1, -1.0, 1.0 ) * TimeWarpEndTransform;\n"
        "   vec3 curUv1 = mix( startUv1, endUv1, displayFraction );\n"
        "   fragmentUv1 = curUv1.xy * ( 1.0 / max( curUv1.z, 0.00001 ) );\n"
        "   viz = vertexUv1.xy;\n"
        "}\n";

const char* const timeWarpSpatialFragmentProgramGLSL =
    "#version " GLSL_VERSION "\n"
    "uniform highp sampler2D Texture;\n"
    "in mediump vec2 fragmentUv1;\n"
    "in mediump vec2 viz;\n"
    "out lowp vec4 outColor;\n"
    "void main()\n"
    "{\n"
    //" outColor = texture( Texture, fragmentUv1 );\n"
    "   outColor = vec4(fract(fragmentUv1.x * 4.), fract(fragmentUv1.y * 4.), 1.0, 1.0);\n"
    "}\n";

const char* const timeWarpChromaticVertexProgramGLSL =
	"#version " GLSL_VERSION "\n"
	"uniform highp mat3x4 TimeWarpStartTransform;\n"
	"uniform highp mat3x4 TimeWarpEndTransform;\n"
	"in highp vec3 vertexPosition;\n"
	"in highp vec2 vertexUv0;\n"
	"in highp vec2 vertexUv1;\n"
	"in highp vec2 vertexUv2;\n"
	"out mediump vec2 fragmentUv0;\n"
	"out mediump vec2 fragmentUv1;\n"
	"out mediump vec2 fragmentUv2;\n"
	"out gl_PerVertex { vec4 gl_Position; };\n"
	"void main( void )\n"
	"{\n"
	"	gl_Position = vec4( vertexPosition, 1.0 );\n"
	"\n"
	"	float displayFraction = vertexPosition.x * 0.5 + 0.5;\n"	// landscape left-to-right
	"\n"
	"	vec3 startUv0 = vec4( vertexUv0, -1, 1 ) * TimeWarpStartTransform;\n"
	"	vec3 startUv1 = vec4( vertexUv1, -1, 1 ) * TimeWarpStartTransform;\n"
	"	vec3 startUv2 = vec4( vertexUv2, -1, 1 ) * TimeWarpStartTransform;\n"
	"\n"
	"	vec3 endUv0 = vec4( vertexUv0, -1, 1 ) * TimeWarpEndTransform;\n"
	"	vec3 endUv1 = vec4( vertexUv1, -1, 1 ) * TimeWarpEndTransform;\n"
	"	vec3 endUv2 = vec4( vertexUv2, -1, 1 ) * TimeWarpEndTransform;\n"
	"\n"
	"	vec3 curUv0 = mix( startUv0, endUv0, displayFraction );\n"
	"	vec3 curUv1 = mix( startUv1, endUv1, displayFraction );\n"
	"	vec3 curUv2 = mix( startUv2, endUv2, displayFraction );\n"
	"\n"
	"	fragmentUv0 = curUv0.xy * ( 1.0 / max( curUv0.z, 0.00001 ) );\n"
	"	fragmentUv1 = curUv1.xy * ( 1.0 / max( curUv1.z, 0.00001 ) );\n"
	"	fragmentUv2 = curUv2.xy * ( 1.0 / max( curUv2.z, 0.00001 ) );\n"
	"}\n";

const char* const timeWarpChromaticFragmentProgramGLSL =
	"#version " GLSL_VERSION "\n"
	"uniform int ArrayLayer;\n"
	"uniform highp sampler2DArray Texture;\n"
	"in mediump vec2 fragmentUv0;\n"
	"in mediump vec2 fragmentUv1;\n"
	"in mediump vec2 fragmentUv2;\n"
	"out lowp vec4 outColor;\n"
	"void main()\n"
	"{\n"
	"	outColor.r = texture( Texture, vec3( fragmentUv0, ArrayLayer ) ).r;\n"
	"	outColor.g = texture( Texture, vec3( fragmentUv1, ArrayLayer ) ).g;\n"
	"	outColor.b = texture( Texture, vec3( fragmentUv2, ArrayLayer ) ).b;\n"
	"	outColor.a = 1.0;\n"
	"}\n";

const char* const timeWarpChromaticFragmentDebugProgramGLSL =
	"#version " GLSL_VERSION "\n"
	"uniform int ArrayLayer;\n"
	"uniform highp sampler2DArray Texture;\n"
	"in mediump vec2 fragmentUv0;\n"
	"in mediump vec2 fragmentUv1;\n"
	"in mediump vec2 fragmentUv2;\n"
	"out lowp vec4 outColor;\n"
	"void main()\n"
	"{\n"
    "   float chess0 = floor(fragmentUv0.x * 5.0) + floor(fragmentUv0.y * 5.0);"
    "   chess0 = fract(chess0 * 0.5);"
    "   float chess1 = floor(fragmentUv1.x * 5.0) + floor(fragmentUv1.y * 5.0);"
    "   chess1 = fract(chess1 * 0.5);"
    "   float chess2 = floor(fragmentUv2.x * 5.0) + floor(fragmentUv2.y * 5.0);"
    "   chess2 = fract(chess2 * 0.5);"
    "   outColor.r = chess0;\n"
    "   outColor.g = chess1;\n"
    "   outColor.b = chess2;\n"
	"}\n";

const char* const basicVertexShader =
        "#version " GLSL_VERSION "\n"
        "in vec3 vertexPosition;\n"
        "in vec2 vertexUV;"
        "out vec2 vUV;\n"
        "out gl_PerVertex { vec4 gl_Position; };\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4( vertexPosition, 1.0 );\n"
        "   vUV = vertexUV;\n"
        "}\n";

const char* const basicFragmentShader =
        "#version " GLSL_VERSION "\n"
        "uniform highp sampler2DArray Texture;\n"
        "uniform int ArrayLayer;\n"
        "in vec2 vUv;\n"
        "out lowp vec4 outcolor;\n"
        "void main()\n"
        "{\n"
        "   outcolor = vec4(fract(vUv.x * 4.), fract(vUv.y * 4.), 1.0, 1.0);\n"
        "}\n";


GLfloat cube_vertices[24] = {  // Coordinates for the vertices of a cube.
           1,1,1,   1,1,-1,   1,-1,-1,   1,-1,1,
          -1,1,1,  -1,1,-1,  -1,-1,-1,  -1,-1,1  };
          
GLfloat cube_colors[24] = {  // An RGB color value for each vertex
           1,1,1,   1,0,0,   1,1,0,   0,1,0,
           0,0,1,   1,0,1,   0,0,0,   0,1,1  };
          
GLuint cube_indices[24] = {  // Vertex number for the six faces.
          0,1,2,3, 0,3,7,4, 0,4,5,1,
          6,2,1,5, 6,5,4,7, 6,7,3,2  };

GLfloat plane_vertices[8] = {  // Coordinates for the vertices of a plane.
         -1, 1,   1, 1,
          1,-1,   1,-1 };
          
GLfloat plane_uvs[8] = {  // UVs for plane
          0, 1,   1, 1,
          0, 0,   1, 0 };
          
GLuint plane_indices[6] = {  // Plane indices
          0,2,3, 1,0,3 };

void GetHmdViewMatrixForTime( ksMatrix4x4f * viewMatrix, float time )
{

    // FIXME: use double?
    const float offset = time * 2.0f;
    const float degrees = 10.0f;
    const float degreesX = sinf( offset ) * degrees;
    const float degreesY = cosf( offset ) * degrees;

    ksMatrix4x4f_CreateRotation( viewMatrix, degreesX, degreesY, 0.0f );
}

void BuildDistortionMeshes( mesh_coord2d_t * distort_coords[NUM_EYES][NUM_COLOR_CHANNELS], hmd_info_t * hmdInfo )
{
    const float horizontalShiftMeters = ( hmdInfo->lensSeparationInMeters / 2 ) - ( hmdInfo->visibleMetersWide / 4 );
    const float horizontalShiftView = horizontalShiftMeters / ( hmdInfo->visibleMetersWide / 2 );

    for ( int eye = 0; eye < NUM_EYES; eye++ )
    {
        for ( int y = 0; y <= hmdInfo->eyeTilesHigh; y++ )
        {
            const float yf = 1.0f - (float)y / (float)hmdInfo->eyeTilesHigh;

            for ( int x = 0; x <= hmdInfo->eyeTilesWide; x++ )
            {
                const float xf = (float)x / (float)hmdInfo->eyeTilesWide;

                const float in[2] = { ( eye ? -horizontalShiftView : horizontalShiftView ) + xf, yf };
                const float ndcToPixels[2] = { hmdInfo->visiblePixelsWide * 0.25f, hmdInfo->visiblePixelsHigh * 0.5f };
                const float pixelsToMeters[2] = { hmdInfo->visibleMetersWide / hmdInfo->visiblePixelsWide, hmdInfo->visibleMetersHigh / hmdInfo->visiblePixelsHigh };

                float theta[2];
                for ( int i = 0; i < 2; i++ )
                {
                    const float unit = in[i];
                    const float ndc = 2.0f * unit - 1.0f;
                    const float pixels = ndc * ndcToPixels[i];
                    const float meters = pixels * pixelsToMeters[i];
                    const float tanAngle = meters / hmdInfo->metersPerTanAngleAtCenter;
                    theta[i] = tanAngle;
                }

                const float rsq = theta[0] * theta[0] + theta[1] * theta[1];
                const float scale = EvaluateCatmullRomSpline( rsq, hmdInfo->K, hmdInfo->numKnots );
                const float chromaScale[NUM_COLOR_CHANNELS] =
                {
                    scale * ( 1.0f + hmdInfo->chromaticAberration[0] + rsq * hmdInfo->chromaticAberration[1] ),
                    scale,
                    scale * ( 1.0f + hmdInfo->chromaticAberration[2] + rsq * hmdInfo->chromaticAberration[3] )
                };

                const int vertNum = y * ( hmdInfo->eyeTilesWide + 1 ) + x;
                for ( int channel = 0; channel < NUM_COLOR_CHANNELS; channel++ )
                {
                    distort_coords[eye][channel][vertNum].x = chromaScale[channel] * theta[0];
                    distort_coords[eye][channel][vertNum].y = chromaScale[channel] * theta[1];
                }
            }
        }
    }
}

void BuildTimewarp(hmd_info_t* hmdInfo){

    // Calculate the number of vertices+indices in the distortion mesh.
    num_distortion_vertices = ( hmdInfo->eyeTilesHigh + 1 ) * ( hmdInfo->eyeTilesWide + 1 );
    num_distortion_indices = hmdInfo->eyeTilesHigh * hmdInfo->eyeTilesWide * 6;

    // Allocate memory for the elements/indices array.
    distortion_indices = (GLuint*) malloc(num_distortion_indices * sizeof(GLuint));

    // This is just a simple grid/plane index array, nothing fancy.
    // Same for both eye distortions, too!
    for ( int y = 0; y < hmdInfo->eyeTilesHigh; y++ )
    {
        for ( int x = 0; x < hmdInfo->eyeTilesWide; x++ )
        {
            const int offset = ( y * hmdInfo->eyeTilesWide + x ) * 6;

            distortion_indices[offset + 0] = (GLuint)( ( y + 0 ) * ( hmdInfo->eyeTilesWide + 1 ) + ( x + 0 ) );
            distortion_indices[offset + 1] = (GLuint)( ( y + 1 ) * ( hmdInfo->eyeTilesWide + 1 ) + ( x + 0 ) );
            distortion_indices[offset + 2] = (GLuint)( ( y + 0 ) * ( hmdInfo->eyeTilesWide + 1 ) + ( x + 1 ) );

            distortion_indices[offset + 3] = (GLuint)( ( y + 0 ) * ( hmdInfo->eyeTilesWide + 1 ) + ( x + 1 ) );
            distortion_indices[offset + 4] = (GLuint)( ( y + 1 ) * ( hmdInfo->eyeTilesWide + 1 ) + ( x + 0 ) );
            distortion_indices[offset + 5] = (GLuint)( ( y + 1 ) * ( hmdInfo->eyeTilesWide + 1 ) + ( x + 1 ) );
        }
    }
    
    // Allocate memory for the distortion coordinates.
    // These are NOT the actual distortion mesh's vertices,
    // they are calculated distortion grid coefficients
    // that will be used to set the actual distortion mesh's UV space.
    mesh_coord2d_t* tw_mesh_base_ptr = (mesh_coord2d_t *) malloc( NUM_EYES * NUM_COLOR_CHANNELS * num_distortion_vertices * sizeof( mesh_coord2d_t ) );

    // Set the distortion coordinates as a series of arrays
    // that will be written into by the BuildDistortionMeshes() function.
    mesh_coord2d_t * distort_coords[NUM_EYES][NUM_COLOR_CHANNELS] =
    {
        { tw_mesh_base_ptr + 0 * num_distortion_vertices, tw_mesh_base_ptr + 1 * num_distortion_vertices, tw_mesh_base_ptr + 2 * num_distortion_vertices },
        { tw_mesh_base_ptr + 3 * num_distortion_vertices, tw_mesh_base_ptr + 4 * num_distortion_vertices, tw_mesh_base_ptr + 5 * num_distortion_vertices }
    };
    BuildDistortionMeshes( distort_coords, hmdInfo );

    // Allocate memory for position and UV CPU buffers.
    for(int eye = 0; eye < NUM_EYES; eye++){
        distortion_positions = (mesh_coord3d_t *) malloc(NUM_EYES * num_distortion_vertices * sizeof(mesh_coord3d_t));
        distortion_uv0 = (uv_coord_t *) malloc(NUM_EYES * num_distortion_vertices * sizeof(uv_coord_t));
        distortion_uv1 = (uv_coord_t *) malloc(NUM_EYES * num_distortion_vertices * sizeof(uv_coord_t));
        distortion_uv2 = (uv_coord_t *) malloc(NUM_EYES * num_distortion_vertices * sizeof(uv_coord_t));
    }
    
    for ( int eye = 0; eye < NUM_EYES; eye++ )
    {
        for ( int y = 0; y <= hmdInfo->eyeTilesHigh; y++ )
        {
            for ( int x = 0; x <= hmdInfo->eyeTilesWide; x++ )
            {
                const int index = y * ( hmdInfo->eyeTilesWide + 1 ) + x;

                // Set the physical distortion mesh coordinates. These are rectangular/gridlike, not distorted.
                // The distortion is handled by the UVs, not the actual mesh coordinates!
                distortion_positions[eye * num_distortion_vertices + index].x = ( -1.0f + eye + ( (float)x / hmdInfo->eyeTilesWide ) );
                distortion_positions[eye * num_distortion_vertices + index].y = ( -1.0f + 2.0f * ( ( hmdInfo->eyeTilesHigh - (float)y ) / hmdInfo->eyeTilesHigh ) *
                                                    ( (float)( hmdInfo->eyeTilesHigh * hmdInfo->tilePixelsHigh ) / hmdInfo->displayPixelsHigh ) );
                distortion_positions[eye * num_distortion_vertices + index].z = 0.0f;

                // Use the previously-calculated distort_coords to set the UVs on the distortion mesh
                distortion_uv0[eye * num_distortion_vertices + index].u = distort_coords[eye][0][index].x;
                distortion_uv0[eye * num_distortion_vertices + index].v = distort_coords[eye][0][index].y;
                distortion_uv1[eye * num_distortion_vertices + index].u = distort_coords[eye][1][index].x;
                distortion_uv1[eye * num_distortion_vertices + index].v = distort_coords[eye][1][index].y;
                distortion_uv2[eye * num_distortion_vertices + index].u = distort_coords[eye][2][index].x;
                distortion_uv2[eye * num_distortion_vertices + index].v = distort_coords[eye][2][index].y;
                
            }
        }
    }
    // Construct a basic perspective projection
    ksMatrix4x4f_CreateProjectionFov( &basicProjection, 40.0f, 40.0f, 40.0f, 40.0f, 0.1f, 0.0f );

    // This was just temporary.
    free(tw_mesh_base_ptr);

    return;
}


///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{

    GLenum err;
    // init global vars
    initSharedMem();

    // register exit callback
    atexit(exitCB);

    // init GLUT and GL
    initGLUT(argc, argv);
    initGL();

    err = glGetError();
    if(err){
        printf("main, error after initGL: %x\n", err);
    }

    // Creating a texture object for the FBO to be mapped into.
    // This texture will be used to perform the timewarp and lens distortion process.
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    
    // Set the texture parameters for the texture that the FBO will be
    // mapped into.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    // Unbind the texture, we'll re-bind it later when we perform the distortion
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create the FBO, and save the handle.
    glGenFramebuffers(1, &fboId);
    // Bind the FBO as the active framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    err = glGetError();
    if(err){
        printf("main, error after creating and binding fbo: %x\n", err);
    }

    // create a renderbuffer object to store depth info
    // NOTE: A depth renderable image should be attached the FBO for depth test.
    // If we don't attach a depth renderable image to the FBO, then
    // the rendering output will be corrupted because of missing depth test.
    // If you also need stencil test for your rendering, then you must
    // attach additional image to the stencil attachement point, too.
    glGenRenderbuffers(1, &rboDepthId);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepthId);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, TEXTURE_WIDTH, TEXTURE_HEIGHT);
    //glRenderbufferStorageMultisample(GL_RENDERBUFFER, fboSampleCount, GL_DEPTH_COMPONENT, TEXTURE_WIDTH, TEXTURE_HEIGHT);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Attach the texture we created earlier to the FBO.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);

    // attach a renderbuffer to depth attachment point
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepthId);

    // check FBO status
    printFramebufferInfo(fboId);
    bool status = checkFramebufferStatus(fboId);
    if(!status)
        fboUsed = false;

    // Unbind the framebuffer, so the distortion can render
    // to the main framebuffer (aka, the screen)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    err = glGetError();
    if(err){
        printf("main, error after fbo things: %x\n", err);
    }

    // start timer
    timer.start();

    // the last GLUT call (LOOP)
    // window will be shown and display callback is triggered by events
    // NOTE: this call never return main().
    glutMainLoop(); /* Start GLUT event-processing loop */

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
// initialize GLUT for windowing
///////////////////////////////////////////////////////////////////////////////
int initGLUT(int argc, char **argv)
{
    // GLUT stuff for windowing
    // initialization openGL window.
    // It must be called before any other GLUT routine.
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_STENCIL);   // display mode

    glutInitWindowSize(screenWidth, screenHeight);              // window size

    glutInitWindowPosition(100, 100);                           // window location

    glutInitContextVersion(4, 3);
    glutInitContextProfile( GLUT_CORE_PROFILE );



    // finally, create a window with openGL context
    // Window will not displayed until glutMainLoop() is called
    // It returns a unique ID.
    int handle = glutCreateWindow(argv[0]);     // param is the title of window

    // register GLUT callback functions
    glutDisplayFunc(displayCB);
    //glutTimerFunc(33, timerCB, 33);             // redraw only every given millisec
    glutIdleFunc(idleCB);                       // redraw whenever system is idle
    glutReshapeFunc(reshapeCB);
    glutMouseFunc(mouseCB);
    glutMotionFunc(mouseMotionCB);

    return handle;
}


// Return: handle to shader program
GLuint init_and_link_shader (const char* vertex_shader, const char* fragment_shader) {
    GLint result, vertex_shader_handle, fragment_shader_handle, shader_program;

    vertex_shader_handle = glCreateShader(GL_VERTEX_SHADER);
    GLint vshader_len = strlen(vertex_shader);
    glShaderSource(vertex_shader_handle, 1, &vertex_shader, &vshader_len);
    glCompileShader(vertex_shader_handle);
    glGetShaderiv(vertex_shader_handle, GL_COMPILE_STATUS, &result);
    if ( result == GL_FALSE )
    {
        GLchar msg[4096];
        GLsizei length;
        glGetShaderInfoLog( vertex_shader_handle, sizeof( msg ), &length, msg );
        printf( "1 Error: %s\n", msg);
    }

    //////////////////////////////////////////////////////////
    // Create and compile timewarp distortion fragment shader

    GLint fragResult = GL_FALSE;
    fragment_shader_handle = glCreateShader(GL_FRAGMENT_SHADER);
    GLint fshader_len = strlen(fragment_shader);
    glShaderSource(fragment_shader_handle, 1, &fragment_shader, &fshader_len);
    glCompileShader(fragment_shader_handle);
    if(glGetError()){
        printf("Fragment shader compilation failed\n");
    }
    glGetShaderiv(fragment_shader_handle, GL_COMPILE_STATUS, &fragResult);
    if ( fragResult == GL_FALSE )
    {
        GLchar msg[4096];
        GLsizei length;
        glGetShaderInfoLog( fragment_shader_handle, sizeof( msg ), &length, msg );
        printf( "2 Error: %s\n", msg);
        
    }
    
    // Create program and link shaders
    shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader_handle);
    glAttachShader(shader_program, fragment_shader_handle);
    if(glGetError()){
        printf("AttachShader or createProgram failed\n");
    }

    ///////////////////
    // Link and verify

    glLinkProgram(shader_program);

    if(glGetError()){
        printf("Linking failed\n");
    }

    glGetProgramiv(shader_program, GL_LINK_STATUS, &result);
    GLenum err = glGetError();
    if(err){
        printf("initGL, error getting link status, %x", err);
    }
    if ( result == GL_FALSE )
    {
        GLchar msg[4096];
        GLsizei length;
        glGetShaderInfoLog( fragment_shader_handle, sizeof( msg ), &length, msg );
        printf( "3 Error: %s\n", msg);
    }

    if(glGetError()){
        printf("initGL, error at end of initGL");
    }

    // After successful link, detach shaders from shader program
    glDetachShader(shader_program, vertex_shader_handle);
    glDetachShader(shader_program, fragment_shader_handle);

    return shader_program;
}

/* initGL()
 *
 * Initializes, links, and compiles relevant shaders
 * Initializes various VBOs for use in the timewarp distortion shader
 * 
 */
void initGL()
{

    // GL features
    //glEnable(GL_DEPTH_TEST);
    //glEnable(GL_CULL_FACE);

    glClearColor(0, 0, 0, 0);                   // background color
    glClearStencil(0);                          // clear stencil buffer
    glClearDepth(1.0f);                         // 0 is near, 1 is far
    glDepthFunc(GL_LEQUAL);

    // Create and bind global VAO object.
    // This may not be necessary, and I can't
    // really find very many good resources
    // online as to why and how this is needed.
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    ///////////////////////////////////////////////////////
    // Create and compile timewarp distortion vertex shader
    tw_shader_program = init_and_link_shader(timeWarpChromaticVertexProgramGLSL, timeWarpChromaticFragmentDebugProgramGLSL);

    //////////////////////
    // VBO Initialization

    // Acquire attribute and uniform locations from the compiled and linked shader program
    distortion_pos_attr = glGetAttribLocation(tw_shader_program, "vertexPosition");
    distortion_uv0_attr = glGetAttribLocation(tw_shader_program, "vertexUv0");
    distortion_uv1_attr = glGetAttribLocation(tw_shader_program, "vertexUv1");
    distortion_uv2_attr = glGetAttribLocation(tw_shader_program, "vertexUv2");
    tw_start_transform_unif = glGetUniformLocation(tw_shader_program, "TimeWarpStartTransform");
    tw_end_transform_unif = glGetUniformLocation(tw_shader_program, "TimeWarpEndTransform");
    tw_eye_index_unif = glGetUniformLocation(tw_shader_program, "ArrayLayer");
    //eye_sampler_0 = glGetUniformLocation(tw_shader_program, "Texture[0]");
    //eye_sampler_1 = glGetUniformLocation(tw_shader_program, "Texture[1]");

    // Config distortion mesh position vbo
    glGenBuffers(1, &distortion_positions_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, distortion_positions_vbo);
    glBufferData(GL_ARRAY_BUFFER, NUM_EYES * (num_distortion_vertices * 3) * sizeof(GLfloat), distortion_positions, GL_STATIC_DRAW);
    glVertexAttribPointer(distortion_pos_attr, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(distortion_pos_attr);

    // Config distortion uv0 vbo
    glGenBuffers(1, &distortion_uv0_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, distortion_uv0_vbo);
    glBufferData(GL_ARRAY_BUFFER, NUM_EYES * (num_distortion_vertices * 2) * sizeof(GLfloat), distortion_uv0, GL_STATIC_DRAW);
    glVertexAttribPointer(distortion_uv0_attr, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(distortion_uv0_attr);

    // Config distortion uv1 vbo
    glGenBuffers(1, &distortion_uv1_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, distortion_uv1_vbo);
    glBufferData(GL_ARRAY_BUFFER, NUM_EYES * (num_distortion_vertices * 2) * sizeof(GLfloat), distortion_uv1, GL_STATIC_DRAW);
    glVertexAttribPointer(distortion_uv1_attr, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(distortion_uv1_attr);

    // Config distortion uv2 vbo
    glGenBuffers(1, &distortion_uv2_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, distortion_uv2_vbo);
    glBufferData(GL_ARRAY_BUFFER, NUM_EYES * (num_distortion_vertices * 2) * sizeof(GLfloat), distortion_uv2, GL_STATIC_DRAW);
    glVertexAttribPointer(distortion_uv2_attr, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(distortion_uv2_attr);

    // Config distortion mesh indices vbo
    glGenBuffers(1, &distortion_indices_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, distortion_indices_vbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_distortion_indices * sizeof(GLuint), distortion_indices, GL_STATIC_DRAW);

    // Create the basic shader program
    basic_shader_program = init_and_link_shader (basicVertexShader, basicFragmentShader);

    return;

}



///////////////////////////////////////////////////////////////////////////////
// initialize global variables
///////////////////////////////////////////////////////////////////////////////
bool initSharedMem()
{
    screenWidth = SCREEN_WIDTH;
    screenHeight = SCREEN_HEIGHT;

    mouseLeftDown = mouseRightDown = false;
    mouseX = mouseY = 0;

    fboId = rboColorId = rboDepthId = textureId = 0;
    fboSupported = fboUsed = false;
    playTime = renderToTextureTime = 0;

    // Generate reference HMD and physical body dimensions
    GetDefaultHmdInfo(SCREEN_WIDTH, SCREEN_HEIGHT, &hmd_info);
    GetDefaultBodyInfo(&body_info);

    // Construct timewarp meshes and other data
    BuildTimewarp(&hmd_info);

    return true;
}



///////////////////////////////////////////////////////////////////////////////
// clean up global variables
///////////////////////////////////////////////////////////////////////////////
void clearSharedMem()
{
    glDeleteTextures(1, &textureId);
    textureId = 0;

    glDeleteBuffers(1, &distortion_positions_vbo);
    glDeleteBuffers(1, &distortion_indices_vbo);
    glDeleteBuffers(1, &distortion_uv0_vbo);

    // clean up FBO, RBO
    if(fboSupported)
    {
        glDeleteFramebuffers(1, &fboId);
        fboId = 0;
        glDeleteRenderbuffers(1, &rboDepthId);
        rboDepthId = 0;
    }
}



///////////////////////////////////////////////////////////////////////////////
// check FBO completeness
///////////////////////////////////////////////////////////////////////////////
bool checkFramebufferStatus(GLuint fbo)
{
    // check FBO status
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); // bind
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch(status)
    {
    case GL_FRAMEBUFFER_COMPLETE:
        std::cout << "Framebuffer complete." << std::endl;
        return true;

    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        std::cout << "[ERROR] Framebuffer incomplete: Attachment is NOT complete." << std::endl;
        return false;

    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        std::cout << "[ERROR] Framebuffer incomplete: No image is attached to FBO." << std::endl;
        return false;
/*
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
        std::cout << "[ERROR] Framebuffer incomplete: Attached images have different dimensions." << std::endl;
        return false;

    case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
        std::cout << "[ERROR] Framebuffer incomplete: Color attached images have different internal formats." << std::endl;
        return false;
*/
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        std::cout << "[ERROR] Framebuffer incomplete: Draw buffer." << std::endl;
        return false;

    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        std::cout << "[ERROR] Framebuffer incomplete: Read buffer." << std::endl;
        return false;

    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        std::cout << "[ERROR] Framebuffer incomplete: Multisample." << std::endl;
        return false;

    case GL_FRAMEBUFFER_UNSUPPORTED:
        std::cout << "[ERROR] Framebuffer incomplete: Unsupported by FBO implementation." << std::endl;
        return false;

    default:
        std::cout << "[ERROR] Framebuffer incomplete: Unknown error." << std::endl;
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);   // unbind
}



///////////////////////////////////////////////////////////////////////////////
// print out the FBO infos
///////////////////////////////////////////////////////////////////////////////
void printFramebufferInfo(GLuint fbo)
{
    // bind fbo
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    std::cout << "\n===== FBO STATUS =====\n";

    // print max # of colorbuffers supported by FBO
    int colorBufferCount = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &colorBufferCount);
    std::cout << "Max Number of Color Buffer Attachment Points: " << colorBufferCount << std::endl;

    // get max # of multi samples
    int multiSampleCount = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &multiSampleCount);
    std::cout << "Max Number of Samples for MSAA: " << multiSampleCount << std::endl;

    int objectType;
    int objectId;

    // print info of the colorbuffer attachable image
    for(int i = 0; i < colorBufferCount; ++i)
    {
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                              GL_COLOR_ATTACHMENT0+i,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                              &objectType);
        if(objectType != GL_NONE)
        {
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                                  GL_COLOR_ATTACHMENT0+i,
                                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                  &objectId);

            std::string formatName;

            std::cout << "Color Attachment " << i << ": ";
            if(objectType == GL_TEXTURE)
            {
                std::cout << "GL_TEXTURE, " << getTextureParameters(objectId) << std::endl;
            }
            else if(objectType == GL_RENDERBUFFER)
            {
                std::cout << "GL_RENDERBUFFER, " << getRenderbufferParameters(objectId) << std::endl;
            }
        }
    }

    // print info of the depthbuffer attachable image
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                          GL_DEPTH_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                          &objectType);
    if(objectType != GL_NONE)
    {
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                              GL_DEPTH_ATTACHMENT,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                              &objectId);

        std::cout << "Depth Attachment: ";
        switch(objectType)
        {
        case GL_TEXTURE:
            std::cout << "GL_TEXTURE, " << getTextureParameters(objectId) << std::endl;
            break;
        case GL_RENDERBUFFER:
            std::cout << "GL_RENDERBUFFER, " << getRenderbufferParameters(objectId) << std::endl;
            break;
        }
    }

    // print info of the stencilbuffer attachable image
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                          GL_STENCIL_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                          &objectType);
    if(objectType != GL_NONE)
    {
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                              GL_STENCIL_ATTACHMENT,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                              &objectId);

        std::cout << "Stencil Attachment: ";
        switch(objectType)
        {
        case GL_TEXTURE:
            std::cout << "GL_TEXTURE, " << getTextureParameters(objectId) << std::endl;
            break;
        case GL_RENDERBUFFER:
            std::cout << "GL_RENDERBUFFER, " << getRenderbufferParameters(objectId) << std::endl;
            break;
        }
    }

    std::cout << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



///////////////////////////////////////////////////////////////////////////////
// return texture parameters as string using glGetTexLevelParameteriv()
///////////////////////////////////////////////////////////////////////////////
std::string getTextureParameters(GLuint id)
{
    if(glIsTexture(id) == GL_FALSE)
        return "Not texture object";

    int width, height, format;
    std::string formatName;
    glBindTexture(GL_TEXTURE_2D, id);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);            // get texture width
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);          // get texture height
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format); // get texture internal format
    glBindTexture(GL_TEXTURE_2D, 0);

    formatName = convertInternalFormatToString(format);

    std::stringstream ss;
    ss << width << "x" << height << ", " << formatName;
    return ss.str();
}



///////////////////////////////////////////////////////////////////////////////
// return renderbuffer parameters as string using glGetRenderbufferParameteriv
///////////////////////////////////////////////////////////////////////////////
std::string getRenderbufferParameters(GLuint id)
{
    if(glIsRenderbuffer(id) == GL_FALSE)
        return "Not Renderbuffer object";

    int width, height, format, samples;
    std::string formatName;
    glBindRenderbuffer(GL_RENDERBUFFER, id);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);       // get renderbuffer width
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);     // get renderbuffer height
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format); // get renderbuffer internal format
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &samples);   // get multisample count
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    formatName = convertInternalFormatToString(format);

    std::stringstream ss;
    ss << width << "x" << height << ", " << formatName << ", MSAA(" << samples << ")";
    return ss.str();
}



///////////////////////////////////////////////////////////////////////////////
// convert OpenGL internal format enum to string
///////////////////////////////////////////////////////////////////////////////
std::string convertInternalFormatToString(GLenum format)
{
    std::string formatName;

    switch(format)
    {
    case GL_STENCIL_INDEX:      // 0x1901
        formatName = "GL_STENCIL_INDEX";
        break;
    case GL_DEPTH_COMPONENT:    // 0x1902
        formatName = "GL_DEPTH_COMPONENT";
        break;
    case GL_ALPHA:              // 0x1906
        formatName = "GL_ALPHA";
        break;
    case GL_RGB:                // 0x1907
        formatName = "GL_RGB";
        break;
    case GL_RGBA:               // 0x1908
        formatName = "GL_RGBA";
        break;
    case GL_LUMINANCE:          // 0x1909
        formatName = "GL_LUMINANCE";
        break;
    case GL_LUMINANCE_ALPHA:    // 0x190A
        formatName = "GL_LUMINANCE_ALPHA";
        break;
    case GL_R3_G3_B2:           // 0x2A10
        formatName = "GL_R3_G3_B2";
        break;
    case GL_ALPHA4:             // 0x803B
        formatName = "GL_ALPHA4";
        break;
    case GL_ALPHA8:             // 0x803C
        formatName = "GL_ALPHA8";
        break;
    case GL_ALPHA12:            // 0x803D
        formatName = "GL_ALPHA12";
        break;
    case GL_ALPHA16:            // 0x803E
        formatName = "GL_ALPHA16";
        break;
    case GL_LUMINANCE4:         // 0x803F
        formatName = "GL_LUMINANCE4";
        break;
    case GL_LUMINANCE8:         // 0x8040
        formatName = "GL_LUMINANCE8";
        break;
    case GL_LUMINANCE12:        // 0x8041
        formatName = "GL_LUMINANCE12";
        break;
    case GL_LUMINANCE16:        // 0x8042
        formatName = "GL_LUMINANCE16";
        break;
    case GL_LUMINANCE4_ALPHA4:  // 0x8043
        formatName = "GL_LUMINANCE4_ALPHA4";
        break;
    case GL_LUMINANCE6_ALPHA2:  // 0x8044
        formatName = "GL_LUMINANCE6_ALPHA2";
        break;
    case GL_LUMINANCE8_ALPHA8:  // 0x8045
        formatName = "GL_LUMINANCE8_ALPHA8";
        break;
    case GL_LUMINANCE12_ALPHA4: // 0x8046
        formatName = "GL_LUMINANCE12_ALPHA4";
        break;
    case GL_LUMINANCE12_ALPHA12:// 0x8047
        formatName = "GL_LUMINANCE12_ALPHA12";
        break;
    case GL_LUMINANCE16_ALPHA16:// 0x8048
        formatName = "GL_LUMINANCE16_ALPHA16";
        break;
    case GL_INTENSITY:          // 0x8049
        formatName = "GL_INTENSITY";
        break;
    case GL_INTENSITY4:         // 0x804A
        formatName = "GL_INTENSITY4";
        break;
    case GL_INTENSITY8:         // 0x804B
        formatName = "GL_INTENSITY8";
        break;
    case GL_INTENSITY12:        // 0x804C
        formatName = "GL_INTENSITY12";
        break;
    case GL_INTENSITY16:        // 0x804D
        formatName = "GL_INTENSITY16";
        break;
    case GL_RGB4:               // 0x804F
        formatName = "GL_RGB4";
        break;
    case GL_RGB5:               // 0x8050
        formatName = "GL_RGB5";
        break;
    case GL_RGB8:               // 0x8051
        formatName = "GL_RGB8";
        break;
    case GL_RGB10:              // 0x8052
        formatName = "GL_RGB10";
        break;
    case GL_RGB12:              // 0x8053
        formatName = "GL_RGB12";
        break;
    case GL_RGB16:              // 0x8054
        formatName = "GL_RGB16";
        break;
    case GL_RGBA2:              // 0x8055
        formatName = "GL_RGBA2";
        break;
    case GL_RGBA4:              // 0x8056
        formatName = "GL_RGBA4";
        break;
    case GL_RGB5_A1:            // 0x8057
        formatName = "GL_RGB5_A1";
        break;
    case GL_RGBA8:              // 0x8058
        formatName = "GL_RGBA8";
        break;
    case GL_RGB10_A2:           // 0x8059
        formatName = "GL_RGB10_A2";
        break;
    case GL_RGBA12:             // 0x805A
        formatName = "GL_RGBA12";
        break;
    case GL_RGBA16:             // 0x805B
        formatName = "GL_RGBA16";
        break;
    case GL_DEPTH_COMPONENT16:  // 0x81A5
        formatName = "GL_DEPTH_COMPONENT16";
        break;
    case GL_DEPTH_COMPONENT24:  // 0x81A6
        formatName = "GL_DEPTH_COMPONENT24";
        break;
    case GL_DEPTH_COMPONENT32:  // 0x81A7
        formatName = "GL_DEPTH_COMPONENT32";
        break;
    case GL_DEPTH_STENCIL:      // 0x84F9
        formatName = "GL_DEPTH_STENCIL";
        break;
    case GL_RGBA32F:            // 0x8814
        formatName = "GL_RGBA32F";
        break;
    case GL_RGB32F:             // 0x8815
        formatName = "GL_RGB32F";
        break;
    case GL_RGBA16F:            // 0x881A
        formatName = "GL_RGBA16F";
        break;
    case GL_RGB16F:             // 0x881B
        formatName = "GL_RGB16F";
        break;
    case GL_DEPTH24_STENCIL8:   // 0x88F0
        formatName = "GL_DEPTH24_STENCIL8";
        break;
    default:
        std::stringstream ss;
        ss << "Unknown Format(0x" << std::hex << format << ")" << std::ends;
        formatName = ss.str();
    }

    return formatName;
}


///////////////////////////////////////////////////////////////////////////////
// Get timewarp transform from projection/view/new view matrix
///////////////////////////////////////////////////////////////////////////////
void CalculateTimeWarpTransform( ksMatrix4x4f * transform, const ksMatrix4x4f * renderProjectionMatrix,
                                        const ksMatrix4x4f * renderViewMatrix, const ksMatrix4x4f * newViewMatrix )
{
    // Convert the projection matrix from [-1, 1] space to [0, 1] space.
    const ksMatrix4x4f texCoordProjection =
    { {
        { 0.5f * renderProjectionMatrix->m[0][0],        0.0f,                                           0.0f,  0.0f },
        { 0.0f,                                          0.5f * renderProjectionMatrix->m[1][1],         0.0f,  0.0f },
        { 0.5f * renderProjectionMatrix->m[2][0] - 0.5f, 0.5f * renderProjectionMatrix->m[2][1] - 0.5f, -1.0f,  0.0f },
        { 0.0f,                                          0.0f,                                           0.0f,  1.0f }
    } };

    // Calculate the delta between the view matrix used for rendering and
    // a more recent or predicted view matrix based on new sensor input.
    ksMatrix4x4f inverseRenderViewMatrix;
    ksMatrix4x4f_InvertHomogeneous( &inverseRenderViewMatrix, renderViewMatrix );

    ksMatrix4x4f deltaViewMatrix;
    ksMatrix4x4f_Multiply( &deltaViewMatrix, &inverseRenderViewMatrix, newViewMatrix );

    ksMatrix4x4f inverseDeltaViewMatrix;
    ksMatrix4x4f_InvertHomogeneous( &inverseDeltaViewMatrix, &deltaViewMatrix );

    // Make the delta rotation only.
    inverseDeltaViewMatrix.m[3][0] = 0.0f;
    inverseDeltaViewMatrix.m[3][1] = 0.0f;
    inverseDeltaViewMatrix.m[3][2] = 0.0f;

    // Accumulate the transforms.
    ksMatrix4x4f_Multiply( transform, &texCoordProjection, &inverseDeltaViewMatrix );
}



//=============================================================================
// CALLBACKS
//=============================================================================

void displayCB()
{
    GLenum err;

    // get the total elapsed time
    playTime = (float)timer.getElapsedTime();

    // render to texture //////////////////////////////////////////////////////
    t1.start();
    
    // with FBO
    // render directly to a texture
    // set the rendering destination to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    if(glGetError()){
        printf("displayCB, error after binding FBO for render");
    }

    // clear buffer
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(tw_shader_program);

    glUniform1f(glGetUniformLocation(tw_shader_program, "override"), 1.0);
    
    ////////////////////////////////////////////////////////////////////////
    // DRAW SOMETHING TO BOUND FBO!

    /*
    // Config position vbo
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), plane_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_uv);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), plane_uvs, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);

    // Draw basic plane to put something in the FBO for testing
    glDrawArrays(GL_TRIANGLES, 0, 6);

    */
    ////////////////////////////////////////////////////////////////////////

    // back to normal window-system-provided framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // unbind

    if(glGetError()){
        printf("displayCB, error after unbinding FBO after render");
    }

    // trigger mipmaps generation explicitly
    // NOTE: If GL_GENERATE_MIPMAP is set to GL_TRUE, then glCopyTexSubImage2D()
    // triggers mipmap generation automatically. However, the texture attached
    // onto a FBO should generate mipmaps manually via glGenerateMipmap().
    glBindTexture(GL_TEXTURE_2D, textureId);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    // measure the elapsed time of render-to-texture
    t1.stop();
    renderToTextureTime = t1.getElapsedTimeInMilliSec();
    ///////////////////////////////////////////////////////////////////////////


    // rendering as normal ////////////////////////////////////////////////////

    // Render to screen viewport
    glViewport(0, 0, screenWidth, screenHeight);
    // Clear color and depth buffers (stencil too, but not used)
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Use the timewarp program
    glUseProgram(tw_shader_program);

    // Identity viewMatrix, simulates
    // the rendered scene's view matrix.
    ksMatrix4x4f viewMatrix;
    ksMatrix4x4f_CreateIdentity(&viewMatrix);

    // We simulate two asynchronous view matrices,
    // one at the beginning of display refresh,
    // and one at the end of display refresh.
    // The distortion shader will lerp between
    // these two predictive view transformations
    // as it renders across the horizontal view,
    // compensating for display panel refresh delay (wow!)
    ksMatrix4x4f viewMatrixBegin;
    ksMatrix4x4f viewMatrixEnd;

    // Get HMD view matrices, one for the beginning of the
    // panel refresh, one for the end. (Exaggerated effect,
    // this is set to 0.1s refresh time.)
    GetHmdViewMatrixForTime(&viewMatrixBegin, playTime);
    GetHmdViewMatrixForTime(&viewMatrixEnd, playTime + 0.1f);

    // Calculate the timewarp transformation matrices.
    // These are a product of the last-known-good view matrix
    // and the predictive transforms.
    ksMatrix4x4f timeWarpStartTransform4x4;
    ksMatrix4x4f timeWarpEndTransform4x4;

    // Calculate timewarp transforms using predictive view transforms
    CalculateTimeWarpTransform(&timeWarpStartTransform4x4, &basicProjection, &viewMatrix, &viewMatrixBegin);
    CalculateTimeWarpTransform(&timeWarpEndTransform4x4, &basicProjection, &viewMatrix, &viewMatrixEnd);

    // We transform from 4x4 to 3x4 as we operate on vec3's in NDC space
    ksMatrix3x4f timeWarpStartTransform3x4;
    ksMatrix3x4f timeWarpEndTransform3x4;
    ksMatrix3x4f_CreateFromMatrix4x4f( &timeWarpStartTransform3x4, &timeWarpStartTransform4x4 );
    ksMatrix3x4f_CreateFromMatrix4x4f( &timeWarpEndTransform3x4, &timeWarpEndTransform4x4 );

    // Push timewarp transform matrices to timewarp shader
    glUniformMatrix3x4fv(tw_start_transform_unif, 1, GL_FALSE, (GLfloat*)&(timeWarpStartTransform3x4.m[0][0]));
    glUniformMatrix3x4fv(tw_end_transform_unif, 1, GL_FALSE,  (GLfloat*)&(timeWarpEndTransform3x4.m[0][0]));

    // Debugging aid, toggle switch for rendering in the fragment shader
    glUniform1f(glGetUniformLocation(tw_shader_program, "override"), 0.0);

    // Bind the FBO's previously-generated glTexture
    glBindTexture(GL_TEXTURE_2D, textureId);

    // Loop over each eye.
    for(int eye = 0; eye < NUM_EYES; eye++){

        // The distortion_positions_vbo GPU buffer already contains
        // the distortion mesh for both eyes! They are contiguously
        // laid out in GPU memory. Therefore, on each eye render,
        // we set the attribute pointer to be offset by the full
        // eye's distortion mesh size, rendering the correct eye mesh
        // to that region of the screen. This prevents re-uploading
        // GPU data for each eye.
        glBindBuffer(GL_ARRAY_BUFFER, distortion_positions_vbo);
        glVertexAttribPointer(distortion_pos_attr, 3, GL_FLOAT, GL_FALSE, 0, (void*)(eye * num_distortion_vertices * sizeof(mesh_coord3d_t)));
        glEnableVertexAttribArray(distortion_pos_attr);

        // We do the exact same thing for the UV GPU memory.
        glBindBuffer(GL_ARRAY_BUFFER, distortion_uv0_vbo);
        glVertexAttribPointer(distortion_uv0_attr, 2, GL_FLOAT, GL_FALSE, 0, (void*)(eye * num_distortion_vertices * sizeof(mesh_coord2d_t)));
        glEnableVertexAttribArray(distortion_uv0_attr);

        // We do the exact same thing for the UV GPU memory.
        glBindBuffer(GL_ARRAY_BUFFER, distortion_uv1_vbo);
        glVertexAttribPointer(distortion_uv1_attr, 2, GL_FLOAT, GL_FALSE, 0, (void*)(eye * num_distortion_vertices * sizeof(mesh_coord2d_t)));
        glEnableVertexAttribArray(distortion_uv1_attr);

        // We do the exact same thing for the UV GPU memory.
        glBindBuffer(GL_ARRAY_BUFFER, distortion_uv2_vbo);
        glVertexAttribPointer(distortion_uv2_attr, 2, GL_FLOAT, GL_FALSE, 0, (void*)(eye * num_distortion_vertices * sizeof(mesh_coord2d_t)));
        glEnableVertexAttribArray(distortion_uv2_attr);


        // Interestingly, the element index buffer is identical for both eyes, and is
        // reused for both eyes. Therefore glDrawElements can be immediately called,
        // with the UV and position buffers correctly offset.
        glDrawElements(GL_TRIANGLES, num_distortion_indices, GL_UNSIGNED_INT, (void*)0);

        err = glGetError();
        if(err){
            printf("displayCB, error after drawElements, %x", err);
        }
    }

    glutSwapBuffers();
}


void idleCB()
{
    glutPostRedisplay();
}

void reshapeCB(int width, int height)
{
    screenWidth = width;
    screenHeight = height;
}


void timerCB(int millisec)
{
    glutTimerFunc(millisec, timerCB, millisec);
    glutPostRedisplay();
}

void mouseCB(int button, int state, int x, int y)
{
    mouseX = x;
    mouseY = y;

    if(button == GLUT_LEFT_BUTTON)
    {
        if(state == GLUT_DOWN)
        {
            mouseLeftDown = true;
        }
        else if(state == GLUT_UP)
            mouseLeftDown = false;
    }

    else if(button == GLUT_RIGHT_BUTTON)
    {
        if(state == GLUT_DOWN)
        {
            mouseRightDown = true;
        }
        else if(state == GLUT_UP)
            mouseRightDown = false;
    }
}


void mouseMotionCB(int x, int y)
{
    if(mouseLeftDown)
    {
        cameraAngleY += (x - mouseX);
        cameraAngleX += (y - mouseY);
        mouseX = x;
        mouseY = y;
    }
    if(mouseRightDown)
    {
        cameraDistance -= (y - mouseY) * 0.2f;
        mouseY = y;
    }
}


void exitCB()
{
    clearSharedMem();
}
