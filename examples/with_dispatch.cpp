import std;
import mcpplibs.cmdline;

using namespace mcpplibs;

int main(int argc, char* argv[]) {
    auto app = cmdline::App("demo")
        .version("0.1.0")
        .description("Demo: parse + run with action")
        .option("yes").short_name('y').global(true).help("Auto confirm")
        .subcommand("add")
            .description("Add a target")
            .arg("target").required()
            .arg("version").required()
            .action([](const cmdline::ParsedArgs& args) {
                std::println("add: {}@{}", args.value("target").value_or(""), args.value("version").value_or(""));
            })
        .subcommand("remove")
            .description("Remove a target")
            .arg("target").required()
            .action([](const cmdline::ParsedArgs& args) { std::println("remove: {}", args.positional(0)); });

    return app.run(argc, argv);
}
