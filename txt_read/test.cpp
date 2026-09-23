#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

void to_lower(string& s) {
    transform(s.begin(), s.end(), s.begin(), (int(*)(int))tolower);
    if (!isalnum(s[s.length()-1])) s.pop_back();
}

void count_words(string& filename, unordered_map<string, int>& word_counts) {
    ifstream file;
    file.open(filename);
    if (!file.is_open()) {
        cerr << "Could not open file " << filename << endl;
    }else {
        cout<<"File opened"<<endl;
        string line, word;
        while (getline(file, line)) {
            stringstream ss(line);
            while (ss>>word) {
                to_lower(word);
                word_counts[word]++;
            }
        }
        file.close();
    }
}

int main() {
    string filename = "Bible.txt";
    unordered_map<string, int> word_counts;
    vector<pair<string, int>> words;
    count_words(filename, word_counts);
    for (auto it = word_counts.begin(); it != word_counts.end(); it++) {
        words.push_back(make_pair(it->first, it->second));
    }
    sort(words.begin(), words.end(), [](const pair<string, int>& p1, const pair<string, int>& p2) {
        return p1.second > p2.second;
    });
    int words_num = words.size();
    int the_first_k=10;
    int actual_k=min(words_num, the_first_k);
    cout << "Total word count in " << filename <<": " << words_num << endl;
    cout <<"Top " << actual_k << " most frequent words:" << endl;
    for (int i = 0; i < 10&&i<words.size(); i++) {
        cout << words[i].first << " " << words[i].second << endl;
    }
}