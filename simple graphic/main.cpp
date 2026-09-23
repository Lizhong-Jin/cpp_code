#include <iostream>
#include "TGA_image.h"

int main() {
    int width=128, height=128;
    TGAColor white={255,255,255,255,3};
    TGAColor black={0,0,0,255,3};
    TGAColor red={0,0,255,255,3};
    TGAImage my_image(width, height, 4, white);
    for (int i=0; i<width&&i<height; i++) {
        my_image.set(i, i, red);
    }
    if (!my_image.write_tga_file("../Image_save/my_image.tga")) {
        std::cerr << "Error writing my_image.tga" << std::endl;
        return 1;
    }
    std::cout << "writing my_image.tga" << std::endl;
    return 0;
}