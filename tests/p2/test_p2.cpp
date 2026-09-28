#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

using namespace std;

const string marker = "<|end_conversation|>";

void test_message() {
    Message empty;
    assert(empty.role() == Role::System && empty.content().empty());
    Message user(Role::User, "hello");
    assert(user.role() == Role::User && user.content() == "hello");
}

void test_empty_conversation() {
    Conversation c;
    assert(c.size() == 0 && c.begin() == c.end());
    bool threw = false;
    try { c.at(0); } catch (const out_of_range&) { threw = true; }
    assert(threw);
}

void test_message_order() {
    Conversation c;
    c.append(Message(Role::System, "Be concise."));
    c.append(Message(Role::User, "hello"));
    c.append(Message(Role::Assistant, "Hi!"));
    assert(c.size() == 3);
    assert(c.at(0).role() == Role::System);
    assert(c.at(1).role() == Role::User);
    assert(c.at(2).role() == Role::Assistant);
}

void test_growth() {
    Conversation c;
    for (size_t i = 0; i < 40; ++i) {
        const Message* old = c.begin();
        c.append(Message(Role::User, to_string(i)));
        // Doubling needs a new array at sizes 1, 2, 3, 5, 9, ...
        bool grew = i == 0 || i == 1 || i == 2 || i == 4 ||
                    i == 8 || i == 16 || i == 32;
        assert((c.begin() != old) == grew);
        assert(c.size() == i + 1);
        for (size_t j = 0; j <= i; ++j)
            assert(c.at(j).content() == to_string(j));
    }
}

void test_copy_constructor() {
    Conversation a;
    a.append(Message(Role::User, "first"));
    Conversation b(a);
    assert(a.begin() != b.begin());
    assert(b.at(0).content() == "first");
    b.append(Message(Role::Assistant, "second"));
    assert(a.size() == 1);
}

void test_copy_assignment() {
    Conversation a, b;
    a.append(Message(Role::User, "first"));
    b.append(Message(Role::Assistant, "old"));
    b = a;
    assert(a.begin() != b.begin());
    assert(b.at(0).content() == "first");
    Conversation& same = b;
    b = same;
    assert(b.at(0).content() == "first");
}

void test_move_constructor() {
    Conversation a;
    a.append(Message(Role::User, "hello"));
    const Message* address = a.begin();
    Conversation b(std::move(a));
    assert(b.begin() == address && b.at(0).content() == "hello");
    assert(a.size() == 0 && a.begin() == a.end());
}

void test_move_assignment() {
    Conversation a, b;
    a.append(Message(Role::User, "hello"));
    b.append(Message(Role::Assistant, "old"));
    const Message* address = a.begin();
    b = std::move(a);
    assert(b.begin() == address && b.at(0).content() == "hello");
    assert(a.size() == 0 && a.begin() == a.end());
}

void test_clean_text() {
    SentinelScanner s(marker);
    auto a = s.feed("Hello, ");
    auto b = s.feed("world!");
    auto c = s.flush();
    assert(!a.sentinel_found && !b.sentinel_found && !c.sentinel_found);
    assert(a.safe_text + b.safe_text + c.safe_text == "Hello, world!");
}

void test_whole_marker() {
    SentinelScanner s(marker);
    auto result = s.feed("Bye.<|end_conversation|>ignored");
    assert(result.sentinel_found && result.safe_text == "Bye.");
    assert(s.feed("more").safe_text.empty());
}

void test_every_split() {
    string text = "Bye." + marker;
    for (size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner s(marker);
        auto a = s.feed(string_view(text).substr(0, split));
        auto b = s.feed(string_view(text).substr(split));
        assert(a.sentinel_found || b.sentinel_found);
        assert(a.safe_text + b.safe_text == "Bye.");
    }
}

void test_one_character_chunks() {
    SentinelScanner s(marker);
    string printed;
    bool found = false;
    for (char ch : string("Bye.") + marker) {
        auto result = s.feed(string_view(&ch, 1));
        printed += result.safe_text;
        found = result.sentinel_found;
    }
    assert(found && printed == "Bye.");
}

void test_false_alarm() {
    SentinelScanner s(marker);
    auto a = s.feed("Start <|end_world|> end");
    auto b = s.flush();
    assert(!a.sentinel_found && !b.sentinel_found);
    assert(a.safe_text + b.safe_text == "Start <|end_world|> end");
}

void test_pending_limit() {
    SentinelScanner s(marker);
    // Repeatedly come close to the marker without completing it.
    const string near_match = marker.substr(0, marker.size() - 1) + "X";
    size_t printed = 0;
    for (size_t i = 0; i < 4 * 1024 * 1024; ++i) {
        const char ch = near_match[i % near_match.size()];
        auto result = s.feed(string_view(&ch, 1));
        assert(!result.sentinel_found);
        assert(s.pending_size() <= marker.size() - 1);
        printed += result.safe_text.size();
    }
    printed += s.flush().safe_text.size();
    assert(printed == 4 * 1024 * 1024);
}

