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

    // Lets tests check the required limit on saved characters.
    size_t pending_size() const noexcept { return pending_.size(); }

private:
    string sentinel_;
    string pending_;
    bool found_ = false;
};
