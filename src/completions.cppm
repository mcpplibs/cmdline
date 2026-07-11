module;

export module mcpplibs.cmdline:completions;

import std;
import :options;

namespace mcpplibs::cmdline {

export enum class Shell {
    bash,
    fish,
    zsh,
};

struct ShellEntry {
    Shell value;
    std::string_view name;
};

constexpr std::array<ShellEntry, 3> shell_entries {{
    {Shell::bash, "bash"},
    {Shell::fish, "fish"},
    {Shell::zsh,  "zsh"},
}};

export [[nodiscard]] std::optional<Shell> shell_from_string(std::string_view sv) {
    for (const auto& [value, name] : shell_entries) {
        if (name == sv) return value;
    }
    return std::nullopt;
}

export [[nodiscard]] std::string_view to_string(Shell shell) {
    for (const auto& [value, name] : shell_entries) {
        if (value == shell) return name;
    }
    return {};
}

export [[nodiscard]] bool shell_supported(Shell shell) {
    switch (shell) {
        case Shell::fish: return true;
        case Shell::bash: return true;
        case Shell::zsh:  return false;
    }
    return false;
}

}; // namespace mcpplibs::cmdline

namespace mcpplibs::cmdline::completions {

export struct Command {
    std::string name;
    std::string description;
    std::string version;
    std::vector<detail::Arg> args;
    std::vector<detail::Option> options;
    std::vector<Command> subcommands;

    [[nodiscard]] bool has_positionals() const { return !args.empty(); }
    [[nodiscard]] bool has_subcommands() const { return !subcommands.empty(); }
};

}; // namespace mcpplibs::cmdline::completions
