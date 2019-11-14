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
#include "teapot.h"
#include "utils/algebra.h"
#include "utils/hmd.h"

using std::stringstream;
using std::string;
using std::cout;
using std::endl;
using std::ends;

#define OPENGL_VERSION_MAJOR	4
#define OPENGL_VERSION_MINOR	3
#define GLSL_VERSION			"430 core"
#define GLSL_EXTENSIONS     	"#extension GL_EXT_shader_io_blocks : enable\n"


// GLUT CALLBACK functions ////////////////////////////////////////////////////
void displayCB();
void reshapeCB(int w, int h);
void timerCB(int millisec);
void idleCB();
void keyboardCB(unsigned char key, int x, int y);
void mouseCB(int button, int stat, int x, int y);
void mouseMotionCB(int x, int y);

// CALLBACK function when exit() called ///////////////////////////////////////
void exitCB();

// function declearations /////////////////////////////////////////////////////
void initGL();
int  initGLUT(int argc, char **argv);
bool initSharedMem();
void clearSharedMem();
void initLights();
void setCamera(float posX, float posY, float posZ, float targetX, float targetY, float targetZ);
void drawString(const char *str, int x, int y, float color[4], void *font);
void drawString3D(const char *str, float pos[3], float color[4], void *font);
void showInfo();
void showFPS();
void toOrtho();
void toPerspective();
void draw();

// FBO utils
bool checkFramebufferStatus(GLuint fbo);
void printFramebufferInfo(GLuint fbo);
std::string convertInternalFormatToString(GLenum format);
std::string getTextureParameters(GLuint id);
std::string getRenderbufferParameters(GLuint id);


// constants
const int   SCREEN_WIDTH    = 400;
const int   SCREEN_HEIGHT   = 300;
const float CAMERA_DISTANCE = 6.0f;
const int   TEXT_WIDTH      = 8;
const int   TEXT_HEIGHT     = 13;
const int   TEXTURE_WIDTH   = 256;  // NOTE: texture size cannot be larger than
const int   TEXTURE_HEIGHT  = 256;  // the rendering window size in non-FBO mode

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

GLuint tw_vertex_shader;
GLuint tw_frag_shader;
GLuint tw_shader_program;

GLint textureLocation;

GLuint vao;
GLuint vbo_pos;
GLuint vbo_norm;
GLuint vbo_uv;

const char* const timeWarpSpatialVertexProgramGLSL =
        "#version " GLSL_VERSION "\n"
        GLSL_EXTENSIONS
        "uniform highp mat4 TimeWarpStartTransform;\n"
        "uniform highp mat4 TimeWarpEndTransform;\n"
        "in highp vec3 vertexPosition;\n"
	    "in highp vec2 vertexUv1;\n"
        "out mediump vec2 fragmentUv1;\n"
        "out gl_PerVertex { vec4 gl_Position; };\n"
        "void main( void )\n"
        "{\n"
        "	gl_Position = vec4( vertexPosition, 1.0 );;\n"
        "\n"
        "	float displayFraction = vertexPosition.x * 0.5 + 0.5;\n"	// landscape left-to-right
        "\n"
        "	vec4 startUv1 = vec4( vertexUv1, -1, 1 ) * TimeWarpStartTransform;\n"
        "	vec4 endUv1 = vec4( vertexUv1, -1, 1 ) * TimeWarpEndTransform;\n"
        "	vec4 curUv1 = mix( startUv1, endUv1, displayFraction );\n"
        "	fragmentUv1 = curUv1.xy * ( 1.0 / max( curUv1.z, 0.00001 ) );\n"
        "}\n";

const char* const timeWarpSpatialFragmentProgramGLSL =
    "uniform highp sampler2D Texture;\n"
    "in mediump vec2 fragmentUv1;\n"
    "out lowp vec4 outColor;\n"
    "void main()\n"
    "{\n"
    //"	outColor = texture( Texture, fragmentUv1 );\n"
    "	outColor = vec4(1.0);\n"
    "}\n";

const char* const basicVertexShader =
        "#version " GLSL_VERSION "\n"
        "in vec3 vertexPosition;\n"
        "in vec2 vertexUv1;"
        "out vec3 vPos;\n"
        "out vec2 vUv;"
        "void main()\n"
        "{\n"
        "	gl_Position = vec4( vertexPosition, 1.0 );\n"
        "   vPos = vertexPosition;\n"
        "   vUv = vertexUv1;\n"
        "}\n";

