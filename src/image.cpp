// Libpng wrapper
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <string.h> // for memcpy
#include "image.h"

// Reference:
// https://gist.github.com/mortennobel/5299151

/* load_png
 *
 * Loads a PNG image into a newly allocated GLubyte array
 *
 * Returns true on success, false on failure
 *
 * Sets outWidth and outHeight to image dimensions
 * Allocates a GLubyte array, and sets outData to point to the newly created array
 */
bool load_png (const char* filename, int& outWidth, int& outHeight, bool &outHasAlpha, GLubyte **outData) {
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned int sig_read = 0;
    int color_type, interlace_type;
    FILE *fp;
    
    if ((fp = fopen(filename, "rb")) == NULL) {
        fprintf (stderr, "(File not found) Failed to open file %s\n", filename);
        return false;
    }
    
    /* Create and initialize the png_struct
     * with the desired error handler
     * functions.  If you want to use the
     * default stderr and longjump method,
     * you can supply NULL for the last
     * three parameters.  We also supply the
     * the compiler header file version, so
     * that we know if the application
     * was compiled with a compatible version
     * of the library.  REQUIRED
     */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                     NULL, NULL, NULL);
    
    if (png_ptr == NULL) {
        fclose(fp);
        return false;
    }
    
    /* Allocate/initialize the memory
     * for image information.  REQUIRED. */
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fclose(fp);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return false;
    }
    
    /* Set error handling if you are
     * using the setjmp/longjmp method
     * (this is the normal method of
     * doing things with libpng).
     * REQUIRED unless you  set up
     * your own error handlers in
     * the png_create_read_struct()
     * earlier.
     */
    if (setjmp(png_jmpbuf(png_ptr))) {
        /* Free all of the memory associated
         * with the png_ptr and info_ptr */
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        /* If we get here, we had a
         * problem reading the file */
        return false;
    }
    
    /* Set up the output control if
     * you are using standard C streams */
    png_init_io(png_ptr, fp);
    
    /* If we have already
     * read some of the signature */
    png_set_sig_bytes(png_ptr, sig_read);
    
    /*
     * If you have enough memory to read
     * in the entire image at once, and
     * you need to specify only
     * transforms that can be controlled
     * with one of the PNG_TRANSFORM_*
     * bits (this presently excludes
     * dithering, filling, setting
     * background, and doing gamma
     * adjustment), then you can read the
     * entire image (including pixels)
     * into the info structure with this
     * call
     *
     * PNG_TRANSFORM_STRIP_16 |
     * PNG_TRANSFORM_PACKING  forces 8 bit
     * PNG_TRANSFORM_EXPAND forces to
     *  expand a palette into RGB
     */
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);
    
    png_uint_32 width, height;
    int bit_depth;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 &interlace_type, NULL, NULL);
    outWidth = width;
    outHeight = height;
    // Whether this is color or grayscale, if alpha channel exists, return true here:
    outHasAlpha = color_type & (PNG_COLOR_MASK_ALPHA);

    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    *outData = (unsigned char*) malloc(row_bytes * outHeight);
    
    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
    
    for (int i = 0; i < outHeight; i++) {
        // note that png is ordered top to
        // bottom, but OpenGL expect it bottom to top
        // so the order or swapped
        memcpy(*outData+(row_bytes * (outHeight-1-i)), row_pointers[i], row_bytes);
    }
    
    /* Clean up after the read,
     * and free any memory allocated */
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    
    /* Close the file */
    fclose(fp);
    
    /* That's it */
    return true;
}

Image::Image() {
    // Start out with empty values
    this->filename = NULL;
    this->width = 0;
    this->height = 0;
    this->hasAlpha = false;
    this->texture = NULL;
}

Image::Image(const char* filename) : filename(filename) {
    this->width = 0;
    this->height = 0;
    this->texture = NULL;
    this->hasAlpha = false;

    if (!load_png(filename, this->width, this->height, this->hasAlpha, &(this->texture))) {
        // An error occurred, try to free the buffer if it was allocated,
        // and set the image to be uninitialized
        free(this->texture);
        this->texture = NULL;
        this->width = 0;
        this->height = 0;
        this->hasAlpha = false;

        fprintf(stderr, "Error loading file %s\n", filename);
    }
}

Image::~Image() {
    // Try to free the allocated memory
    if (texture != NULL)
        free(texture);
    // Set the image parameters such that any code using this object should become a NOP
    this->texture = NULL;
    this->width = 0;
    this->height = 0;
    this->hasAlpha = false;
}