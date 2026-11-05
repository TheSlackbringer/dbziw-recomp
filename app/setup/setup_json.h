#pragma once
// The little JSON the launcher's own small records need (install.json, installations.json). Nothing else reads them.
// parse() throws std::runtime_error on anything malformed; the callers treat that as a damaged record.
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace dbziw::setup::json {

struct Value
{
    enum class Kind { Null, Bool, Number, String, Array, Object } kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<Value> items;
    std::map<std::string, Value> members;

    bool isString() const { return kind == Kind::String; }
    bool isArray() const { return kind == Kind::Array; }
    bool isObject() const { return kind == Kind::Object; }
    bool isNumber() const { return kind == Kind::Number; }
    const Value *find(const std::string &key) const
    {
        const auto it = members.find(key);
        return it == members.end() ? nullptr : &it->second;
    }
};

namespace detail {
class Parser
{
public:
    explicit Parser(const std::string &text) : m_text(text) {}
    Value document()
    {
        Value value = parse(0);
        space();
        if (m_at != m_text.size())
            throw std::runtime_error("text after the value");
        return value;
    }

private:
    const std::string &m_text;
    size_t m_at = 0;

    void space()
    {
        while (m_at < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_at])))
            ++m_at;
    }
    char peek() const
    {
        if (m_at >= m_text.size())
            throw std::runtime_error("unexpected end");
        return m_text[m_at];
    }
    bool word(const char *text)
    {
        const std::string expected(text);
        if (m_text.compare(m_at, expected.size(), expected) != 0)
            return false;
        m_at += expected.size();
        return true;
    }
    std::string string()
    {
        ++m_at;
        std::string out;
        for (;;)
        {
            const char c = peek();
            ++m_at;
            if (c == '"')
                return out;
            if (static_cast<unsigned char>(c) < 0x20)
                throw std::runtime_error("control character in a string");
            if (c != '\\')
            {
                out += c;
                continue;
            }
            const char e = peek();
            ++m_at;
            switch (e)
            {
            case '"': case '\\': case '/': out += e; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            default: throw std::runtime_error("unsupported escape");
            }
        }
    }
    Value parse(int depth)
    {
        if (depth > 8)
            throw std::runtime_error("nested too deeply");
        space();
        Value value;
        const char c = peek();
        if (c == '{')
        {
            ++m_at;
            value.kind = Value::Kind::Object;
            space();
            if (peek() == '}')
                return ++m_at, value;
            for (;;)
            {
                space();
                if (peek() != '"')
                    throw std::runtime_error("expected a key");
                std::string key = string();
                space();
                if (peek() != ':')
                    throw std::runtime_error("expected a colon");
                ++m_at;
                value.members[std::move(key)] = parse(depth + 1);
                space();
                const char next = peek();
                ++m_at;
                if (next == '}')
                    return value;
                if (next != ',')
                    throw std::runtime_error("expected a comma");
            }
        }
        if (c == '[')
        {
            ++m_at;
            value.kind = Value::Kind::Array;
            space();
            if (peek() == ']')
                return ++m_at, value;
            for (;;)
            {
                value.items.push_back(parse(depth + 1));
                space();
                const char next = peek();
                ++m_at;
                if (next == ']')
                    return value;
                if (next != ',')
                    throw std::runtime_error("expected a comma");
            }
        }
        if (c == '"')
        {
            value.kind = Value::Kind::String;
            value.text = string();
            return value;
        }
        if (word("true") || word("false"))
        {
            value.kind = Value::Kind::Bool;
            value.boolean = m_text[m_at - 1] == 'e' && m_text[m_at - 2] == 'u';
            return value;
        }
        if (word("null"))
            return value;
        size_t used = 0;
        value.kind = Value::Kind::Number;
        try
        {
            value.number = std::stod(m_text.substr(m_at, 32), &used);
        }
        catch (const std::exception &)
        {
            throw std::runtime_error("expected a value");
        }
        m_at += used;
        return value;
    }
};
} // namespace detail

inline Value parse(const std::string &text) { return detail::Parser(text).document(); }

inline std::string quote(const std::string &text)
{
    std::string out = "\"";
    for (const char c : text)
    {
        switch (c)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c;
        }
    }
    return out + "\"";
}

} // namespace dbziw::setup::json
