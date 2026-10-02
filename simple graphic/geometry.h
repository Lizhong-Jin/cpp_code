#pragma once

#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <cmath>
#include <cassert>
#include <iostream>

// vector
template<int n> struct vec {
    double data[n]={0};
    double& operator[](const int i) {
        assert(i>=0 && i<n);
        return data[i];
    }
    double operator[](const int i) const {
        assert(i>=0 && i<n);
        return data[i];
    }
};
// vector dot product
template<int n> double operator*(const vec<n>& lhs, const vec<n>& rhs) {
    double sum=0;
    for(int i=0; i<n; i++) sum+=lhs[i]*rhs[i];
    return sum;
}
// vector addition
template<int n> vec<n> operator+(const vec<n>& lhs, const vec<n>& rhs) {
    vec<n> res=lhs;
    for(int i=0; i<n; i++) res[i]+=rhs[i];
    return res;
}
// vector subtraction
template<int n> vec<n> operator-(const vec<n>& lhs, const vec<n>& rhs) {
    vec<n> res=lhs;
    for(int i=0; i<n; i++) res[i]-=rhs[i];
    return res;
}
// vector scalar product
template<int n> vec<n> operator*(const vec<n>& v, const double s) {
    vec<n> res=v;
    for(int i=0; i<n; i++) res[i]*=s;
    return res;
}
template<int n> vec<n> operator*(const double s, const vec<n>& v) {
    vec<n> res=v;
    for(int i=0; i<n; i++) res[i]*=s;
    return res;
}
// vector scalar division
template<int n> vec<n> operator/(const vec<n>& v, const double s) {
    vec<n> res=v;
    for(int i=0; i<n; i++) res[i]/=s;
    return res;
}
// output vector using stream <<
template<int n> std::ostream& operator<<(std::ostream& out, const vec<n>& v) {
    for(int i=0; i<n; i++) out << v[i] << " ";
    return out;
}

template<> struct vec<2> {
    double x, y;
    vec(double _x=0, double _y=0): x(_x), y(_y) {}
    double& operator[](const int i) {
        assert(i>=0 && i<2);
        return i == 0 ? x : y;
    }
    double operator[](const int i) const {
        assert(i>=0 && i<2);
        return i == 0 ? x : y;
    }
};
template<> struct vec<3> {
    double x, y, z;
    vec(double _x=0, double _y=0, double _z=0): x(_x), y(_y), z(_z) {}
    double& operator[](const int i) {
        assert(i>=0 && i<3);
        return i == 0 ? x : (i == 1 ? y : z);
    }
    double operator[](const int i) const {
        assert(i>=0 && i<3);
        return i == 0 ? x : (i == 1 ? y : z);
    }
};
template<> struct vec<4> {
    double x, y, z, w;
    vec(double _x=0, double _y=0, double _z=0, double _w=0): x(_x), y(_y), z(_z), w(_w) {}
    double& operator[](const int i) {
        assert(i>=0 && i<4);
        return i == 0 ? x : (i == 1 ? y : (i == 2 ? z : w));
    }
    double operator[](const int i) const {
        assert(i>=0 && i<4);
        return i == 0 ? x : (i == 1 ? y : (i == 2 ? z : w));
    }
    vec<2> xy() const {return {x,y};};
    vec<3> xyz() const {return {x,y,z};}
};
typedef vec<2> vec2;
typedef vec<3> vec3;
typedef vec<4> vec4;

// calculate normalization parameter
template<int n> double norm(const vec<n>& v) {
    return std::sqrt(v*v);
}
// normalization
template<int n> vec<n> normalized(const vec<n>& v) {
    return v/norm(v);
}
// 3-vector cross product
inline vec3 cross(const vec3& v1, const vec3& v2) {
    return {v1.y*v2.z-v1.z*v2.y, v1.z*v2.x-v1.x*v2.z, v1.x*v2.y-v1.y*v2.x};
}
// declaration
template<int n> struct dt;

