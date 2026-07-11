#include <gtest/gtest.h>

import std;
import mcpplibs.cmdline;

using namespace mcpplibs::cmdline;

// -----------------------------------------------------------------------------
// Shell completion generation: bash
// -----------------------------------------------------------------------------
TEST(BashCompletions, GeneratesScript) {
    App app("myapp");
    (void)app.version("1.0");
    (void)app.option(Option("verbose").short_name('v').help("Verbose"));
    (void)app.option(Option("config").long_opt("config").takes_value().value_name("FILE").help("Config file"));

    std::ostringstream oss;
    generate_completions(app, Shell::bash, oss);
    auto result = oss.str();

    EXPECT_NE(result.find("_myapp()"), std::string::npos);
    EXPECT_NE(result.find("complete -F _myapp"), std::string::npos);
    EXPECT_NE(result.find("--help"), std::string::npos);
    EXPECT_NE(result.find("--version"), std::string::npos);
    EXPECT_NE(result.find("--config"), std::string::npos);
    EXPECT_NE(result.find("compgen -f"), std::string::npos);
}

TEST(BashCompletions, SubcommandDispatch) {
    App app("myapp");
    app.subcommand("install").description("Install packages");
    app.subcommand("remove").description("Remove packages");

    std::ostringstream oss;
    generate_completions(app, Shell::bash, oss);
    auto result = oss.str();

    EXPECT_NE(result.find("\"myapp,install\")"), std::string::npos);
    EXPECT_NE(result.find("\"myapp,remove\")"), std::string::npos);
    EXPECT_NE(result.find("myapp__subcmd__install)"), std::string::npos);
    EXPECT_NE(result.find("myapp__subcmd__remove)"), std::string::npos);
}

TEST(BashCompletions, GlobalUnderSubcommand) {
    App app("myapp");
    (void)app.option(Option("verbose").short_name('v').long_opt("verbose").global().help("Verbose"));
    (void)app.subcommand("install").description("Install");

    std::ostringstream oss;
    generate_completions(app, Shell::bash, oss);
    auto result = oss.str();

    // Global appears in root opts
    auto root_start = result.find("myapp)");
    ASSERT_NE(root_start, std::string::npos);
    auto root_branch = result.substr(root_start,
        result.find("esac", root_start) - root_start);
    EXPECT_NE(root_branch.find("--verbose"), std::string::npos);

    // Global also appears under subcommand branch
    auto install_start = result.find("myapp__subcmd__install)");
    ASSERT_NE(install_start, std::string::npos);
    auto install_branch = result.substr(install_start,
        result.find(";;", install_start) - install_start);
    EXPECT_NE(install_branch.find("--verbose"), std::string::npos);
}

// -----------------------------------------------------------------------------
// Shell completion generation: fish
// -----------------------------------------------------------------------------
TEST(FishCompletions, GeneratesScript) {
    App app("myapp");
    (void)app.option(Option("verbose").short_name('v').help("Verbose"));

    std::ostringstream oss;
    generate_completions(app, Shell::fish, oss);
    auto result = oss.str();

    EXPECT_NE(result.find("complete -c myapp"), std::string::npos);
    EXPECT_EQ(result.find("function"), std::string::npos);
}

TEST(FishCompletions, SubcommandHelpers) {
    App app("myapp");
    app.subcommand("install").description("Install packages");
    app.subcommand("remove").description("Remove packages");

    std::ostringstream oss;
    generate_completions(app, Shell::fish, oss);
    auto result = oss.str();

    EXPECT_NE(result.find("function __fish_myapp_global_optspecs"), std::string::npos);
    EXPECT_NE(result.find("function __fish_myapp_needs_command"), std::string::npos);
    EXPECT_NE(result.find("function __fish_myapp_using_subcommand"), std::string::npos);
    EXPECT_NE(result.find("-a \"install\""), std::string::npos);
    EXPECT_NE(result.find("-a \"remove\""), std::string::npos);
}

// -----------------------------------------------------------------------------
// Shell completion generation: zsh
// -----------------------------------------------------------------------------
TEST(ZshCompletions, GeneratesScript) {
    App app("myapp");
    (void)app.option(Option("verbose").short_name('v').help("Verbose"));
    (void)app.option(Option("config").long_opt("config").takes_value().value_name("FILE").help("Config file"));

    std::ostringstream oss;
    generate_completions(app, Shell::zsh, oss);
    auto result = oss.str();

    EXPECT_NE(result.find("#compdef myapp"), std::string::npos);
    EXPECT_NE(result.find("_myapp()"), std::string::npos);
    EXPECT_NE(result.find("_arguments \"${_arguments_options[@]}\" :"), std::string::npos);
    EXPECT_NE(result.find("compdef _myapp myapp"), std::string::npos);
    EXPECT_NE(result.find("_default"), std::string::npos);
}

TEST(ZshCompletions, CommandsHelpers) {
    App app("myapp");
    app.subcommand("install").description("Install packages");
    app.subcommand("remove").description("Remove packages");

    std::ostringstream oss;
    generate_completions(app, Shell::zsh, oss);
    auto result = oss.str();

    EXPECT_NE(result.find("_myapp_commands()"), std::string::npos);
    EXPECT_NE(result.find("_describe -t commands 'myapp commands'"), std::string::npos);
    EXPECT_NE(result.find("'install:Install packages'"), std::string::npos);
}

// -----------------------------------------------------------------------------
// Shell completion generation: no subcommands (all shells)
// -----------------------------------------------------------------------------
TEST(Completions, NoSubcommands) {
    App app("myapp");
    (void)app.option(Option("verbose").short_name('v').help("Verbose"));

    {
        std::ostringstream oss;
        generate_completions(app, Shell::bash, oss);
        auto r = oss.str();
        EXPECT_NE(r.find("_myapp()"), std::string::npos);
        EXPECT_EQ(r.find("__subcmd__"), std::string::npos);
    }
    {
        std::ostringstream oss;
        generate_completions(app, Shell::fish, oss);
        auto r = oss.str();
        EXPECT_NE(r.find("complete -c myapp"), std::string::npos);
        EXPECT_EQ(r.find("needs_command"), std::string::npos);
    }
    {
        std::ostringstream oss;
        generate_completions(app, Shell::zsh, oss);
        auto r = oss.str();
        EXPECT_NE(r.find("#compdef myapp"), std::string::npos);
        EXPECT_EQ(r.find("_commands"), std::string::npos);
    }
}
