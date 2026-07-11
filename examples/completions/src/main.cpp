import std;
import mcpplibs.cmdline;

using namespace mcpplibs;
using cmdline::App;
using cmdline::Option;
using cmdline::ParsedArgs;

[[nodiscard]] App create_subos();

void dummy_action(const ParsedArgs& p) {
    std::println("Options:");
    for (const auto& [key, optvalue] : p.opts) {
        std::println("  {} -> count: {}, value: {}", key, optvalue.count, optvalue.value());
    }

    std::println("Positionals:");
    for (const auto& positional : p.positionals) {
        std::println("  {}", positional);
    }
}

int main(int argc, char* argv[]) {
    // Partially mimicked xlings
    auto app = App("completions")
        .version("0.1.0")
        .author("d2learn community")
        .description("A mimicked xlings using mcpplibs.cmdline with shell completions")
        // Global options
        .option("yes").short_name('y').help("Skip confirmation prompts").global()
        .option("verbose").short_name('v').help("Enable verbose output").global()
        .option("quiet").short_name('q').help("Suppress non-essential output").global()
        .option("agent").help("Plain-text output without TUI formatting (for LLM agents)").global()
        // End of Global options
        .subcommand("install")
            .description("Install packages (e.g. xlings install gcc@15 node)")
            .option(Option("global").short_name('g').help("Install to global scope (not project-local subos)"))
            .option(Option("use").short_name('u').help("Activate the installed version even if another version is currently active"))
            .arg("packages").help("Package names with optional version")
            .action(dummy_action)
        .subcommand("remove")
            .description("Remove a package")
            .arg("package").required().help("Package to remove (name or name@ver)")
            .arg("version").help("Optional version (alternative to name@ver form)")
            .action(dummy_action)
        .subcommand("update")
            .description("Update package index or a specific package")
            .arg("package").help("Package to update (omit for index only)")
            .arg("version").help("Optional version (alternative to name@ver form)")
            .action(dummy_action)
        .subcommand("search")
            .description("Search for packages")
            .arg("keyword").required().help("Search keyword")
            .action(dummy_action)
        .subcommand("list")
            .description("List installed packages")
            .option(Option("all").short_name('a').help("Show packages across all subos (default: current subos only)"))
            .arg("filter").help("Filter pattern")
            .action(dummy_action)
        .subcommand("info")
            .description("Show package information")
            .arg("package").required().help("Package name (or name@ver)")
            .arg("version").help("Optional version (alternative to name@ver form)")
            .action(dummy_action)
        .subcommand("use")
            .description("Switch tool version")
            .option(Option("all").short_name('a').help("Show versions across all subos (default: current subos only)"))
            .arg("target").required().help("Tool name (or name@ver one-shot)")
            .arg("version").help("Version to switch to (omit to list installed versions)")
            .action(dummy_action)
        .subcommand("config")
            .description("Show or modify xlings configuration")
            .option(Option("lang").takes_value().value_name("LANG").help("Set language (en/zh)"))
            .option(Option("mirror").takes_value().value_name("MIRROR").help("Set mirror (GLOBAL/CN)"))
            .option(Option("add-xpkg").takes_value().value_name("FILE").help("Add xpkg file to package index"))
            .option(Option("index-repo").takes_value().value_name("NS:URL").help("Add/update index repo (e.g. myns:https://...git)"))
            .action(dummy_action);
    app.subcommand(create_subos());

    // Completions
    app.subcommand("completions")
        .description("Generate shell completions")
        .option(Option("shell").takes_value().value_name("SHELL").help("Target shell [possible values: bash, fish, zsh]"))
        .action([&app](const ParsedArgs& p) {
            auto shell_opt = p.value("shell");
            if (!shell_opt) {
                std::println(std::cerr, "SHELL not provided.");
                return;
            }

            auto shell = cmdline::shell_from_string(*shell_opt);
            if (!shell || !cmdline::shell_supported(*shell)) {
                std::println(std::cerr, "Unsupported shell: {}", *shell_opt);
                return;
            }

            std::print("{}", app.completions(*shell));
        });

    return app.run(argc, argv);
}

App create_subos() {
    auto subos = App("subos")
        .description("Manage sub-OS environments")
        .subcommand("list")
            .description("List all sub-OS environments")
            .action(dummy_action)
        .subcommand("ls")
            .description("alias: list")
            .action(dummy_action)
        .subcommand("remove")
            .description("Remove a sub-OS")
            .arg("name").required().help("sub-OS name")
            .action(dummy_action)
        .subcommand("rm")
            .description("alias: rm")
            .arg("name").required().help("sub-OS name")
            .action(dummy_action)
        .subcommand("new")
            .description("Create a new sub-OS")
            .option(Option("storage").takes_value().value_name("mode").help("storage mode"))
            .option(Option("image-size").takes_value().value_name("size").help("image size"))
            .option(Option("from").takes_value().value_name("spec").help("from spec"))
            .arg("name").required().help("sub-OS name")
            .action(dummy_action)
        .subcommand("use")
            .description("Switch active sub-OS")
            .option(Option("global").help("persist the choice into ~/.xlings.json + symlinki (legacy behavior; affects every shell)"))
            .option(Option("shell").takes_value().value_name("kind").help("emit shell code on stdout for the user to eval/Invoke-Expression."))
            .option(Option("sandbox").help("Linux-only: enter via proot fs-isolation."))
            .arg("name").required().help("sub-OS name")
            .action(dummy_action)
        .subcommand("info")
            .description("Show sub-OS details")
            .arg("name").help("sub-OS name")
            .action(dummy_action)
        .subcommand("i")
            .description("alias: info")
            .arg("name").help("sub-OS name")
            .action(dummy_action);
    return subos;
}
