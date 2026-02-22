# API 与示例

## 类型一览

| 类型 | 说明 |
|------|------|
| `App` | 根或子命令 |
| `Arg` | 位置参数 |
| `Option` | 选项（短/长、带值、多值、全局） |
| `ParsedArgs` | 解析结果 |
| `OptionValue` | 单个选项取值（flag 计数 + values） |
| `ParseError` | 解析错误（kind + message） |
| `Argv` | `std::vector<std::string>` 的别名，用于 `parse_from` |

解析返回 `ParseResult`，即 `std::expected<ParsedArgs, ParseError>`。`-h`/`--help`、`--version` 时，`ParseError::kind` 分别为 `help` / `version`，不携带 `message`，通过 `is_error()` 可区分真实错误。

---

## App

| 方法 | 说明 |
|------|------|
| `.version(s)` `.author(s)` `.description(s)` | 元信息 |
| `.arg(Arg)` `.arg("name")` | 添加位置参数（字符串即链式） |
| `.option(Option)` `.option("name")` | 添加选项 |
| `.subcommand(App)` `.subcommand("name")` | 添加子命令 |
| `.action(fn)` | 根/子命令执行函数 `void(const ParsedArgs&)` |
| `.parse(argc, argv)` `.parse_from(Argv)` `.parse_from(string_view)` | 解析，返回 `ParseResult` |
| `.run(parsed)` | 根据解析结果执行对应 action |
| `.run(argc, argv)` | 一键 parse + dispatch + 返回退出码（推荐主入口） |
| `.print_help(program_name)` | 打印帮助 |

---

## Arg / Option

**Arg**：`.help(s)` `.required(bool)` `.default_value(s)`

**Option**：`.short_name(c)` `.long_opt(s)` `.help(s)` `.takes_value(bool)` `.value_name(s)` `.multiple(bool)` `.global(bool)`

---

## ParseError

| 成员 | 说明 |
|------|------|
| `kind` | 枚举：`help` / `version` / `error` |
| `message` | 错误文本（仅 `kind == error` 时有内容） |
| `.is_error()` | `kind == error` 时返回 `true` |

```cpp
auto result = app.parse(argc, argv);
if (!result) {
    if (!result.error().is_error()) return 0; // help / version，已打印
    std::println("Error: {}", result.error().message);
    return 1;
}
```

---

## ParsedArgs

| 方法 | 说明 |
|------|------|
| `.is_flag_set(name)` | 选项是否被设置 |
| `.value(name)` | 选项或同名位置参数的一个值 `optional<string>` |
| `.option(name)` | 选项的 `OptionValue`（optional） |
| `.option_or_empty(name)` | 选项值，未设置则空 |
| `.positional(i)` `.positional_or(i, default)` `.positional_count()` | 按索引取位置参数 |
| `.has_subcommand()` `.subcommand_name()` `.subcommand()` | 子命令信息 |

**OptionValue**：`.value()` `.value_or(default)` `.is_set()`

---

## 示例

### 最简：位置参数 + 选项

```cpp
import std;
import mcpplibs.cmdline;

using namespace mcpplibs;

int main(int argc, char* argv[]) {
    auto app = cmdline::App("myapp")
        .version("1.0.0")
        .description("My CLI")
        .arg("input").required().help("Input file")
        .option("verbose").short_name('v').help("Verbose")
        .option("config").short_name('c').takes_value().value_name("FILE").help("Config file")
        .action([](const cmdline::ParsedArgs& p) {
            if (p.is_flag_set("verbose")) std::println("Verbose on");
            if (auto c = p.value("config")) std::println("Config: {}", *c);
            std::println("Input: {}", p.positional(0));
        });

    return app.run(argc, argv); // parse + dispatch + exit code
}
```

### 手动解析与错误处理

```cpp
auto result = app.parse(argc, argv);
if (!result) {
    if (!result.error().is_error()) return 0; // help / version
    std::println("Error: {}", result.error().message);
    return 1;
}
const cmdline::ParsedArgs& p = *result;
```

### 多种输入

```cpp
auto r1 = app.parse(argc, argv);
auto r2 = app.parse_from(cmdline::Argv{"myapp", "add", "x"});
auto r3 = app.parse_from("myapp add x --yes");
```

### 子命令 + action / run

```cpp
using namespace mcpplibs;

auto app = cmdline::App("demo")
    .version("0.1.0")
    .option("yes").short_name('y').global().help("Auto confirm")
    .subcommand("add")
        .description("Add a target")
        .arg("target").required()
        .arg("version").required()
        .action([](const cmdline::ParsedArgs& a) {
            std::println("add: {}@{}", a.value("target").value_or(""), a.value("version").value_or(""));
        })
    .subcommand("remove")
        .description("Remove a target")
        .arg("target").required()
        .action([](const cmdline::ParsedArgs& a) { std::println("remove: {}", a.positional(0)); });

return app.run(argc, argv);
```

### 自行分发子命令

```cpp
auto result = app.parse(argc, argv);
if (!result) return result.error().is_error() ? 1 : 0;
const cmdline::ParsedArgs& parsed = *result;
if (parsed.has_subcommand()) {
    auto sub = parsed.subcommand();
    if (sub && parsed.subcommand_name() == "add") {
        const cmdline::ParsedArgs& sub_args = sub->get();
        // 使用 sub_args.positional(0) 等
    }
}
```

### 传对象写法（与字符串链式等价）

```cpp
using namespace mcpplibs;

app.option(cmdline::Option("yes").long_opt("yes").global().help("Auto confirm"));
app.arg(cmdline::Arg("input").required().help("Input file"));
app.subcommand(cmdline::App("add").description("Add").arg(cmdline::Arg("x").required()).action([](const cmdline::ParsedArgs&) {}));
```
