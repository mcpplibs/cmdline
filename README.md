# cmdline

> 一个简单易用的命令行解析库/框架 - `import mcpplibs.cmdline;`

使用 C++23 模块的类型安全命令行解析库。流式链式接口，零样板代码。支持位置参数、短/长选项、子命令、全局标志，以及基于 `std::expected` 的错误处理。

## ✨ 特性

- **C++23 模块** — `import mcpplibs.cmdline`

- **流式接口** — 链式构建方法，所见即所得的 API

- **子命令** — 支持 `action` 回调或手动分发的嵌套命令

- **全局选项** — 标志透明地传播到所有子命令

- **类型安全结果** — `std::expected<ParsedArgs, std::string>`

- **多种解析模式** — 支持 `argc/argv`、`cmdline::Argv`（即 `vector<string>`）或原始 `string_view`

## 快速开始

```cpp
import std;
import mcpplibs.cmdline;

using namespace mcpplibs;

int main(int argc, char* argv[]) {
    auto app = cmdline::App("myapp")
        .version("1.0.0")
        .description("A demo CLI")
        .arg("input").required().help("输入文件")
        .option("verbose").short_name('v').help("详细输出")
        .option("config").short_name('c').takes_value().value_name("FILE").help("配置文件")
        .action([](const cmdline::ParsedArgs& p) {
            if (p.is_flag_set("verbose")) std::println("详细模式已开启");
            if (auto c = p.value("config"))  std::println("配置文件: {}", *c);
            std::println("输入文件: {}", p.positional(0));
        });

    return app.run(argc, argv);
}
```

### 子命令 + action / run

```cpp
auto app = cmdline::App("demo")
    .version("0.1.0")
    .description("Demo: subcommands with action dispatch")
    .option("yes").short_name('y').global().help("自动确认")
    .subcommand("add")
        .description("添加目标")
        .arg("target").required()
        .arg("version").required()
        .action([](const cmdline::ParsedArgs& a) {
            std::println("add: {}@{}", a.value("target").value_or(""),
                                       a.value("version").value_or(""));
        })
    .subcommand("remove")
        .description("移除目标")
        .arg("target").required()
        .action([](const cmdline::ParsedArgs& a) { std::println("remove: {}", a.positional(0)); });

app.run(argc, argv);
```

### 多种解析模式

```cpp
auto r1 = app.parse(argc, argv);
auto r2 = app.parse_from(cmdline::Argv{"myapp", "add", "x", "1.0"});
auto r3 = app.parse_from("myapp remove x --yes");
```

## 构建

**使用 xmake**

```shell
xmake                       # 构建库
xmake run basic             # 运行基础示例
xmake -y run cmdline_test   # 运行测试（自动安装 gtest）
```

**使用 mcpp**

```shell
mcpp build                  # 构建库
mcpp test                   # 运行 tests/ 下的测试
```

## 集成到构建工具

### xmake

```lua
-- 添加 mcpplibs 包仓库
add_repositories("mcpplibs-index https://github.com/mcpplibs/mcpplibs-index.git")

-- 添加依赖
add_requires("cmdline")

target("mytool")
    set_kind("binary")
    set_languages("c++23")
    add_files("main.cpp")
    add_packages("cmdline")
    set_policy("build.c++.modules", true)
```

### mcpp

#### 添加依赖

```bash
mcpp add cmdline@0.0.2
```

或在 `mcpp.toml` 中手动添加：

```toml
[dependencies]
cmdline = "0.0.2"
```

#### 构建

```bash
mcpp build
```

#### 代码示例

```cpp
import mcpplibs.cmdline;
// ... 参见上方"快速开始"
```

## 相关链接

- [社区官网](https://mcpp.d2learn.org)
- [社区论坛](https://forum.d2learn.org/category/20) ([`Q: 1067245099`](https://qm.qq.com/q/nBQdBaYNna))
- [mcpp开源主页](https://github.com/mcpp-community)
