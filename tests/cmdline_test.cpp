#include <gtest/gtest.h>

import std;
import mcpplibs.cmdline;

using namespace mcpplibs::cmdline;

// Build argv from program name + args for parse()
static std::vector<std::string> g_argv_storage;
static std::vector<char*> g_argv_ptrs;

static std::pair<int, char**> make_argv(std::string_view program_name, std::initializer_list<std::string_view> args) {
    g_argv_storage.clear();
    g_argv_ptrs.clear();
    g_argv_storage.push_back(std::string(program_name));
    for (auto arg : args) g_argv_storage.push_back(std::string(arg));
    for (auto& s : g_argv_storage) g_argv_ptrs.push_back(s.data());
    g_argv_ptrs.push_back(nullptr);
    return { static_cast<int>(g_argv_ptrs.size()) - 1, g_argv_ptrs.data() };
}

// -----------------------------------------------------------------------------
// Parse input: parse_from(span), parse_from(string_view)
// -----------------------------------------------------------------------------
TEST(Parse, ParseFromSpan) {
    App app("prog");
    (void)app.arg(Arg("a").required());
    auto result = app.parse_from(Argv{"prog", "val"});
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ((*result).positional(0), "val");
    EXPECT_EQ((*result).positional_count(), 1u);
}

TEST(Parse, ParseFromStringView) {
    App app("tool");
    (void)app.arg(Arg("cmd").required());
    auto result = app.parse_from("tool run");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ((*result).positional(0), "run");
}

TEST(Parse, ParseFromStringViewWithOptions) {
    App app("tool");
    (void)app.arg(Arg("cmd").required())
       .option(Option("verbose").long_opt("verbose"));
    auto result = app.parse_from("tool run --verbose");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_TRUE((*result).is_flag_set("verbose"));
}

TEST(Parse, EmptyArgsUnexpected) {
    App app("prog");
    auto result = app.parse_from(Argv{});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "no program name");
}

TEST(Parse, EmptyCommandLineUnexpected) {
    App app("prog");
    auto result = app.parse_from("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "empty command line");
}

// -----------------------------------------------------------------------------
// Positional args: required, default_value, arg(), arg_count(), value(name)
// -----------------------------------------------------------------------------
TEST(Args, PositionalRequired) {
    App app("p");
    (void)app.arg(Arg("x").required());
    auto ok = app.parse_from(Argv{"p", "v"});
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(ok->positional(0), "v");
    EXPECT_EQ(ok->value("x"), "v");

    auto fail = app.parse_from(Argv{"p"});
    ASSERT_FALSE(fail.has_value());
    EXPECT_TRUE(fail.error().message.find("required") != std::string::npos);
}

TEST(Args, DefaultValue) {
    App app("p");
    (void)app.arg(Arg("x").default_value("default"));
    auto result = app.parse_from(Argv{"p"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->positional(0), "default");
}

TEST(Args, ArgCountAndArgOr) {
    App app("p");
    (void)app.arg(Arg("a")).arg(Arg("b"));
    auto result = app.parse_from(Argv{"p", "1", "2"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->positional_count(), 2u);
    EXPECT_EQ(result->positional(0), "1");
    EXPECT_EQ(result->positional(1), "2");
    EXPECT_EQ(result->positional_or(2, "def"), "def");
}

// -----------------------------------------------------------------------------
// Options: short, long, flag, takes_value, is_flag_set, value, opt_key
// -----------------------------------------------------------------------------
TEST(Options, LongFlag) {
    App app("p");
    (void)app.option(Option("verbose").long_opt("verbose").help("v"));
    auto result = app.parse_from(Argv{"p", "--verbose"});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_flag_set("verbose"));
}

TEST(Options, ShortFlag) {
    App app("p");
    (void)app.option(Option("v").short_name('v').long_opt("verbose"));
    auto result = app.parse_from(Argv{"p", "-v"});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_flag_set("verbose"));
}

TEST(Options, TakesValue) {
    App app("p");
    (void)app.option(Option("config").long_opt("config").takes_value().value_name("FILE"));
    auto result = app.parse_from(Argv{"p", "--config", "my.conf"});
    ASSERT_TRUE(result.has_value());
    auto config_val = result->value("config");
    ASSERT_TRUE(config_val.has_value());
    EXPECT_EQ(*config_val, "my.conf");
}

TEST(Options, TakesValueEq) {
    App app("p");
    (void)app.option(Option("config").long_opt("config").takes_value());
    auto result = app.parse_from(Argv{"p", "--config=path.conf"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value("config").value_or(""), "path.conf");
}

TEST(Options, OptOrEmpty) {
    App app("p");
    (void)app.option(Option("x").long_opt("x").takes_value());
    auto result = app.parse_from(Argv{"p"});
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->option_or_empty("x").is_set());
    EXPECT_EQ(result->option_or_empty("x").value_or("default"), "default");
}

TEST(Options, OptionValueIsSet) {
    App app("p");
    (void)app.option(Option("f").long_opt("flag"));
    auto result = app.parse_from(Argv{"p", "--flag"});
    ASSERT_TRUE(result.has_value());
    auto opt = result->option("flag");
    ASSERT_TRUE(opt.has_value());
    EXPECT_TRUE(opt->get().is_set());
}

// -----------------------------------------------------------------------------
// opt(string_view) 链式：.option("name").help("...").global() 等
// -----------------------------------------------------------------------------
TEST(OptBuilder, StringChain) {
    App app("p");
    app.option("verbose").help("v");
    auto result = app.parse_from(Argv{"p", "--verbose"});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_flag_set("verbose"));
}