const char* const basicFragmentShader =
        "#version " GLSL_VERSION "\n"
        "uniform highp sampler2D Texture;\n"
        "uniform float override;\n"
        "in vec3 vPos;\n"
        "in vec2 vUv;\n"
        "out vec4 outcolor;\n"
        "void main()\n"
        "{\n"
        "   if(override > 0.5)\n"
        "       outcolor = vec4(vUv.x, vUv.y, 1.0, 1.0);\n"
        "   else\n"
        "	    outcolor = texture( Texture, vUv );\n"
        //"	outcolor = vec4(vUv.x, vUv.y, 1.0, 1.0);\n"
        "}\n";


const GLfloat plane_vertices[] = {
    -0.9, -0.9, 0.0,
     0.9, -0.9, 0.0,
    -0.9,  0.9, 0.0,

     0.9, -0.9, 0.0,
     0.9, 0.9, -0.0,
    -0.9,  0.9, 0.0
};

const GLfloat small_plane_vertices[] = {
    -0.1, -0.1, 0.0,
     0.1, -0.1, 0.0,
    -0.1,  0.1, 0.0,

     0.1, -0.1, 0.0,
     0.1, 0.1, -0.0,
    -0.1,  0.1, 0.0
};

const GLfloat plane_normals[] = {
    0.0, 0.0, 1.0,
    0.0, 0.0, 1.0,
    0.0, 0.0, 1.0,
    0.0, 0.0, 1.0,
    0.0, 0.0, 1.0,
    0.0, 0.0, 1.0
};

const GLfloat plane_uvs[] = {
     0.0, 0.0,
     1.0, 0.0,
     0.0, 1.0,

     1.0, 0.0,
     1.0, 1.0,
     0.0, 1.0
};

///////////////////////////////////////////////////////////////////////////////
// draw a textured cube with GL_TRIANGLES
///////////////////////////////////////////////////////////////////////////////
void draw()
{
    
    glDrawArrays(GL_TRIANGLES, 0, 6);

    /*
    glBindTexture(GL_TEXTURE_2D, textureId);

    glColor4f(1, 1, 1, 1);
    glBegin(GL_TRIANGLES);
        // front faces
        glNormal3f(0,0,1);
        // face v0-v1-v2
        glTexCoord2f(1,1);  glVertex3f(1,1,1);
        glTexCoord2f(0,1);  glVertex3f(-1,1,1);
        glTexCoord2f(0,0);  glVertex3f(-1,-1,1);
        // face v2-v3-v0
        glTexCoord2f(0,0);  glVertex3f(-1,-1,1);
        glTexCoord2f(1,0);  glVertex3f(1,-1,1);
        glTexCoord2f(1,1);  glVertex3f(1,1,1);

        // right faces
        glNormal3f(1,0,0);
        // face v0-v3-v4
        glTexCoord2f(0,1);  glVertex3f(1,1,1);
        glTexCoord2f(0,0);  glVertex3f(1,-1,1);
        glTexCoord2f(1,0);  glVertex3f(1,-1,-1);
        // face v4-v5-v0
        glTexCoord2f(1,0);  glVertex3f(1,-1,-1);
        glTexCoord2f(1,1);  glVertex3f(1,1,-1);
        glTexCoord2f(0,1);  glVertex3f(1,1,1);

        // top faces
        glNormal3f(0,1,0);
        // face v0-v5-v6
        glTexCoord2f(1,0);  glVertex3f(1,1,1);
        glTexCoord2f(1,1);  glVertex3f(1,1,-1);
        glTexCoord2f(0,1);  glVertex3f(-1,1,-1);
        // face v6-v1-v0
        glTexCoord2f(0,1);  glVertex3f(-1,1,-1);
        glTexCoord2f(0,0);  glVertex3f(-1,1,1);
        glTexCoord2f(1,0);  glVertex3f(1,1,1);

        // left faces
        glNormal3f(-1,0,0);
        // face  v1-v6-v7
        glTexCoord2f(1,1);  glVertex3f(-1,1,1);
        glTexCoord2f(0,1);  glVertex3f(-1,1,-1);
        glTexCoord2f(0,0);  glVertex3f(-1,-1,-1);
        // face v7-v2-v1
        glTexCoord2f(0,0);  glVertex3f(-1,-1,-1);
        glTexCoord2f(1,0);  glVertex3f(-1,-1,1);
        glTexCoord2f(1,1);  glVertex3f(-1,1,1);

        // bottom faces
        glNormal3f(0,-1,0);
        // face v7-v4-v3
        glTexCoord2f(0,0);  glVertex3f(-1,-1,-1);
        glTexCoord2f(1,0);  glVertex3f(1,-1,-1);
        glTexCoord2f(1,1);  glVertex3f(1,-1,1);
        // face v3-v2-v7
        glTexCoord2f(1,1);  glVertex3f(1,-1,1);
        glTexCoord2f(0,1);  glVertex3f(-1,-1,1);
        glTexCoord2f(0,0);  glVertex3f(-1,-1,-1);

        // back faces
        glNormal3f(0,0,-1);
        // face v4-v7-v6
        glTexCoord2f(0,0);  glVertex3f(1,-1,-1);
        glTexCoord2f(1,0);  glVertex3f(-1,-1,-1);
        glTexCoord2f(1,1);  glVertex3f(-1,1,-1);
        // face v6-v5-v4
        glTexCoord2f(1,1);  glVertex3f(-1,1,-1);
        glTexCoord2f(0,1);  glVertex3f(1,1,-1);
        glTexCoord2f(0,0);  glVertex3f(1,-1,-1);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    */
}

