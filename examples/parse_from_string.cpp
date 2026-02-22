import std;
import mcpplibs.cmdline;

using namespace mcpplibs;

// 从字符串解析，适合测试或脚本内构造参数
int main() {
    auto app = cmdline::App("tool")
        .version("1.0")
        .arg("cmd").required()
        .option("verbose").help("Verbose")
        .action([](const cmdline::ParsedArgs& p) {
            std::println("cmd = {}", p.positional(0));
            std::println("verbose = {}", p.is_flag_set("verbose"));
        });

    auto result = app.parse_from("tool add --verbose");
    if (!result) {
        if (result.error().is_error())
            std::println("Parse error: {}", result.error().message);
        return result.error().is_error() ? 1 : 0;
    }
    app.run(*result);
    return 0;
}
