import std;
import mcpplibs.cmdline;

using namespace mcpplibs;

int main(int argc, char* argv[]) {
    auto app = cmdline::App("myapp")
        .version("1.0.0")
        .description("A demo CLI")
        .arg("input").required().help("Input file")
        .option("verbose").short_name('v').help("Verbose output")
        .option("config").short_name('c').takes_value().value_name("FILE").help("Config file")
        .action([](const cmdline::ParsedArgs& p) {
            if (p.is_flag_set("verbose")) std::println("Verbose on");
            if (auto c = p.value("config")) std::println("Config: {}", *c);
            std::println("Input: {}", p.positional(0));
        });

    return app.run(argc, argv);
}
