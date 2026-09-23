#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <ranges>
using namespace std;

class Solution {
public:
    string decodeString(string s) {
        string res;
        string repeat_part;
        int n=s.size();
        for(int i=0; i<n; ){
            if(s[i]>='a'&&s[i]<='z'){
                res.push_back(s[i]);
                ++i;
            }else{
                int repeat_num=0;
                while(s[i]>='0'&&s[i]<='9'){
                    repeat_num=repeat_num*10+s[i]-'0';
                    ++i;
                }
                ++i;
                int j=i, sta=1;
                while(sta>0){
                    if(s[j]=='[') ++sta;
                    if(s[j]==']') --sta;
                    ++j;
                }
                repeat_part=decodeString(s.substr(i, j-i-1));
                while(repeat_num-->0){
                    res+=repeat_part;
                }
                repeat_part="";
                i=j;
            }
        }
        return res;
    }
};

int main() {
    string s="3[a]2[bc]";
    Solution sol;
    auto result = sol.decodeString(s);
    cout << result << endl;
}