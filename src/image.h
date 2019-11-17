// Libpng wrapper

// An Image representing 
class Image {
public:
    int width, height;
    GLubyte* texture;
    char* filename;

    ~Image();
};

// Load a PNG at filename into a GLubyte array
bool load_png (char* filename, int& outWidth, int& outHeight, GLubyte **outData);