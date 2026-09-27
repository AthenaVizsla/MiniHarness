// tests/p2/test_p2.cpp
//
// YOUR test suite goes here. At least 12 assert-based test cases — see
// spec §5 for the required categories and the sample test for the
// expected level of rigor.
//
// This file is a stub so the project builds out of the box; replace the
// body of main() with your own tests.
#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

static int tests_run = 0;
#define CHECK(cond) do { ++tests_run; assert(cond); \
    std::printf("ok %2d - %s\n", tests_run, #cond); } while (0)

namespace {

class VectorInput : public InputSource {
public:
    explicit VectorInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    std::string read_line() override {
        if (idx_ < lines_.size()) {
            eof_ = false;
            return lines_[idx_++];
        }
        eof_ = true;
        return "";
    }

    bool is_eof() const override { return eof_; }

private:
    std::vector<std::string> lines_;
    std::size_t idx_ = 0;
    bool eof_ = false;
};

class StringOutput : public OutputSink {
public:
    void write(std::string_view text) override { buffer_ += text; }
    const std::string& buffer() const { return buffer_; }

private:
    std::string buffer_;
};

void write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

}  

// 1. Empty Conversation Bounds

static void test_empty_conversation_bounds() {
    Conversation c;
    CHECK(c.size() == 0);
    CHECK(c.begin() == c.end());   // empty range: nothing to iterate over
    CHECK(c.data() == nullptr);    // no allocation has happened yet

    c.append(Message(Role::User, "hello"));
    c.append(Message(Role::Assistant, "Hi!"));
    CHECK(c.size() == 2);
    CHECK(c.at(1).content() == "Hi!");
}


// 2. System Message Ordering
static void test_system_message_pinned_at_front() {
    const std::string script_path = "/tmp/test_p2_system.script";
    write_file(script_path,
        "role: system\n"
        "Be concise.\n"
        "---\n"
        "role: assistant\n"
        "Hi there\n"
        "---\n"
        "role: assistant\n"
        "Ok then\n");

    auto model = std::make_unique<ScriptedModelClient>(script_path);

    HarnessConfig cfg;
    cfg.max_turns = 20;
    cfg.system_message = model->system_message();
    CHECK(cfg.system_message == "Be concise.");

    Harness h(std::move(model), cfg);
    VectorInput in({"hi", "again"});
    StringOutput out;
    h.run(in, out);  // stop reason doesn't matter for this test

    const Conversation& conv = h.conversation();
    CHECK(conv.size() >= 1);
    CHECK(conv.at(0).role() == Role::System);
    CHECK(conv.at(0).content() == "Be concise.");
    // Still pinned at the front after several more turns were appended.
    CHECK(conv.size() > 1);
}
// 3. Rule of Five (Copy)
static void test_copy_is_deep() {
    Conversation a;
    a.append(Message(Role::User, "x"));
    Conversation b = a;             // copy ctor
    CHECK(b.data() != a.data());    // different buffer addresses
    b.append(Message(Role::User, "y"));
    CHECK(a.size() == 1);           // original unaffected
    CHECK(b.size() == 2);

    Conversation c;
    c.append(Message(Role::Assistant, "z"));
    c = c;                           // self-assignment must not corrupt state
    CHECK(c.size() == 1);
    CHECK(c.at(0).content() == "z");

    Conversation d;
    d = a;                           // assignment into an existing object
    CHECK(d.data() != a.data());
    CHECK(d.size() == a.size());
}

// 4. Rule of Five (Move)

static void test_move_steals() {
    Conversation a;
    a.append(Message(Role::User, "x"));
    const Message* original = a.data();

    Conversation b = std::move(a);   // move ctor
    CHECK(b.data() == original);     // pointer stolen, not reallocated
    CHECK(a.data() == nullptr);      // source zeroed
    CHECK(a.size() == 0);

    Conversation c;
    c.append(Message(Role::Assistant, "y"));
    c.append(Message(Role::Assistant, "z"));
    const Message* c_ptr = c.data();
    Conversation d;
    d = std::move(c);                // move-assignment
    CHECK(d.data() == c_ptr);
    CHECK(d.size() == 2);
    CHECK(c.data() == nullptr);
    CHECK(c.size() == 0);
}

// 5. Scanner (Clean Text)
static void test_scanner_clean() {
    SentinelScanner s("<|end_conversation|>");
    auto out = s.feed("Hello there!");
    CHECK(!out.sentinel_found);
    CHECK(out.safe_text == "Hello there!");
}


// 6. Scanner (Split Sentinel) — every possible split point
static void test_scanner_split_every_boundary() {
    const std::string reply = "Goodbye.<|end_conversation|>";
    for (std::size_t split = 1; split < reply.size() - 1; ++split) {
        SentinelScanner s("<|end_conversation|>");
        auto o1 = s.feed(reply.substr(0, split));
        auto o2 = s.feed(reply.substr(split));
        CHECK(o2.sentinel_found);
        CHECK(o1.safe_text + o2.safe_text == "Goodbye.");
    }
}

// 7. Scanner (False Alarms)

static void test_scanner_false_alarm() {
    SentinelScanner s("<|end_conversation|>");
    auto out = s.feed("bye <|end_world|> ok");
    CHECK(!out.sentinel_found);
    CHECK(out.safe_text == "bye <|end_world|> ok");
}

// 8. Harness (Turn Limit)
static void test_harness_turn_limit() {
    const std::string script_path = "/tmp/test_p2_turnlimit.script";
    write_file(script_path,
        "role: assistant\n"
        "no sentinel here 1\n"
        "---\n"
        "role: assistant\n"
        "no sentinel here 2\n"
        "---\n"
        "role: assistant\n"
        "no sentinel here 3\n");

    HarnessConfig cfg;
    cfg.max_turns = 2;
    auto model = std::make_unique<ScriptedModelClient>(script_path);
    Harness h(std::move(model), cfg);

    VectorInput in({"a", "b", "c"});
    StringOutput out;
    StopReason reason = h.run(in, out);

    CHECK(reason.kind == StopReason::Kind::TurnLimit);
    CHECK(h.conversation().size() == 4);  // 2 turns x (user + assistant)
}

// 9. Harness (EOF)
static void test_harness_eof_mid_conversation() {
    const std::string script_path = "/tmp/test_p2_eof.script";
    write_file(script_path,
        "role: assistant\n"
        "Hi there\n");

    HarnessConfig cfg;
    cfg.max_turns = 20;
    auto model = std::make_unique<ScriptedModelClient>(script_path);
    Harness h(std::move(model), cfg);

    VectorInput in({"hello"});  // one real turn, then Ctrl-D
    StringOutput out;
    StopReason reason = h.run(in, out);

    CHECK(reason.kind == StopReason::Kind::UserExit);
    CHECK(h.conversation().size() == 2);  // the one completed turn was kept
}

// 10. Harness (Sentinel Halt)

static void test_harness_sentinel_halt() {
    const std::string script_path = "/tmp/test_p2_sentinel.script";
    write_file(script_path,
        "chunk: 5\n"
        "role: assistant\n"
        "Goodbye.<|end_conversation|>\n");

    HarnessConfig cfg;
    cfg.max_turns = 20;
    auto model = std::make_unique<ScriptedModelClient>(script_path);
    Harness h(std::move(model), cfg);

    VectorInput in({"bye"});
    StringOutput out;
    StopReason reason = h.run(in, out);

    CHECK(reason.kind == StopReason::Kind::Sentinel);
    CHECK(h.conversation().size() == 2);
    CHECK(h.conversation().at(1).content() == "Goodbye.<|end_conversation|>");
    // The sentinel must never leak to the user-visible output stream.
    CHECK(out.buffer().find("<|end_conversation|>") == std::string::npos);
}

// 11. Clean Destruction

static void test_harness_clean_destruction() {
    const std::string script_path = "/tmp/test_p2_destruct.script";
    write_file(script_path,
        "role: assistant\n"
        "ok1\n"
        "---\n"
        "role: assistant\n"
        "ok2\n");


    bool threw = false;
    try {
        for (int i = 0; i < 5; ++i) {
            HarnessConfig cfg;
            cfg.max_turns = 1;
            auto model = std::make_unique<ScriptedModelClient>(script_path);
            Harness h(std::move(model), cfg);
            VectorInput in({"hi"});
            StringOutput out;
            h.run(in, out);
            // h, in, out all destruct here at the end of each iteration.
        }
    } catch (...) {
        threw = true;
    }
    CHECK(!threw);
}
// 12. Transcript Round-Trip

static void test_transcript_round_trip() {
    Conversation conv;
    conv.append(Message(Role::System, "Be concise."));
    conv.append(Message(Role::User, "hello"));
    conv.append(Message(Role::Assistant, "Hi! What can I do for you today?"));
    conv.append(Message(Role::User, "goodbye"));
    conv.append(Message(Role::Assistant, "Goodbye.<|end_conversation|>"));

    const std::string path = "/tmp/test_p2_roundtrip.txt";
    {
        std::ofstream file(path);
        auto role_name = [](Role r) -> const char* {
            switch (r) {
                case Role::System:    return "system";
                case Role::User:      return "user";
                case Role::Assistant: return "assistant";
            }
            return "assistant";
        };
        bool first = true;
        for (const Message* m = conv.begin(); m != conv.end(); ++m) {
            if (!first) file << "---\n";
            first = false;
            file << "role: " << role_name(m->role()) << "\n";
            file << m->content() << "\n";
        }
    }

    ReplayModelClient replay(path);
    CHECK(replay.system_message() == "Be concise.");

    Conversation dummy;  
    Message first_reply = replay.generate(dummy);
    CHECK(first_reply.content() == "Hi! What can I do for you today?");

    Message second_reply = replay.generate(dummy);
    CHECK(second_reply.content() == "Goodbye.<|end_conversation|>");
}

int main() {
    test_empty_conversation_bounds();
    test_system_message_pinned_at_front();
    test_copy_is_deep();
    test_move_steals();
    test_scanner_clean();
    test_scanner_split_every_boundary();
    test_scanner_false_alarm();
    test_harness_turn_limit();
    test_harness_eof_mid_conversation();
    test_harness_sentinel_halt();
    test_harness_clean_destruction();
    test_transcript_round_trip();

    std::printf("All %d checks passed.\n", tests_run);
    return 0;
}