void BuildDistortionMeshes( mesh_coord_t * meshCoords[NUM_EYES][NUM_COLOR_CHANNELS], hmd_info_t * hmdInfo )
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
					meshCoords[eye][channel][vertNum].x = chromaScale[channel] * theta[0];
					meshCoords[eye][channel][vertNum].y = chromaScale[channel] * theta[1];
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    // init global vars
    initSharedMem();

    // register exit callback
    atexit(exitCB);

    // init GLUT and GL
    initGLUT(argc, argv);
    initGL();

    if(glGetError()){
        printf("main, error after initGL");
    }

    // create a texture object
    glGenTextures(1, &textureId);
    if(glGetError()){
        printf("main, error after glGenTextures");
    }
    glBindTexture(GL_TEXTURE_2D, textureId);
    if(glGetError()){
        printf("main, gl error");
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE); // automatic mipmap generation included in OpenGL v1.4
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // create a framebuffer object, you need to delete them when program exits.
    glGenFramebuffers(1, &fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

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

    // attach a texture to FBO color attachement point
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
    //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);

    // attach a renderbuffer to depth attachment point
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepthId);

    //@@ disable color buffer if you don't attach any color buffer image,
    //@@ for example, rendering the depth buffer only to a texture.
    //@@ Otherwise, glCheckFramebufferStatus will not be complete.
    //glDrawBuffer(GL_NONE);
    //glReadBuffer(GL_NONE);

    // check FBO status
    printFramebufferInfo(fboId);
    bool status = checkFramebufferStatus(fboId);
    if(!status)
        fboUsed = false;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if(glGetError()){
        printf("main, error after fbo things");
    }

    // start timer, the elapsed time will be used for rotating the teapot
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
    //glutKeyboardFunc(keyboardCB);
    glutMouseFunc(mouseCB);
    glutMotionFunc(mouseMotionCB);

    return handle;
}



