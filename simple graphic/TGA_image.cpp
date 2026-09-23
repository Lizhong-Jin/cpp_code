#include <iostream>
#include <fstream>
#include <cstring>
#include "TGA_image.h"

TGAImage::TGAImage(const int w, const int h, const int bpp, TGAColor c): w(w), h(h), bpp(bpp), data(w*h*bpp, 0) {
    for (int j=0; j<h; j++) {
        for (int i=0; i<w; i++) {
            set(i, j, c);
        }
    }
}
bool TGAImage::read_tga_file(const std::string &filename) {
    std::ifstream in;
    in.open(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Could not open file " << filename << std::endl;
        return false;
    }
    TGAHeader header;
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in.good()) {
        std::cerr << "Could not read header from file " << filename << std::endl;
        return false;
    }
    w=header.width;
    h=header.height;
    bpp=header.bitsperpixel>>3;
    if (w<=0||h<=0||(bpp!=GRAYSCALE&&bpp!=RGB&&bpp!=RGBA)) {
        std::cerr << "Invalid tga header!" << std::endl;
        return false;
    }
    size_t nbytes=w*h*bpp;
    data=std::vector<std::uint8_t>(nbytes, 0);
    if (header.datatypecode==2||header.datatypecode==3) {
        in.read(reinterpret_cast<char*>(data.data()), nbytes);
        if (!in.good()) {
            std::cerr << "Could not read data from file " << filename << std::endl;
            return false;
        }
    }else if (header.datatypecode==10||header.datatypecode==11) {
        if (!load_rle_data(in)) {
            std::cerr << "Could not load data from file " << filename << std::endl;
            return false;
        }
    }else {
        std::cerr << "Unknown file format " << (int)header.datatypecode << std::endl;
        return false;
    }
    if (!(header.imagedescriptor)&0x20) {
        flip_vertically();
    }
    if (!(header.imagedescriptor)&0x10) {
        flip_horizontally();
    }
    std::cerr << w << "x" << h << "/" << bpp*8 << std::endl;
    return true;
}
bool TGAImage::load_rle_data(std::ifstream &in) {
    size_t pixelcount=w*h;
    size_t currentpixel=0;
    size_t currentbyte=0;
    TGAColor colorbuffer;
    do {
        std::uint8_t chunkheader=0;
        chunkheader=in.get();
        if (!in.good()) {
            std::cerr << "Could not read data from file " << std::endl;
            return false;
        }
        if (chunkheader<0x80) {
            chunkheader++;
            for (int i=0; i<chunkheader; i++) {
                in.read(reinterpret_cast<char*>(colorbuffer.bgra), bpp);
                if (!in.good()) {
                    std::cerr << "an error occured while reading the header " << std::endl;
                    return false;
                }
                for (int t=0; t<bpp; t++) {
                    data[currentbyte++]=colorbuffer.bgra[t];
                }
                currentpixel++;
                if (currentpixel>pixelcount) {
                    std::cerr << "too many pixels read from file " << std::endl;
                    return false;
                }
            }
        }else {
            chunkheader-=127;
            in.read(reinterpret_cast<char*>(colorbuffer.bgra), bpp);
            if (!in.good()) {
                std::cerr << "an error occured while reading the header " << std::endl;
                return false;
            }
            for (int i=0; i<chunkheader; i++) {
                for (int t=0; t<bpp; t++) {
                    data[currentbyte++]=colorbuffer.bgra[t];
                }
                currentpixel++;
                if (currentpixel>pixelcount) {
                    std::cerr << "too many pixels read from file " << std::endl;
                    return false;
                }
            }
        }
    }while (currentpixel<pixelcount);
    return true;
}
bool TGAImage::write_tga_file(const std::string &filename, const bool vflip, const bool rle) const {
    constexpr std::uint8_t developer_area_ref[4]={0,0,0,0};
    constexpr std::uint8_t extension_area_ref[4]={0,0,0,0};
    constexpr std::uint8_t footer[18]={'T','R','U','E','V','I','S','I','O','N','-','X','F','I','L','E','.','\0'};
    std::ofstream out;
    out.open(filename, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Could not open file " << filename << std::endl;
        return false;
    }
    TGAHeader header;
    header.bitsperpixel=bpp<<3;
    header.width=w;
    header.height=h;
    header.datatypecode=(bpp==GRAYSCALE?(rle?11:3):(rle?10:2));
    header.imagedescriptor=vflip?0x00:0x20; // top-left or bottom-left origin
    header.imagedescriptor|=(bpp==4?8:0);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!out.good()) goto err;
    if (!rle) {
        out.write(reinterpret_cast<const char *>(data.data()), w*h*bpp);
        if (!out.good()) goto err;
    }else if (!unload_rle_data(out)) goto err;
    out.write(reinterpret_cast<const char *>(developer_area_ref), sizeof(developer_area_ref));
    if (!out.good()) goto err;
    out.write(reinterpret_cast<const char *>(extension_area_ref), sizeof(extension_area_ref));
    if (!out.good()) goto err;
    out.write(reinterpret_cast<const char *>(footer), sizeof(footer));
    if (!out.good()) goto err;
    return true;

    err:
    std::cerr << "Could not write TGA file " << filename << std::endl;
    return false;
}
bool TGAImage::unload_rle_data(std::ofstream &out) const {
    const std::uint8_t max_chunk_length=128;
    size_t npixels=w*h;
    size_t currentpixel=0;
    while (currentpixel<npixels) {
        size_t chunkstart=currentpixel*bpp;
        size_t curbyte=currentpixel*bpp;
        std::uint8_t run_lenght=1;
        bool raw=true;
        while (currentpixel+run_lenght<npixels && run_lenght<max_chunk_length) {
            bool succ_eq=true;
            for (int t=0; succ_eq&&t<bpp; t++) {
                succ_eq=(data[curbyte+t]==data[curbyte+bpp+t]);
            }
            curbyte+=bpp;
            if (run_lenght==1) {
                raw=!succ_eq;
            }
            if (raw && succ_eq) {
                run_lenght--;
                break;
            }
            if (!raw && !succ_eq) {
                break;
            }
            run_lenght++;
        }
        currentpixel+=run_lenght;
        out.put(raw?run_lenght-1:run_lenght+127);
        if (!out.good()) return false;
        out.write(reinterpret_cast<const char *>(data.data()+chunkstart),(raw?run_lenght*bpp:bpp));
        if (!out.good()) return false;
    }
    return true;
}
TGAColor TGAImage::get(const int x, const int y) const {
    if (!data.size()||x<0||y<0||x>=w||y>=h) return {};
    TGAColor ret={0,0,0,0,bpp};
    const std::uint8_t* p=data.data()+(x+y*w)*bpp;
    for (int i=bpp; i--; ret.bgra[i]=p[i]);
    return ret;
}
void TGAImage::set(int x, int y, const TGAColor &color) {
    if (!data.size()||x<0||y<0||x>=w||y>=h) return;
    memcpy(data.data()+(x+y*w)*bpp, color.bgra, bpp);
}
void TGAImage::flip_vertically() {
    for (int i=0; i<w; i++) {
        for (int j=0; j<(h>>1); j++) {
            for (int b=0; b<bpp; b++) {
                std::swap(data[(i+j*w)*bpp+b], data[(i+(h-1-j)*w)*bpp+b]);
            }
        }
    }
}
void TGAImage::flip_horizontally() {
    for (int i=0; i<(w>>1); i++) {
        for (int j=0; j<h; j++) {
            for (int b=0; b<bpp; b++) {
                std::swap(data[(i+j*w)*bpp+b], data[(w-1-i+j*w)*bpp+b]);
            }
        }
    }
}
int TGAImage::height() const {return h;}
int TGAImage::width() const {return w;}