// matrix
template<int nrows, int ncols> struct mat {
    vec<ncols> rows[nrows]={{}};
    vec<ncols>& operator[](const int i) {
        assert(i>=0 && i<nrows);
        return rows[i];
    }
    const vec<ncols>& operator[](const int i) const {
        assert(i>=0 && i<nrows);
        return rows[i];
    }
    // calculate determinant
    double det() const {
        return dt<ncols>::det(*this);
    }
    // calculate cofactor
    double cofactor(const int row, const int col) const {
        mat<nrows-1,ncols-1> submatrix;
        for (int i=0; i<nrows; i++) {
            for (int j=0; j<ncols; j++) {
                submatrix[i][j]=rows[i+int(i>=row)][j+int(j>=col)];
            }
        }
        return submatrix.det()*((row+col)%2?-1:1);
    }
    // calculate inverse matrix
    mat<nrows, ncols> invert_transpose() const {
        mat<nrows, ncols> adjugate_transpose;
        for (int i=0; i<nrows; i++) {
            for (int j=0; j<ncols; j++) {
                adjugate_transpose[i][j]=cofactor(i,j);
            }
        }
        return adjugate_transpose/(adjugate_transpose[0]*rows[0]);
    }
    mat<nrows, ncols> invert() const {
        return invert_transpose().transpose();
    }
    // calculate transposed matrix
    mat<ncols, nrows> transpose() const {
        mat<ncols, nrows> result;
        for (int i=0; i<nrows; i++) {
            for (int j=0; j<ncols; j++) {
                result[i][j]=rows[j][i];
            }
        }
        return result;
    }
};
// Multiplication of vectors and matrices
template<int nrows, int ncols> vec<ncols> operator*(const vec<nrows>& lhs, const mat<nrows, ncols>& rhs) {
    return (mat<1, nrows>{{lhs}}*rhs)[0];
}
// Multiplication of matrices and vectors
template<int nrows, int ncols> vec<nrows> operator*(const mat<nrows, ncols>& lhs, const vec<ncols>& rhs) {
    vec<nrows> result;
    for (int i=0; i<nrows; i++) {
        result[i]=lhs[i]*rhs;
    }
    return result;
}
// matrix multiplication
template<int R1, int C1, int C2> mat<R1, C2> operator*(const mat<R1, C1>& lhs, const mat<C1, C2>& rhs) {
    mat<R1, C2> result;
    for (int i=0; i<R1; i++) {
        for (int j=0; j<C2; j++) {
            for (int k=0; k<C1; k++) {
                result[i][j]+=lhs[i][k]*rhs[k][j];
            }
        }
    }
    return result;
}
// matrix scalar product
template<int nrows, int ncols> mat<nrows, ncols> operator*(const mat<nrows, ncols>& m, const double s) {
    mat<nrows, ncols> result;
    for (int i=0; i<nrows; i++) {
        result[i]=m[i]*s;
    }
    return result;
}
template<int nrows, int ncols> mat<nrows, ncols> operator*(const double s, const mat<nrows, ncols>& m) {
    mat<nrows, ncols> result;
    for (int i=0; i<nrows; i++) {
        result[i]=m[i]*s;
    }
    return result;
}
// matrix scalar division
template<int nrows, int ncols> mat<nrows, ncols> operator/(const mat<nrows, ncols>& m, const double s) {
    mat<nrows, ncols> result;
    for (int i=0; i<nrows; i++) {
        result[i]=m[i]/s;
    }
    return result;
}
// matrix addition
template<int nrows, int ncols> mat<nrows, ncols> operator+(const mat<nrows, ncols>& m1, const mat<nrows, ncols>& m2) {
    mat<nrows, ncols> result;
    for (int i=0; i<nrows; i++) {
        result[i]=m1[i]+m2[i];
    }
    return result;
}
// matrix substraction
template<int nrows, int ncols> mat<nrows, ncols> operator-(const mat<nrows, ncols>& m1, const mat<nrows, ncols>& m2) {
    mat<nrows, ncols> result;
    for (int i=0; i<nrows; i++) {
        result[i]=m1[i]-m2[i];
    }
    return result;
}
// output matrix using stream <<
template<int nrows, int ncols> std::ostream& operator<<(std::ostream& out, const mat<nrows, ncols>& m) {
    for (int i=0; i<nrows; i++) out << m[i] << std::endl;
    return out;
}
// calculate determinant recursively
template<int n> struct dt {
    static double det(const mat<n, n>& m) {
        double result = 0;
        for (int i=0; i<n; i++) result += m[0][i]*m.cofactor(0, i);
        return result;
    }
};
template<> struct dt<1> {
    static double det(const mat<1, 1>& m) {
        return m[0][0];
    }
};


#endif //GEOMETRY_H
