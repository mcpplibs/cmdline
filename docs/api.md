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
| `Shell` | 目标 shell 枚举：`bash` / `fish` / `zsh` / `powershell` |
| `Command` | 命令树快照，补全引擎读取它 |

解析返回 `ParseResult`，即 `std::expected<ParsedArgs, ParseError>`。`-h`/`--help`、`--version` 时，`ParseError::kind` 分别为 `help` / `version`（补全见“Shell 补全”，为 `completion`），不携带 `message`，通过 `is_error()` 可区分真实错误。

---

## App

| 方法 | 说明 |
|------|------|
| `.version(s)` `.author(s)` `.description(s)` | 元信息 |
| `.arg(Arg)` `.arg("name")` | 添加位置参数（字符串即链式） |
| `.option(Option)` `.option("name")` | 添加选项 |
| `.subcommand(App)` `.subcommand("name")` | 添加子命令 |
| `.action(fn)` | 根/子命令执行函数：`int(const ParsedArgs&)`（返回值即退出码）或 `void(const ParsedArgs&)`（退出码 0） |
| `.parse(argc, argv)` `.parse_from(Argv)` `.parse_from(string_view)` | 解析，返回 `ParseResult` |
| `.run(parsed)` | 执行解析结果所命中的命令的 action，返回其退出码（规则见“分发规则”） |
| `.run(argc, argv)` `.run(Argv)` | parse + dispatch，返回退出码（推荐主入口）：help/version 为 0，解析错误写入标准错误并返回 2 |
| `.print_help(program_name)` | 打印帮助 |

---

## Arg / Option

**Arg**：`.help(s)` `.required(bool)` `.default_value(s)` `.repeatable(bool)` `.values({...})` `.value_hint(h)` `.complete(fn)`

**Option**：`.short_name(c)` `.long_opt(s)` `.help(s)` `.takes_value(bool)` `.value_name(s)` `.multiple(bool)` `.global(bool)` `.values({...})` `.value_hint(h)` `.complete(fn)`

**App**（子命令）：`.hidden(bool)`；根命令：`.completion(options)`（见“Shell 补全”）

---

## Shell

| 函数 | 说明 |
|------|------|
| `shell_from_string(sv)` | 字符串 → `std::optional<Shell>`（如 `"fish"` → `Shell::fish`） |
| `to_string(shell)` | `Shell` → `std::string_view` |
| `shell_supported(shell)` | 该 shell 补全是否已实现（bash / zsh / fish / powershell） |

## ParseError

| 成员 | 说明 |
|------|------|
| `kind` | 枚举：`help` / `version` / `completion` / `error` |
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
    std::println(std::cerr, "myapp: {}", result.error().message);
    return 2;
}
const cmdline::ParsedArgs& p = *result;
```

`run(argc, argv)` 与 `run(Argv)` 的错误处理与此相同：错误信息写入标准错误（`<name>: <message>` 及 `<name>: see '<name> --help'`），返回 2；标准输出只承载 help、version 与 action 自身的输出。

### 分发规则与退出码

`run(parsed)` 沿解析结果向下找到最深一层被命中的子命令（任意层级），按以下规则执行：

1. 该命令有 action 时，以其自身的解析结果执行，返回值即退出码（`void` action 为 0）。
2. 该命令无 action 时，由路径上最近的、有 action 的祖先命令执行；祖先收到的是它自己这一层的解析结果，可按 `subcommand_name()` 自行分发。
3. 路径上均无 action 且该命令带有子命令（命令组被单独调用）时，向标准错误写出可选的子命令并返回 2。
4. 其余情况返回 0。

```cpp
cmdline::App remote("remote");
remote.subcommand("add")
    .arg("name").required()
    .action([](const cmdline::ParsedArgs& a) -> int {
        return add_remote(a.positional(0)) ? 0 : 1;
    });

auto app = cmdline::App("git").subcommand(std::move(remote));
return app.run(argc, argv); // git remote add origin → add 的退出码；git remote → 2
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
if (!result) return result.error().is_error() ? 2 : 0;
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

---

## Shell 补全

### 一次调用开启

```cpp
(void)app.completion();                                   // 缺省：`completions <shell>` 与隐藏入口 `__complete`
(void)app.completion({.command = "", .entry = "__complete"}); // 不加可见命令，脚本由程序自己写出
```

开启之后：

- `myapp completions bash`（`zsh`、`fish`、`powershell`）打印该 shell 的适配脚本。脚本只有几行，内容是把光标前的词交给
  `myapp __complete …` 并列出回答，不含命令树；程序新增命令之后，用户无需重新安装脚本。
- `myapp __complete <words…>` 是隐藏入口：以命令树、解析所用的同一套词法规则（`completions::classify`）回答最后一个词
  可以补成什么。回答是文本，每行一个候选（`值<TAB>说明`），最后一行 `:<指令>`：`:0` 无附加，`:1` 由 shell 补文件，
  `:2` 由 shell 补目录。
- 两者都在 `--help` 的位置处理，解析返回 `ParseError`，其 `kind` 为 `completion`，`is_error()` 为 `false`。
- 不调用 `completion()` 时，行为与此前一致。

### 补全读取的事实

| 位置 | 方法 | 含义 |
|------|------|------|
| `Option`、`Arg` | `.values({...})` | 可选值的封闭集合：解析校验它，帮助与补全列出它；`Option` 由此带值 |
| `Option`、`Arg` | `.value_hint(ValueHint::file / directory)` | 值是文件或目录，由 shell 补全 |
| `Option`、`Arg` | `.complete(fn)` | 候选回调 `std::vector<std::string>(std::string_view prefix)`，只在补全时调用 |
| `Arg` | `.repeatable()` | 该位置参数代表其后的每个位置参数 |
| `App`（子命令） | `.hidden()` | 不出现在帮助与补全中，照常解析与执行 |

### 引擎与适配器

| 函数 | 说明 |
|------|------|
| `completions::complete(cmd, words)` | 纯函数：命令树与光标前的词 → `Answer`（候选与指令） |
| `completions::render(answer)` | `Answer` → 上述文本协议 |
| `completions::adapter(shell, program, entry)` | 某个 shell 的适配脚本 |
| `completions::classify(word, isSubcommand)` | 一个词对一个命令的含义：`--`、长选项（含 `--k=v`）、短选项、子命令、位置参数；解析与补全共用 |
| `snapshot(app)` | 命令树快照（自 0.0.5）：每个命令含自己的 `-h`/`--help`，有版本的命令含 `--version`；隐藏命令不在其中 |

`generate_completions(cmd|app, shell[, out])` 与 `App::completions(shell)` 的签名不变，自 0.0.6 起输出适配脚本，而不是
写出整棵命令树的脚本。