void test_empty_marker() {
    bool threw = false;
    try { SentinelScanner s(""); }
    catch (const invalid_argument&) { threw = true; }
    assert(threw);
}

class TestInput : public InputSource {
public:
    TestInput(string a = "", string b = "", string c = "")
        : lines_{a, b, c}, count_(!c.empty() ? 3 : !b.empty() ? 2 : !a.empty() ? 1 : 0) {}
    string read_line() override {
        if (next_ == count_) { eof_ = true; return ""; }
        return lines_[next_++];
    }
    bool is_eof() const override { return eof_; }
private:
    string lines_[3];
    size_t count_;
    size_t next_ = 0;
    bool eof_ = false;
};

class TestOutput : public OutputSink {
public:
    void write(string_view piece) override { text += piece; }
    string text;
};

string script_path() {
    filesystem::path file(__FILE__);
    filesystem::path from_source = file.parent_path().parent_path().parent_path() /
                                   "scripts" / "greeting.script";
    if (filesystem::exists(from_source)) return from_source.string();
    if (filesystem::exists("scripts/greeting.script")) return "scripts/greeting.script";
    if (filesystem::exists("../scripts/greeting.script")) return "../scripts/greeting.script";
    throw runtime_error("Cannot find scripts/greeting.script");
}

void test_turn_limit() {
    auto model = make_unique<ScriptedModelClient>(script_path());
    HarnessConfig config;
    config.max_turns = 2;
    config.system_message = model->system_message();
    Harness h(std::move(model), config);
    TestInput input("hello", "more");
    TestOutput output;
    StopReason reason = h.run(input, output);
    assert(reason.kind == StopReason::Kind::TurnLimit);
    assert(h.conversation().size() == 5);
    assert(h.conversation().at(0).role() == Role::System);
}

void test_sentinel_halt() {
    auto model = make_unique<ScriptedModelClient>(script_path());
    HarnessConfig config;
    config.system_message = model->system_message();
    Harness h(std::move(model), config);
    TestInput input("hello", "more", "bye");
    TestOutput output;
    StopReason reason = h.run(input, output);
    assert(reason.kind == StopReason::Kind::Sentinel);
    assert(h.conversation().size() == 7);
    assert(output.text.find(marker) == string::npos);
    assert(h.conversation().at(6).content() == "Goodbye!" + marker);
}

void test_user_eof() {
    auto model = make_unique<ScriptedModelClient>(script_path());
    HarnessConfig config;
    config.system_message = model->system_message();
    Harness h(std::move(model), config);
    TestInput input;
    TestOutput output;
    StopReason reason = h.run(input, output);
    assert(reason.kind == StopReason::Kind::UserExit);
    assert(h.conversation().size() == 1);
}

void save_for_replay(const Conversation& c, const string& path) {
    ofstream file(path);
    assert(file.is_open());
    bool first = true;
    for (const Message* m = c.begin(); m != c.end(); ++m) {
        if (!first) file << "---\n";
        first = false;
        if (m->role() == Role::System) file << "role: system\n";
        if (m->role() == Role::User) file << "role: user\n";
        if (m->role() == Role::Assistant) file << "role: assistant\n";
        file << m->content() << "\n";
    }
}

void test_transcript_round_trip() {
    auto model = make_unique<ScriptedModelClient>(script_path());
    HarnessConfig config;
    config.system_message = model->system_message();
    Harness original(std::move(model), config);
    TestInput input("hello", "more", "bye");
    TestOutput output;
    StopReason first_reason = original.run(input, output);
    assert(first_reason.kind == StopReason::Kind::Sentinel);

    filesystem::path path = filesystem::temp_directory_path() / "ece309_p2_round_trip.txt";
    save_for_replay(original.conversation(), path.string());
    auto replay_model = make_unique<ReplayModelClient>(path.string());
    config.system_message = replay_model->system_message();
    Harness replay(std::move(replay_model), config);
    TestInput replay_input("hello", "more", "bye");
    TestOutput replay_output;
    StopReason second_reason = replay.run(replay_input, replay_output);
    filesystem::remove(path);

    assert(second_reason.kind == StopReason::Kind::Sentinel);
    assert(output.text == replay_output.text);
    assert(original.conversation().size() == replay.conversation().size());
    for (size_t i = 0; i < original.conversation().size(); ++i) {
        assert(original.conversation().at(i).role() == replay.conversation().at(i).role());
        assert(original.conversation().at(i).content() == replay.conversation().at(i).content());
    }
}

int main() {
    test_message();
    test_empty_conversation();
    test_message_order();
    test_growth();
    test_copy_constructor();
    test_copy_assignment();
    test_move_constructor();
    test_move_assignment();
    test_clean_text();
    test_whole_marker();
    test_every_split();
    test_one_character_chunks();
    test_false_alarm();
    test_pending_limit();
    test_empty_marker();
    test_turn_limit();
    test_sentinel_halt();
    test_user_eof();
    test_transcript_round_trip();
    cout << "All Project 2 tests passed.\n";
}
