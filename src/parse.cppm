module;

export module mcpplibs.cmdline:parse;

import std;

namespace mcpplibs::cmdline::detail {

/// 选项解析结果：同时表示 flag（用 count）和带值/多值选项（用 values）。
/// 不用 std::optional<std::string> 的原因：flag 无值且需出现次数；multiple 需多个值。
export struct OptionValue {
    int count = 0;                      // flag 出现次数（如 -v -v -v）
    std::vector<std::string> values;    // 带值选项的值，multiple 时多个

    [[nodiscard]] std::string value() const {
        return values.empty() ? std::string{} : values.back();
    }
    [[nodiscard]] std::string value_or(std::string_view default_value) const {
        return values.empty() ? std::string{default_value} : values.back();
    }
    [[nodiscard]] bool is_set() const { return count > 0 || !values.empty(); }
};

export struct ParseError {
    enum Kind : std::uint8_t { help, version, error };
    Kind kind = error;
    std::string message;

    bool is_error() const { return kind == error; }

    static ParseError make_help() { return {.kind = help}; }
    static ParseError make_version() { return {.kind = version}; }

    friend std::ostream& operator<<(std::ostream& os, const ParseError& e) {
        return os << e.message;
    }
};

export struct ParsedArgs {
    std::map<std::string, OptionValue, std::less<>> opts;
    std::vector<std::string> positionals;
    std::vector<std::string> positional_names_;
    std::string subcommand_name_;
    std::unique_ptr<ParsedArgs> subcommand_matches;

    [[nodiscard]] std::optional<std::reference_wrapper<const OptionValue>> option(std::string_view name) const {
        auto it = opts.find(name);
        if (it == opts.end()) return std::nullopt;
        return std::cref(it->second);
    }
    [[nodiscard]] OptionValue option_or_empty(std::string_view name) const {
        auto opt = option(name);
        return opt ? opt->get() : OptionValue{};
    }
    [[nodiscard]] bool is_flag_set(std::string_view name) const {
        return option_or_empty(name).is_set();
    }
    [[nodiscard]] std::optional<std::string> value(std::string_view name) const {
        auto opt = option(name);
        if (opt && opt->get().is_set()) return opt->get().value();
        for (std::size_t idx = 0; idx < positional_names_.size() && idx < positionals.size(); ++idx)
            if (positional_names_[idx] == name) return positionals[idx];
        return std::nullopt;
    }
    /// An option's (or a named positional's) value as a number: what an option
    /// declared with value_type<Number>() takes, already checked by the parse.
    /// Nothing when it was not given — or, for a value no declaration checked,
    /// when it is not wholly a number of that type.
    template <class Number>
        requires(std::is_arithmetic_v<Number> && !std::is_same_v<Number, bool>)
    [[nodiscard]] std::optional<Number> value_as(std::string_view name) const {
        const auto text = value(name);
        if (!text) return std::nullopt;
        Number number{};
        const char* first = text->data();
        const char* last = first + text->size();
        const auto [end, error] = std::from_chars(first, last, number);
        if (error != std::errc{} || end != last) return std::nullopt;
        return number;
    }
    [[nodiscard]] std::string positional(std::size_t index) const {
        return index < positionals.size() ? positionals[index] : std::string{};
    }
    [[nodiscard]] std::string positional_or(std::size_t index, std::string_view default_value) const {
        return index < positionals.size() ? positionals[index] : std::string{default_value};
    }
    [[nodiscard]] std::size_t positional_count() const { return positionals.size(); }
    [[nodiscard]] bool has_subcommand() const { return !subcommand_name_.empty(); }
    [[nodiscard]] std::string_view subcommand_name() const { return std::string_view(subcommand_name_); }
    [[nodiscard]] std::optional<std::reference_wrapper<const ParsedArgs>> subcommand() const {
        if (subcommand_matches) return std::cref(*subcommand_matches);
        return std::nullopt;
    }
};

export using ParseResult = std::expected<ParsedArgs, ParseError>;

} // namespace mcpplibs::cmdline::detail
