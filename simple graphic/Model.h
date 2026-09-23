#pragma once

#ifndef MODEL_H
#define MODEL_H

#include "geometry.h"
#include "TGA_image.h"

class Model {
    std::vector<vec4> vertices={};
    std::vector<vec4> normals={};
    std::vector<vec2> texture={}; // texture coordinates
    std::vector<int> facet_vrt={};
    std::vector<int> facet_nrm={};
    std::vector<int> facet_tex={};
    TGAImage diffusemap={}; // diffuse color texture
    TGAImage normalmap={}; // normal map texture
    TGAImage specularmap={}; // specular texture
public:
    Model(const std::string& filename);
    std::size_t nverts() const noexcept; // number of vertices
    std::size_t nfaces() const noexcept; // number of triangles
    vec4 vert(const int i) const; // 0 <= i < nverts()
    vec4 vert(const int iface, const int nthvert) const; // 0 <= iface <= nfaces(), 0 <= nthvert < 3
    vec4 normal(const int iface, const int nthvert) const; // normal coming from the "vn x y z" entries in the .obj file
    vec4 normal(const vec2& uv) const; // normal vector from the normal map texture
    vec2 uv(const int iface, const int nthvert) const; // uv coordinates of triangle corners
    const TGAImage& diffuse() const noexcept;
    const TGAImage& specular() const noexcept;
};

#endif //MODEL_H
