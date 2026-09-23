#include <iostream>

using namespace std;



int main() {
    int n=31;
    int p=31-__builtin_clz(n);
    cout<<p<<endl;
}