#pragma once

#include <string>
#include <vector>

namespace structura_license {

// Escapa caracteres para JSON seguro
inline std::string EscapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out += buf;
            } else {
                out += c;
            }
            break;
        }
    }
    return out;
}

// Localiza o valor de `key` SOMENTE no nivel superior do objeto JSON: ignora ocorrencias dentro de
// strings e de objetos/arrays aninhados (uma chave extra num valor nao engana mais o extrator).
// Retorna o indice do primeiro caractere do valor ou npos.
inline size_t JsonFindTopLevelValue(const std::string& json, const std::string& key) {
    size_t i = json.find('{');
    if (i == std::string::npos) return std::string::npos;
    int depth = 0;
    for (; i < json.size(); ++i) {
        char c = json[i];
        if (c == '{' || c == '[') { ++depth; continue; }
        if (c == '}' || c == ']') { --depth; if (depth <= 0) break; continue; }
        if (c != '"') continue;
        // string: le ate a aspa de fechamento respeitando escapes
        size_t start = ++i;
        std::string name;
        bool esc = false;
        for (; i < json.size(); ++i) {
            if (esc) { name += json[i]; esc = false; }
            else if (json[i] == '\\') esc = true;
            else if (json[i] == '"') break;
            else name += json[i];
        }
        (void)start;
        if (depth != 1) continue;
        size_t j = json.find_first_not_of(" \t\r\n", i + 1);
        if (j == std::string::npos || json[j] != ':') continue;   // era um valor string, nao uma chave
        if (name == key) {
            return json.find_first_not_of(" \t\r\n", j + 1);
        }
    }
    return std::string::npos;
}

// Extrator estrito (apenas chaves de nivel superior) de valores em JSON (sem dependência externa)
inline std::string JsonGetString(const std::string& json, const std::string& key) {
    size_t pos = JsonFindTopLevelValue(json, key);
    if (pos == std::string::npos || json[pos] != '"') return "";

    size_t start = pos + 1;
    size_t end = start;
    std::string out;
    bool escaped = false;
    while (end < json.size()) {
        char c = json[end];
        if (escaped) {
            switch (c) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            default: out += c; break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return out;
        } else {
            out += c;
        }
        ++end;
    }
    return "";
}

inline bool JsonGetBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    size_t pos = JsonFindTopLevelValue(json, key);
    if (pos == std::string::npos) return defaultVal;

    if (json.compare(pos, 4, "true") == 0) return true;
    if (json.compare(pos, 5, "false") == 0) return false;
    return defaultVal;
}

inline int JsonGetInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    size_t pos = JsonFindTopLevelValue(json, key);
    if (pos == std::string::npos) return defaultVal;

    // Se for string ou número
    if (json[pos] == '"') {
        std::string s = JsonGetString(json, key);
        try { return static_cast<int>(std::stoll(s)); } catch (...) { return defaultVal; }
    }

    try {
        return static_cast<int>(std::stoll(json.substr(pos)));
    } catch (...) {
        return defaultVal;
    }
}

// Decodifica Base64 padrão e Base64URL
inline bool Base64Decode(const std::string& in, std::vector<uint8_t>& out) {
    out.clear();
    uint32_t acc = 0;
    int bits = 0;
    for (char c : in) {
        if (c == '=') break;
        int v = -1;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+' || c == '-') v = 62;
        else if (c == '/' || c == '_') v = 63;
        else if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        else return false;
        acc = (acc << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>(acc >> bits));
            acc &= (1u << bits) - 1;
        }
    }
    return true;
}

} // namespace structura_license
