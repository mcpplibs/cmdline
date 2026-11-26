#include <gtest/gtest.h>

import std;
import mcpplibs.cmdline;

using namespace mcpplibs::cmdline;

namespace {

// A tree with what a completion reads: nested subcommands, a global option, an
// option with a closed set, one with a callback, one that names a file, a
// repeatable positional, a hidden command.
App tree() {
    App app("tool");
    (void)app.version("1.0");
    (void)app.option(Option("verbose").short_name('v').global().help("Say more"));
    (void)app.option(Option("color").values({"auto", "always", "never"}).help("Colour"));
    (void)app.completion();
    app.subcommand("build")
        .description("Build the project")
        .option(Option("profile").complete([](std::string_view prefix) {
            std::vector<std::string> out;
            for (std::string p : {"dev", "release", "dist"})
                if (p.starts_with(prefix)) out.push_back(p);
            return out;
        }))
        .option(Option("manifest").value_hint(ValueHint::file))
        .subcommand("add")
        .description("Add packages")
        .subcommand("internal-thing")
        .hidden();
    return app;
}

std::vector<std::string> values(const completions::Answer& a) {
    std::vector<std::string> out;
    for (const auto& c : a.candidates) out.push_back(c.value);
    return out;
}

completions::Answer ask(const App& app, std::vector<std::string> words) {
    return completions::complete(snapshot(app), words);
}

} // namespace

TEST(Completion, SubcommandsAtTheRootAndNoHiddenOne) {
    auto app = tree();
    auto a = ask(app, {""});
    auto v = values(a);
    EXPECT_NE(std::ranges::find(v, "build"), v.end());
    EXPECT_NE(std::ranges::find(v, "completions"), v.end());
    EXPECT_EQ(std::ranges::find(v, "internal-thing"), v.end());
    EXPECT_EQ(values(ask(app, {"bu"})), std::vector<std::string>{"build"});
}

TEST(Completion, OptionsOfTheCommandAndTheGlobalOnes) {
    auto app = tree();
    auto v = values(ask(app, {"build", "--"}));
    EXPECT_NE(std::ranges::find(v, "--profile"), v.end());
    EXPECT_NE(std::ranges::find(v, "--verbose"), v.end()) << "a global option under a subcommand";
    EXPECT_NE(std::ranges::find(v, "--help"), v.end());
    EXPECT_EQ(std::ranges::find(v, "--color"), v.end()) << "the root's own option is not the subcommand's";
}

TEST(Completion, ValuesFromTheClosedSetTheCallbackAndTheHint) {
    auto app = tree();
    EXPECT_EQ(values(ask(app, {"--color", "a"})), (std::vector<std::string>{"auto", "always"}));
    EXPECT_EQ(values(ask(app, {"--color=n"})), std::vector<std::string>{"--color=never"});
    EXPECT_EQ(values(ask(app, {"build", "--profile", "r"})), std::vector<std::string>{"release"});
    auto file = ask(app, {"build", "--manifest", ""});
    EXPECT_TRUE(file.candidates.empty());
    EXPECT_EQ(file.directive, completions::Directive::files);
}

TEST(Completion, AValueTakenIsNotOfferedAgainAsASubcommand) {
    auto app = tree();
    // `--color` took `never`; what follows is the root's next word.
    auto v = values(ask(app, {"--color", "never", "b"}));
    EXPECT_EQ(v, std::vector<std::string>{"build"});
}

TEST(Completion, AfterTheEndOfOptionsOnlyPositionals) {
    App app("tool");
    (void)app.arg(Arg("file").repeatable().value_hint(ValueHint::file));
    auto a = ask(app, {"--", "-"});
    EXPECT_TRUE(a.candidates.empty()) << "an option offered after --";
    EXPECT_EQ(a.directive, completions::Directive::files);
}

