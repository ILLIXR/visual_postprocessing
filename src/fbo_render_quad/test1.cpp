#include <GL/glut.h>
#include <GL/gl.h>
#include <stdio.h>
#include "../image.h"

// Good reference:
// https://stackoverflow.com/questions/24262264/drawing-a-2d-texture-in-opengl

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [image]\n", argv[0]);
        exit(0);
    }

    //create GL context
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Render to FBO Demo");

    Image new_image(argv[1]);

    //create test checker image
    unsigned char texDat[64];
    for (int i = 0; i < 64; ++i)
        texDat[i] = ((i + (i / 8)) % 2) * 128 + 127;

    //upload to GPU texture
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, new_image.width, new_image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, new_image.texture);
    glBindTexture(GL_TEXTURE_2D, 0);

    //match projection to window resolution (could be in reshape callback)
    glMatrixMode(GL_PROJECTION);
    glOrtho(0, 800, 0, 600, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    //clear and draw quad with texture (could be in display callback)
    glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2i(0, 0); glVertex2i(0, 0);
    glTexCoord2i(0, 1); glVertex2i(0, 800);
    glTexCoord2i(1, 1); glVertex2i(800, 800);
    glTexCoord2i(1, 0); glVertex2i(800, 0);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFlush(); //don't need this with GLUT_DOUBLE and glutSwapBuffers

    getchar(); //pause so you can see what just happened
    //System("pause"); //I think this works on windows

    return 0;
}