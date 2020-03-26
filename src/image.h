// Libpng wrapper

// A class representing a given GLubyte array representing a PNG, loaded from a file
class Image {
public:
    int width, height;
    bool hasAlpha;
    GLubyte* texture;
    const char* filename;

    Image();
    Image(const char* filename);
    ~Image();
};

// Load a PNG at filename into a GLubyte array
bool load_png (const char* filename, int& outWidth, int& outHeight, GLubyte **outData);