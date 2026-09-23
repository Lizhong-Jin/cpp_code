#include <iostream>

#include "matrix_test.h"
#include "RedBlack_Tree_test.h"
#include "sort_test.h"

using namespace std;

int main() {
    static constexpr int N = 10;
    auto mat=generate_random_matrix(N, N, 0, 5*N);
    for (int i=0; i<N; i++) {
        for (int j=0; j<N; j++) {
            cout << mat[i][j] << " ";
        }
        cout << endl;
    }
}