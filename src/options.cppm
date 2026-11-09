module;

export module mcpplibs.cmdline:options;

import std;

namespace mcpplibs::cmdline::detail {

export struct Arg {
    std::string name;
    std::string help_;
    bool required_ = false;
    std::string default_val;

    constexpr Arg(std::string_view name) : name(name) {}

    [[nodiscard]] constexpr Arg& help(std::string_view help_text) {
        help_ = help_text;
        return *this;
    }
    [[nodiscard]] constexpr Arg& required(bool required = true) {
        required_ = required;
        return *this;
    }
    [[nodiscard]] constexpr Arg& default_value(std::string_view value) {
        default_val = value;
        return *this;
    }
};

export struct Option {
    char short_ = '\0';
    std::string long_name;
    std::string help_;
    bool takes_value_ = false;
    bool multiple_ = false;
    bool global_ = false;
    std::string value_name_;
    // A value's check, declared with value_type<T>(): a value it refuses is a parse error.
    bool (*value_check_)(std::string_view) = nullptr;
    std::string value_kind_;  // what it takes, for that error: "a number", "an integer" …

    constexpr Option(std::string_view long_opt) : long_name(long_opt) {}
    constexpr Option(char short_char) : short_(short_char) {}

    [[nodiscard]] constexpr Option& short_name(char short_char) {
        short_ = short_char;
        return *this;
    }
    [[nodiscard]] Option& long_opt(std::string_view long_opt) {
        long_name = long_opt;
        return *this;
    }
    [[nodiscard]] constexpr Option& help(std::string_view help_text) {
        help_ = help_text;
        return *this;
    }
    [[nodiscard]] constexpr Option& takes_value(bool takes = true) {
        takes_value_ = takes;
        return *this;
    }
    [[nodiscard]] constexpr Option& multiple(bool multiple = true) {
        multiple_ = multiple;
        return *this;
    }
    [[nodiscard]] constexpr Option& global(bool global = true) {
        global_ = global;
        return *this;
    }
    [[nodiscard]] constexpr Option& value_name(std::string_view value_name) {
        value_name_ = value_name;
        return *this;
    }
    /// The option takes a number of type `Number`, read with std::from_chars:
    /// `--rate 2.5` with value_type<double>(), `--frames 60` with value_type<int>().
    /// A value that is not wholly one (`12px`, `-1` for an unsigned, out of
    /// range) is refused when the command line is parsed; afterwards
    /// ParsedArgs::value_as<Number>() reads it.
    template <class Number>
        requires(std::is_arithmetic_v<Number> && !std::is_same_v<Number, bool>)
    [[nodiscard]] Option& value_type() {
        takes_value_ = true;
        value_check_ = [](std::string_view text) {
            Number number{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
            return error == std::errc{} && end == text.data() + text.size();
        };
        value_kind_ = std::is_floating_point_v<Number> ? "a number"
                      : std::is_unsigned_v<Number>    ? "a whole number, not negative"
                                                      : "a whole number";
        if (value_name_.empty()) value_name_ = std::is_floating_point_v<Number> ? "NUMBER" : "N";
        return *this;
    }

    [[nodiscard]] bool matches_short(char short_char) const { return short_ && short_ == short_char; }
    [[nodiscard]] bool matches_long(std::string_view long_opt) const {
        return !long_name.empty() && long_name == long_opt;
    }
    [[nodiscard]] bool matches(std::string_view token) const {
        if (token.size() == 2 && token[0] == '-' && token[1] == short_) return true;
        if (token.size() > 2 && token.starts_with("--") && token.substr(2) == long_name) return true;
        return false;
    }
};

} // namespace mcpplibs::cmdline::detail
