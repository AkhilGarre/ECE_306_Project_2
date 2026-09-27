#pragma once

#include <string>
#include <string_view>

using namespace std;

class SentinelScanner {
public:
    explicit SentinelScanner(string sentinel);

    struct Out {
        string safe_text;
        bool sentinel_found;
    };

    Out feed(string_view chunk);
    Out flush();

private:
    string sentinel_;
    string pending_;
    bool found_ = false;
};
