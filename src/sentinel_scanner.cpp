#include "core/sentinel_scanner.h"

#include <stdexcept>
#include <utility>

using namespace std;

SentinelScanner::SentinelScanner(string sentinel)
    : sentinel_(std::move(sentinel)) {
    if (sentinel_.empty()) {
        throw invalid_argument("Sentinel cannot be empty");
    }
}

SentinelScanner::Out SentinelScanner::feed(string_view chunk) {
    if (found_) return {"", true};

    // Add the new chunk to the short piece left from the last call.
    string text = pending_ + string(chunk);
    size_t match = text.find(sentinel_);

    if (match != string::npos) {
        found_ = true;
        pending_.clear();
        return {text.substr(0, match), true};
    }

    // Keep only the end of the text, where a split sentinel might start.
    size_t keep = sentinel_.size() - 1;
    if (keep > text.size()) keep = text.size();

    string safe_text = text.substr(0, text.size() - keep);
    pending_ = text.substr(text.size() - keep);
    return {safe_text, false};
}

SentinelScanner::Out SentinelScanner::flush() {
    if (found_) return {"", true};

    string safe_text = pending_;
    pending_.clear();
    return {safe_text, false};
}