///////////////////////////////////////////////////////////////////////////////
// initialize OpenGL
// disable unused features
///////////////////////////////////////////////////////////////////////////////
void initGL()
{
    //glShadeModel(GL_SMOOTH);                    // shading mathod: GL_SMOOTH or GL_FLAT
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 4);      // 4-byte pixel alignment

    // enable /disable features
    //glEnable(GL_DEPTH_TEST);
    //glEnable(GL_CULL_FACE);

    glClearColor(0, 0, 0, 0);                   // background color
    glClearStencil(0);                          // clear stencil buffer
    glClearDepth(1.0f);                         // 0 is near, 1 is far
    glDepthFunc(GL_LEQUAL);

    //initLights();

    glGenVertexArrays(1, &vao);

    glBindVertexArray(vao);

    glGenBuffers(1, &vbo_pos);

    glGenBuffers(1, &vbo_uv);

    if(glGetError()){
        printf("GenBuffers or bindvertex array failed\n");
    }

    // Config position vbo
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, 18 * sizeof(GLfloat), plane_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    if(glGetError()){
        printf("something in position buffer failed\n");
    }

    // Config uv vbo
    glBindBuffer(GL_ARRAY_BUFFER, vbo_uv);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), plane_uvs, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);

    if(glGetError()){
        printf("Uv buffer failed\n");
    }

    // Config normal vbo
    //glBindBuffer(GL_ARRAY_BUFFER, vbo_norm);
    //glBufferData(GL_ARRAY_BUFFER, 18 * sizeof(GLfloat), plane_normals, GL_STATIC_DRAW);
    //glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
    //glEnableVertexAttribArray(2);
    
    GLint result;
    tw_vertex_shader = glCreateShader(GL_VERTEX_SHADER);

    
    if(glGetError()){
        printf("glCreateShader for vertex shader failed\n");
    }
    GLint vshader_len = strlen(basicVertexShader);
    glShaderSource(tw_vertex_shader, 1, &basicVertexShader, &vshader_len);
    if(glGetError()){
        printf("shaderSource for vertex shader failed\n");
    }
    glCompileShader(tw_vertex_shader);
    if(glGetError()){
        printf("compileShader for vertex shader failed\n");
    }
    glGetShaderiv(tw_vertex_shader, GL_COMPILE_STATUS, &result);
    if(glGetError()){
        printf("aaaah!\n");
    }
    if ( result == GL_FALSE )
	{
		GLchar msg[4096];
		GLsizei length;
		glGetShaderInfoLog( tw_vertex_shader, sizeof( msg ), &length, msg );
		printf( "Error: %s\n", msg);
	}

    GLint fragResult = GL_FALSE;
    tw_frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    if(glGetError()){
        printf("createShader for fragment shader failed\n");
    }
    GLint fshader_len = strlen(basicFragmentShader);
    glShaderSource(tw_frag_shader, 1, &basicFragmentShader, &fshader_len);
    if(glGetError()){
        printf("shaderSource for fragment shader failed\n");
    }
    glCompileShader(tw_frag_shader);
    if(glGetError()){
        printf("compileShader for fragshader failed\n");
    }
    glGetShaderiv(tw_frag_shader, GL_COMPILE_STATUS, &fragResult);
    if ( fragResult == GL_FALSE )
	{
        GLchar msg[4096];
        GLsizei length;
        glGetShaderInfoLog( tw_frag_shader, sizeof( msg ), &length, msg );
        printf( "Error: %s\n", msg);
		
	}
    

    tw_shader_program = glCreateProgram();
    glAttachShader(tw_shader_program, tw_vertex_shader);
    glAttachShader(tw_shader_program, tw_frag_shader);

    if(glGetError()){
        printf("AttachShader or createProgram failed\n");
    }


    glBindAttribLocation(tw_shader_program, 0, "vertexPosition");
    glBindAttribLocation(tw_shader_program, 1, "vertexUv1");

    if(glGetError()){
        printf("Binding attributes failed\n");
    } else {
        printf("Successfully bound attrib locations\n");
    }



    glLinkProgram(tw_shader_program);

    if(glGetError()){
        printf("Linking failed\n");
    }

    glGetProgramiv(tw_shader_program, GL_LINK_STATUS, &result);

    GLenum err = glGetError();
    if(err){
        printf("initGL, error getting link status, %x", err);
    }
    if ( result == GL_FALSE )
	{
		GLchar msg[4096];
		GLsizei length;
		glGetShaderInfoLog( tw_frag_shader, sizeof( msg ), &length, msg );
		printf( "Error: %s\n", msg);
	}

    if(glGetError()){
        printf("initGL, error at end of initGL");
    }

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

    cameraAngleX = cameraAngleY = 45;
    cameraDistance = 4;

    drawMode = 0; // 0:fill, 1: wireframe, 2:points

    fboId = rboColorId = rboDepthId = textureId = 0;
    fboSupported = fboUsed = false;
    playTime = renderToTextureTime = 0;


    return true;
}



