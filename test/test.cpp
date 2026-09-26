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
    vector<string> braceExpansionII(string expression) {
        vector<string> temp, res;
        vector<string> new_temp;
        int n=expression.size();

        auto update_res=[&]()->void{
            if(temp.empty()) return;
            if(res.empty()){
                res.move(temp);
                return;
            }
            for(auto& s: temp){
                if(find(res.begin(), res.end(), s)==res.end()){
                    res.emplace_back(s);
                }
            }
        };

        for(int i=0, j=0; i<n; ){
            char c=expression[i];
            if(c>='a'&&c<='z'){
                while (j<n&&expression[j]>='a'&&expression[j]<='z') ++j;
                if(temp.empty()) temp.emplace_back(expression.substr(i, j-i));
                else{
                    string s = expression.substr(i, j-i);
                    for(auto& r_s: res){
                        r_s += s;
                    }
                }
                i=j;
            }else if(c=='{'){
                int sta=1;
                ++j;
                while(sta>0){
                    if(expression[j]=='{') ++sta;
                    else if(expression[j]=='}') --sta;
                    ++j;
                }
                vector<string> sub_strs=braceExpansionII(expression.substr(i+1, j-i-2));
                if(temp.empty()) temp=move(sub_strs);
            }else{
                new_temp.clear();
                for(auto& s: temp){
                    for(auto& s_s: sub_strs){
                        new_temp.emplace_back(s+s_s);
                    }
                }
                temp=move(new_temp);
            }
            i=j;
        }else if(c==','){
            update_res();
            temp.clear()
            ++i; ++j;
        }
    }
    update_res();
    return res;
}
};

int main() {
    string s="{{a,z},a{b,c},{ab,z}}";
    Solution sol;
    auto result = sol.braceExpansionII(s);
    for (auto& r: result) {
        cout << r << endl;
    }
}