TEST(OptBuilder, StringChainTakesValue) {
    App app("p");
    app.option("config").takes_value().value_name("FILE");
    auto result = app.parse_from(Argv{"p", "--config", "my.conf"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value("config").value_or(""), "my.conf");
}

TEST(OptBuilder, ChainToSubcommand) {
    App app("cli");
    app.option("yes").global().help("confirm")
        .subcommand("add")
            .description("Add")
            .action([](const ParsedArgs&) {});
    auto result = app.parse_from(Argv{"cli", "--yes", "add"});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_flag_set("yes"));
    EXPECT_EQ(result->subcommand_name(), "add");
}

// -----------------------------------------------------------------------------
// arg(string_view) 链式：.arg("name").required().help("...") 等
// -----------------------------------------------------------------------------
TEST(ArgBuilder, StringChainRequired) {
    App app("p");
    app.arg("input").required();
    auto result = app.parse_from(Argv{"p", "file.txt"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->positional(0), "file.txt");
}

TEST(ArgBuilder, StringChainDefaultValue) {
    App app("p");
    app.arg("x").default_value("default");
    auto result = app.parse_from(Argv{"p"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->positional(0), "default");
}

TEST(ArgBuilder, ChainToOpt) {
    App app("p");
    app.arg("input").required()
        .option("verbose").help("v");
    auto result = app.parse_from(Argv{"p", "f", "--verbose"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->positional(0), "f");
    EXPECT_TRUE(result->is_flag_set("verbose"));
}

// -----------------------------------------------------------------------------
// Subcommand 内 .arg("name").required() / .option("name")
// -----------------------------------------------------------------------------
TEST(Subcommand, StringBuilderWithArgString) {
    App app("cli");
    app.subcommand("add")
        .description("Add")
        .arg("t").required()
        .action([](const ParsedArgs& args) { EXPECT_EQ(args.positional(0), "x"); });
    auto result = app.parse_from(Argv{"cli", "add", "x"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->subcommand_name(), "add");
    EXPECT_EQ(result->subcommand()->get().positional(0), "x");
}

TEST(Subcommand, StringBuilderWithOptString) {
    App app("cli");
    app.subcommand("run")
        .description("Run")
        .option("dry-run").help("dry run")
        .action([](const ParsedArgs&) {});
    auto result = app.parse_from(Argv{"cli", "run", "--dry-run"});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->subcommand()->get().is_flag_set("dry-run"));
}

// -----------------------------------------------------------------------------
// parse(argc, argv)
// -----------------------------------------------------------------------------
TEST(Parse, ParseArgcArgv) {
    App app("myprog");
    (void)app.arg(Arg("input").required());
    auto [argc, argv] = make_argv("myprog", {"file.txt"});
    auto result = app.parse(argc, argv);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->positional(0), "file.txt");
}

// -----------------------------------------------------------------------------
// Help and version
// -----------------------------------------------------------------------------
TEST(Parse, HelpRequested) {
    App app("prog");
    (void)app.version("1.0");
    auto [argc, argv] = make_argv("prog", {"--help"});
    auto result = app.parse(argc, argv);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, ParseError::help);
}

TEST(Parse, VersionRequested) {
    App app("prog");
    (void)app.version("1.0.0");
    auto [argc, argv] = make_argv("prog", {"--version"});
    auto result = app.parse(argc, argv);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, ParseError::version);
}

TEST(Parse, VersionEmptyNoError) {
    App app("prog");
    auto [argc, argv] = make_argv("prog", {"--version"});
    auto result = app.parse(argc, argv);
    // When version is empty, --version is parsed as long option; no Option("version") => unknown option
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("unknown") != std::string::npos);
}

// -----------------------------------------------------------------------------
// Errors: unknown option, missing value
// -----------------------------------------------------------------------------
TEST(Parse, UnknownLongOption) {
    App app("p");
    (void)app.option(Option("known").long_opt("known"));
    auto result = app.parse_from(Argv{"p", "--unknown"});
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("unknown") != std::string::npos);
}

TEST(Parse, OptionRequiresValue) {
    App app("p");
    (void)app.option(Option("cfg").long_opt("config").takes_value());
    auto result = app.parse_from(Argv{"p", "--config"});
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("value") != std::string::npos || result.error().message.find("config") != std::string::npos);
}