///////////////////////////////////////////////////////////////////////////////
// clean up global variables
///////////////////////////////////////////////////////////////////////////////
void clearSharedMem()
{
    glDeleteTextures(1, &textureId);
    textureId = 0;

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
// set projection matrix as orthogonal
///////////////////////////////////////////////////////////////////////////////
void toOrtho()
{
    // set viewport to be the entire window
    glViewport(0, 0, (GLsizei)screenWidth, (GLsizei)screenHeight);

    // set orthographic viewing frustum
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, screenWidth, 0, screenHeight, -1, 1);

    // switch to modelview matrix in order to set scene
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}



///////////////////////////////////////////////////////////////////////////////
// set the projection matrix as perspective
///////////////////////////////////////////////////////////////////////////////
void toPerspective()
{
    // set viewport to be the entire window
    glViewport(0, 0, (GLsizei)screenWidth, (GLsizei)screenHeight);

    // set perspective viewing frustum
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (float)(screenWidth)/screenHeight, 1.0f, 1000.0f); // FOV, AspectRatio, NearClip, FarClip

    // switch to modelview matrix in order to set scene
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
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
    // get the total elapsed time
    playTime = (float)timer.getElapsedTime();

    // compute rotation angle
    const float ANGLE_SPEED = 90;   // degree/s
    float angle = ANGLE_SPEED * playTime;

    // render to texture //////////////////////////////////////////////////////
    t1.start();

    // adjust viewport and projection matrix to texture dimension
    //glViewport(0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT);
    //glMatrixMode(GL_PROJECTION);
    //glLoadIdentity();
    //gluPerspective(60.0f, (float)(TEXTURE_WIDTH)/TEXTURE_HEIGHT, 1.0f, 100.0f);
    //glMatrixMode(GL_MODELVIEW);
    GLenum err = glGetError();
    if(err){
        printf("displayCB, error after old perspective setup, %x", err);
    }

    // camera transform
    //glLoadIdentity();
    //glTranslatef(0, 0, -CAMERA_DISTANCE);

    if(glGetError()){
        printf("displayCB, error after translate setup");
    }

    
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

    // Config position vbo
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, 18 * sizeof(GLfloat), small_plane_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    if(glGetError()){
        printf("something in position buffer failed\n");
    }

    draw();

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

    // back to normal viewport and projection matrix
    glViewport(0, 0, screenWidth, screenHeight);
    //glMatrixMode(GL_PROJECTION);
    //glLoadIdentity();
    //gluPerspective(60.0f, (float)(screenWidth)/screenHeight, 1.0f, 100.0f);
    //glMatrixMode(GL_MODELVIEW);
    //glLoadIdentity();
    if(glGetError()){
        printf("displayCB, error after viewport setup");
    }

    // tramsform camera
    //glTranslatef(0, 0, -cameraDistance);
    //glRotatef(cameraAngleX, 1, 0, 0);   // pitch
    //glRotatef(cameraAngleY, 0, 1, 0);   // heading

    // clear framebuffer
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    //glPushMatrix();

    if(glGetError()){
        printf("displayCB, error before draw");
    }

    glUseProgram(tw_shader_program);
    // Config position vbo
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, 18 * sizeof(GLfloat), plane_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glUniform1f(glGetUniformLocation(tw_shader_program, "override"), 0.0);

    glBindTexture(GL_TEXTURE_2D, textureId);

    draw();

    if(glGetError()){
        printf("displayCB, error after draw");
    }

    //glPopMatrix();

    // draw info messages
    //showInfo();
    //showFPS();
    glutSwapBuffers();
}


void reshapeCB(int width, int height)
{
    screenWidth = width;
    screenHeight = height;
    toPerspective();
}


void timerCB(int millisec)
{
    glutTimerFunc(millisec, timerCB, millisec);
    glutPostRedisplay();
}


void idleCB()
{
    glutPostRedisplay();
}


void keyboardCB(unsigned char key, int x, int y)
{
    switch(key)
    {
    case 27: // ESCAPE
        exit(0);
        break;

    case ' ':
        if(fboSupported)
            fboUsed = !fboUsed;
        std::cout << "FBO mode: " << (fboUsed ? "on" : "off") << std::endl;
         break;

    case 'd': // switch rendering modes (fill -> wire -> point)
    case 'D':
        drawMode = ++drawMode % 3;
        if(drawMode == 0)        // fill mode
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
        }
        else if(drawMode == 1)  // wireframe mode
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
        }
        else                    // point mode
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
        }
        break;

    default:
        ;
    }
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
