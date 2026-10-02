#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <ranges>
#include <stdexcept>
using namespace std;

class Solution {
public:
    vector<string> braceExpansionII(string expression) {
        vector<string> temp, res;
        int n=expression.size();

        auto update_res=[&]()->void{
            if(temp.empty()) return;
            if(res.empty()){
                res = std::move(temp);
                return;
            }
            for(auto& s: temp){
                if(find(res.begin(), res.end(), s)==res.end()){
                    res.emplace_back(s);
                }
            }
        };

        for(int i=0; i<n; ){
            char c=expression[i];
            if(c==','){
                update_res();
                temp.clear();
                ++i;
                continue;
            }

            vector<string> sub_strs;
            if(c>='a'&&c<='z'){
                int j=i;
                while (j<n&&expression[j]>='a'&&expression[j]<='z') ++j;
                sub_strs.emplace_back(expression.substr(i, j-i));
                i=j;
            }else if(c=='{'){
                int sta=1;
                int j=i+1;
                while(j<n && sta>0){
                    if(expression[j]=='{') ++sta;
                    else if(expression[j]=='}') --sta;
                    ++j;
                }
                if(sta!=0) throw invalid_argument("unmatched brace");
                sub_strs=braceExpansionII(expression.substr(i+1, j-i-2));
                i=j;
            }else{
                throw invalid_argument("unexpected character in brace expression");
            }

            if(temp.empty()) temp=std::move(sub_strs);
            else{
                vector<string> new_temp;
                for(auto& s: temp){
                    for(auto& s_s: sub_strs){
                        new_temp.emplace_back(s+s_s);
                    }
                }
                temp=std::move(new_temp);
            }
        }
        update_res();
        sort(res.begin(), res.end());
        res.erase(unique(res.begin(), res.end()), res.end());
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
    return 0;
}