// -----------------------------------------------------------------------------
// Subcommands: string builder, App object, has_subcommand, subcommand_name
// -----------------------------------------------------------------------------
TEST(Subcommand, StringBuilderStyle) {
    App app("cli");
    app.subcommand("add")
        .description("Add")
        .arg(Arg("name").required())
        .action([](const ParsedArgs&) {});

    auto result = app.parse_from(Argv{"cli", "add", "foo"});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->has_subcommand());
    EXPECT_EQ(result->subcommand_name(), "add");
    auto sub = result->subcommand();
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->get().positional(0), "foo");
}

TEST(Subcommand, AppObjectStyle) {
    App app("cli");
    (void)app.subcommand(
        App("remove")
            .description("Remove")
            .arg(Arg("target").required())
            .action([](const ParsedArgs&) {}));

    auto result = app.parse_from(Argv{"cli", "remove", "x"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->subcommand_name(), "remove");
    EXPECT_EQ(result->subcommand()->get().positional(0), "x");
}

TEST(Subcommand, MultipleSubcommandsChain) {
    App app("cli");
    app.subcommand("add").description("Add").arg(Arg("a").required()).action([](const ParsedArgs&) {})
      .subcommand("rm").description("Remove").action([](const ParsedArgs&) {});

    auto result_add = app.parse_from(Argv{"cli", "add", "x"});
    ASSERT_TRUE(result_add.has_value());
    EXPECT_EQ(result_add->subcommand_name(), "add");
    EXPECT_EQ(result_add->subcommand()->get().positional(0), "x");

    auto r2 = app.parse_from(Argv{"cli", "rm"});
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->subcommand_name(), "rm");
}

// -----------------------------------------------------------------------------
// Dispatch
// -----------------------------------------------------------------------------
TEST(Dispatch, RootHandler) {
    int called = 0;
    App app("p");
    (void)app.arg(Arg("x").required())
       .action([&called](const ParsedArgs& args) {
           called = 1;
           EXPECT_EQ(args.positional(0), "hello");
       });
    auto result = app.parse_from(Argv{"p", "hello"});
    ASSERT_TRUE(result.has_value());
    app.run(*result);
    EXPECT_EQ(called, 1);
}

TEST(Dispatch, SubcommandHandler) {
    int add_called = 0;
    App app("cli");
    app.subcommand("add")
        .arg(Arg("t").required())
        .action([&add_called](const ParsedArgs& args) {
            add_called = 1;
            EXPECT_EQ(args.positional(0), "python");
        });

    auto result = app.parse_from(Argv{"cli", "add", "python"});
    ASSERT_TRUE(result.has_value());
    app.run(*result);
    EXPECT_EQ(add_called, 1);
}

// -----------------------------------------------------------------------------
// Global option
// -----------------------------------------------------------------------------
TEST(Options, GlobalOption) {
    App app("cli");
    (void)app.option(Option("yes").long_opt("yes").global().help("confirm"));
    app.subcommand("add")
        .description("Add")
        .action([](const ParsedArgs&) {});

    auto result = app.parse_from(Argv{"cli", "--yes", "add"});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_flag_set("yes"));
    auto sub = result->subcommand();
    ASSERT_TRUE(sub.has_value());
    EXPECT_TRUE(sub->get().is_flag_set("yes"));
}

TEST(Options, GlobalOptionAfterSubcommand) {
    App app("cli");
    (void)app.option(Option("yes").long_opt("yes").short_name('y').global().help("confirm"));
    app.subcommand("add")
        .description("Add")
        .arg("target").required()
        .arg("version").required()
        .action([](const ParsedArgs&) {});

    auto r_long = app.parse_from(Argv{"cli", "add", "xx", "000", "--yes"});
    ASSERT_TRUE(r_long.has_value()) << r_long.error();
    EXPECT_TRUE(r_long->is_flag_set("yes"));

    auto r_short = app.parse_from(Argv{"cli", "add", "xx", "000", "-y"});
    ASSERT_TRUE(r_short.has_value()) << r_short.error();
    EXPECT_TRUE(r_short->is_flag_set("yes"));
}

// -----------------------------------------------------------------------------
// value by positional name
// -----------------------------------------------------------------------------
TEST(Args, GetOneByPositionalName) {
    App app("p");
    (void)app.arg(Arg("input").required()).arg(Arg("output").required());
    auto result = app.parse_from(Argv{"p", "a.in", "a.out"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value("input"), "a.in");
    EXPECT_EQ(result->value("output"), "a.out");
}

// -----------------------------------------------------------------------------
// -- as positional separator
// -----------------------------------------------------------------------------
TEST(Parse, DoubleDashPositionalsOnly) {
    App app("p");
    (void)app.arg(Arg("a").required()).arg(Arg("b").required());
    auto result = app.parse_from(Argv{"p", "first", "--", "-x", "--second"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->positional_count(), 3u);
    EXPECT_EQ(result->positional(0), "first");
    EXPECT_EQ(result->positional(1), "-x");
    EXPECT_EQ(result->positional(2), "--second");
}