TEST(Completion, ARepeatablePositionalKeepsOffering) {
    App app("tool");
    (void)app.arg(Arg("pkg").repeatable().complete([](std::string_view) {
        return std::vector<std::string>{"fmt", "zlib"};
    }));
    EXPECT_EQ(values(ask(app, {"fmt", "z"})), std::vector<std::string>{"zlib"});
}

TEST(Completion, TheEntryAnswersAndIsNoError) {
    auto app = tree();
    testing::internal::CaptureStdout();
    auto r = app.parse_from(Argv{"tool", "__complete", "bu"});
    auto out = testing::internal::GetCapturedStdout();
    ASSERT_FALSE(r.has_value());
    EXPECT_FALSE(r.error().is_error());
    EXPECT_EQ(r.error().kind, ParseError::completion);
    EXPECT_EQ(out, "build\tBuild the project\n:0\n");
}

TEST(Completion, TheCommandPrintsAnAdapterThatHoldsNoTree) {
    auto app = tree();
    for (auto shell : {Shell::bash, Shell::zsh, Shell::fish, Shell::powershell}) {
        auto script = app.completions(shell);
        EXPECT_NE(script.find("tool __complete"), std::string::npos) << to_string(shell);
        EXPECT_EQ(script.find("build"), std::string::npos) << to_string(shell) << ": the tree is copied";
    }
    testing::internal::CaptureStdout();
    auto r = app.parse_from(Argv{"tool", "completions", "fish"});
    auto out = testing::internal::GetCapturedStdout();
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, ParseError::completion);
    EXPECT_NE(out.find("complete -c tool"), std::string::npos);
    auto bad = app.parse_from(Argv{"tool", "completions", "tcsh"});
    ASSERT_FALSE(bad.has_value());
    EXPECT_TRUE(bad.error().is_error());
}

TEST(Completion, OffUnlessAskedFor) {
    App app("tool");
    (void)app.arg(Arg("x"));
    auto r = app.parse_from(Argv{"tool", "__complete"});
    ASSERT_TRUE(r.has_value()) << "an app without completion() took the entry";
    EXPECT_EQ(r->positional(0), "__complete");
}

TEST(Values, TheParseRefusesAValueOutsideTheSet) {
    auto app = tree();
    auto bad = app.parse_from(Argv{"tool", "--color", "pink"});
    ASSERT_FALSE(bad.has_value());
    EXPECT_NE(bad.error().message.find("one of auto, always, never"), std::string::npos) << bad.error().message;
    EXPECT_TRUE(app.parse_from(Argv{"tool", "--color=always"}).has_value());
    App positional("tool");
    (void)positional.arg(Arg("shell").values({"bash", "zsh"}));
    EXPECT_FALSE(positional.parse_from(Argv{"tool", "csh"}).has_value());
}

TEST(Hidden, AHiddenCommandParsesAndIsNotListed) {
    auto app = tree();
    testing::internal::CaptureStdout();
    app.print_help();
    auto help = testing::internal::GetCapturedStdout();
    EXPECT_EQ(help.find("internal-thing"), std::string::npos);
    EXPECT_NE(help.find("completions"), std::string::npos);
    auto r = app.parse_from(Argv{"tool", "internal-thing"});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->subcommand_name(), "internal-thing");
}

TEST(Classify, TheParseAndTheCompletionReadWordsAlike) {
    using completions::Classified;
    EXPECT_EQ(completions::classify("--", false).kind, Classified::end_of_options);
    auto kv = completions::classify("--k=v", false);
    EXPECT_EQ(kv.kind, Classified::long_option);
    EXPECT_EQ(kv.key, "k");
    EXPECT_EQ(kv.inline_value, std::optional<std::string_view>{"v"});
    EXPECT_EQ(completions::classify("-x", false).kind, Classified::short_option);
    EXPECT_EQ(completions::classify("sub", true).kind, Classified::subcommand);
    EXPECT_EQ(completions::classify("-", false).kind, Classified::positional);
}
