// preprocessing.cpp
// Prototype preprocessing implementation - based on Python version

#include "preprocessing.hpp"

#include <cctype>
#include <string>
#include <optional>
#include <algorithm>

namespace preprocessing {

using Result = std::optional<std::string>;

namespace {

// Normalize whitespace - collapse newlines/tabs to space, trim
std::string normalize_whitespace(std::string s) {
    std::string result;
    result.reserve(s.size());
    
    bool last_was_space = true;
    for (char c : s) {
        if (c == '\n' || c == '\r' || c == '\t') {
            c = ' ';
        }
        if (c == ' ') {
            if (!last_was_space) {
                result.push_back(c);
                last_was_space = true;
            }
        } else {
            result.push_back(c);
            last_was_space = false;
        }
    }
    
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    
    return result;
}

// Remove __declspec(...)
std::string remove_declspec(std::string s) {
    std::string result;
    result.reserve(s.size());
    
    size_t i = 0;
    while (i < s.size()) {
        // Look for __declspec
        if (i + 10 <= s.size() && s.compare(i, 10, "__declspec") == 0) {
            size_t j = i + 10;
            if (j < s.size() && s[j] == '(') {
                int paren_depth = 1;
                j++;
                while (j < s.size() && paren_depth > 0) {
                    if (s[j] == '(') paren_depth++;
                    else if (s[j] == ')') paren_depth--;
                    j++;
                }
                if (paren_depth == 0) {
                    result.push_back(' ');
                    i = j;
                    continue;
                }
            }
        }
        result.push_back(s[i]);
        i++;
    }
    
    return result;
}

// Remove DECLSPEC_ALIGN(...)
std::string remove_declspec_align(std::string s) {
    std::string result;
    result.reserve(s.size());
    
    size_t i = 0;
    while (i < s.size()) {
        // Look for DECLSPEC_ALIGN
        if (i + 14 <= s.size() && (s[i] == 'D' || s[i] == 'd') && 
            s.compare(i + 1, 13, "ECLSPEC_ALIGN") == 0) {
            size_t j = i + 14;
            if (j < s.size() && s[j] == '(') {
                int paren_depth = 1;
                j++;
                while (j < s.size() && paren_depth > 0) {
                    if (s[j] == '(') paren_depth++;
                    else if (s[j] == ')') paren_depth--;
                    j++;
                }
                if (paren_depth == 0) {
                    result.push_back(' ');
                    i = j;
                    continue;
                }
            }
        }
        result.push_back(s[i]);
        i++;
    }
    
    return result;
}

// Remove extern "C" block
std::string remove_extern_c(std::string s) {
    std::string result;
    result.reserve(s.size());
    
    size_t i = 0;
    while (i < s.size()) {
        // Look for extern
        if (i + 6 <= s.size() && s.compare(i, 6, "extern") == 0) {
            size_t j = i + 6;
            // Skip whitespace
            while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) j++;
            // Check for "C"
            if (j + 3 <= s.size() && s[j] == '"' && s[j + 1] == 'C' && s[j + 2] == '"') {
                j += 3;
                while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) j++;
                if (j < s.size() && s[j] == '{') {
                    // Find matching closing brace
                    int brace_depth = 1;
                    j++;
                    while (j < s.size() && brace_depth > 0) {
                        if (s[j] == '{') brace_depth++;
                        else if (s[j] == '}') brace_depth--;
                        j++;
                    }
                    if (brace_depth == 0) {
                        result.push_back(' ');
                        i = j;
                        continue;
                    }
                }
            }
        }
        result.push_back(s[i]);
        i++;
    }
    
    return result;
}

// Remove bracket annotations [in], [out], etc.
std::string remove_bracket_annotations(std::string s) {
    static const std::string_view annotations[] = {
        "in", "out", "in,out", "in,optional", "out,optional",
        "annotation", "retval", "unique", "range", "size_is", "length_is",
        "switch_type", "switch_is", "iid_is", "string", "ptr", "ref",
        "ignore", "optional", "source", "defaultvalue", "lcid",
        "helpstring", "helpcontext", "hidden", "id", "propget", "propput",
        "propputref", "readonly", "restricted", "vararg"
    };
    
    std::string result;
    result.reserve(s.size());
    
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '[') {
            size_t j = i + 1;
            int bracket_depth = 1;
            while (j < s.size() && bracket_depth > 0) {
                if (s[j] == '[') bracket_depth++;
                else if (s[j] == ']') bracket_depth--;
                j++;
            }
            
            if (bracket_depth == 0) {
                std::string content = s.substr(i + 1, j - i - 2);
                
                bool is_annotation = false;
                for (auto ann_sv : annotations) {
                    std::string lower_content;
                    lower_content.reserve(content.size());
                    for (char c : content) {
                        lower_content.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                    }
                    if (lower_content.find(ann_sv) != std::string::npos) {
                        is_annotation = true;
                        break;
                    }
                }
                
                if (is_annotation) {
                    result.push_back(' ');
                    i = j;
                    continue;
                }
            }
        }
        result.push_back(s[i]);
        i++;
    }
    
    return result;
}

// Remove SAL annotations like _In_(...), _Out_(...), etc.
std::string remove_sal_annotations(std::string s) {
    static const std::string_view sal_prefixes[] = {
        "_In_", "_Out_", "_Inout_", "_In_opt_", "_Out_opt_", "_Inout_opt_",
        "_In_reads_", "_In_reads_bytes_", "_In_reads_opt_", "_Out_writes_",
        "_Out_writes_bytes_", "_Out_writes_opt_", "_Inout_updates_",
        "_Inout_updates_bytes_", "_Outptr_", "_Outptr_opt_",
        "_Outptr_result_maybenull_", "_COM_Outptr_", "_COM_Outptr_opt_",
        "_Deref_out_", "_Deref_out_opt_", "_Reserved_", "_Success_",
        "_Check_return_", "_Must_inspect_result_", "_Post_maybez_",
        "_Null_terminated_", "_NullNull_terminated_"
    };
    
    std::string result;
    result.reserve(s.size());
    
    size_t i = 0;
    while (i < s.size()) {
        bool matched = false;
        
        for (auto prefix_sv : sal_prefixes) {
            size_t prefix_len = prefix_sv.size();
            if (i + prefix_len <= s.size()) {
                bool match = true;
                for (size_t k = 0; k < prefix_len && match; k++) {
                    char c1 = std::tolower(static_cast<unsigned char>(s[i + k]));
                    char c2 = std::tolower(static_cast<unsigned char>(prefix_sv[k]));
                    if (c1 != c2) match = false;
                }
                
                if (match) {
                    // Check if followed by (...)
                    if (i + prefix_len < s.size() && s[i + prefix_len] == '(') {
                        size_t j = i + prefix_len + 1;
                        int paren_depth = 1;
                        while (j < s.size() && paren_depth > 0) {
                            if (s[j] == '(') paren_depth++;
                            else if (s[j] == ')') paren_depth--;
                            j++;
                        }
                        
                        if (paren_depth == 0) {
                            result.push_back(' ');
                            i = j;
                            matched = true;
                            break;
                        }
                    }
                    
                    // Check word boundaries for standalone keyword
                    bool valid_start = (i == 0 || (!std::isalnum(static_cast<unsigned char>(s[i - 1])) && s[i - 1] != '_'));
                    bool valid_end = (i + prefix_len >= s.size() || (!std::isalnum(static_cast<unsigned char>(s[i + prefix_len])) && s[i + prefix_len] != '_'));
                    
                    if (valid_start && valid_end) {
                        result.push_back(' ');
                        i += prefix_len;
                        matched = true;
                        break;
                    }
                }
            }
        }
        
        if (!matched) {
            result.push_back(s[i]);
            i++;
        }
    }
    
    return result;
}

// Strip API prefixes and modifiers
std::string strip_words(std::string s) {
    static const std::string_view words[] = {
        "NTSYSAPI", "NTHALAPI", "NTKERNELAPI", "NTKRNLVISTAAPI",
        "WINBASEAPI", "WINADVAPI", "WINUSERAPI", "WINGDIAPI",
        "WINCRYPT32API", "WINSCARDAPI", "WINSPOOLAPI", "WINAPI_INLINE",
        "DECLSPEC_NORETURN", "DECLSPEC_NOINLINE", "DECLSPEC_DEPRECATED",
        "DECLSPEC_IMPORT", "DECLSPEC_EXPORT",
        "FORCEINLINE", "__forceinline", "__inline"
    };
    
    for (auto word_sv : words) {
        std::string word(word_sv);
        std::string lower_word;
        lower_word.reserve(word.size());
        for (char c : word) {
            lower_word.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        
        std::string lower_s;
        lower_s.reserve(s.size());
        for (char c : s) {
            lower_s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        
        size_t pos = 0;
        while ((pos = lower_s.find(lower_word, pos)) != std::string::npos) {
            bool valid_start = (pos == 0 || !std::isalnum(static_cast<unsigned char>(lower_s[pos - 1])) && lower_s[pos - 1] != '_');
            size_t word_end = pos + lower_word.size();
            bool valid_end = (word_end >= lower_s.size() || !std::isalnum(static_cast<unsigned char>(lower_s[word_end])) && lower_s[word_end] != '_');
            
            if (valid_start && valid_end) {
                s.erase(pos, lower_word.size());
                lower_s.erase(pos, lower_word.size());
            } else {
                pos++;
            }
        }
    }
    
    return s;
}

// Map calling conventions to canonical form
std::string map_calling_conventions(std::string s) {
    struct CCReplacement {
        std::string from;
        std::string to;
    };
    static const CCReplacement replacements[] = {
        {"NTAPI", "__stdcall"},
        {"WINAPI", "__stdcall"},
        {"CALLBACK", "__stdcall"},
        {"APIENTRY", "__stdcall"},
        {"PASCAL", "__stdcall"},
        {"NTFASTCALL", "__fastcall"},
        {"FASTCALL", "__fastcall"},
        {"CDECL", "__cdecl"},
        {"WINAPIV", "__cdecl"},
    };
    
    for (const auto& rep : replacements) {
        std::string lower_s;
        lower_s.reserve(s.size());
        for (char c : s) {
            lower_s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        std::string lower_from = rep.from;
        for (char& c : lower_from) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        
        size_t pos = 0;
        while ((pos = lower_s.find(lower_from, pos)) != std::string::npos) {
            bool valid_start = (pos == 0 || !std::isalnum(static_cast<unsigned char>(lower_s[pos - 1])) && lower_s[pos - 1] != '_');
            size_t end = pos + lower_from.size();
            bool valid_end = (end >= lower_s.size() || !std::isalnum(static_cast<unsigned char>(lower_s[end])) && lower_s[end] != '_');
            
            if (valid_start && valid_end) {
                s.replace(pos, rep.from.size(), rep.to);
                lower_s.replace(pos, lower_from.size(), rep.to);
            } else {
                pos++;
            }
        }
    }
    
    return s;
}

// Remove inline, static, extern keywords
std::string remove_storage_class(std::string s) {
    static const std::string_view keywords[] = {
        "inline", "static", "extern"
    };
    
    for (auto kw_sv : keywords) {
        std::string kw(kw_sv);
        std::string lower_kw;
        lower_kw.reserve(kw.size());
        for (char c : kw) {
            lower_kw.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        
        std::string lower_s;
        lower_s.reserve(s.size());
        for (char c : s) {
            lower_s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        
        size_t pos = 0;
        while ((pos = lower_s.find(lower_kw, pos)) != std::string::npos) {
            bool valid_start = (pos == 0 || !std::isalnum(static_cast<unsigned char>(lower_s[pos - 1])) && lower_s[pos - 1] != '_');
            size_t end = pos + lower_kw.size();
            bool valid_end = (end >= lower_s.size() || !std::isalnum(static_cast<unsigned char>(lower_s[end])) && lower_s[end] != '_');
            
            if (valid_start && valid_end) {
                s.erase(pos, kw.size());
                lower_s.erase(pos, kw.size());
            } else {
                pos++;
            }
        }
    }
    
    return s;
}

// Replace VOID with void
std::string replace_void(std::string s) {
    std::string result;
    result.reserve(s.size());
    
    size_t i = 0;
    while (i < s.size()) {
        if (i + 4 <= s.size()) {
            bool match = true;
            for (size_t k = 0; k < 4 && match; k++) {
                char c = s[i + k];
                if (c != "VOID"[k] && c != "void"[k]) match = false;
            }
            
            if (match) {
                bool valid_start = (i == 0 || !std::isalnum(static_cast<unsigned char>(s[i - 1])) && s[i - 1] != '_');
                size_t end = i + 4;
                bool valid_end = (end >= s.size() || !std::isalnum(static_cast<unsigned char>(s[end])) && s[end] != '_');
                
                if (valid_start && valid_end) {
                    result += "void";
                    i += 4;
                    continue;
                }
            }
        }
        result.push_back(s[i]);
        i++;
    }
    
    return result;
}

// Remove closing brace at end
std::string remove_closing_brace(std::string s) {
    // Find the last non-space position
    size_t end = s.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    
    // Check if it ends with }
    if (end > 0 && s[end - 1] == '}') {
        // Remove everything after and including the brace
        s = s.substr(0, end - 1);
    }
    
    return s;
}

} // anonymous namespace

Result preprocess_prototype(std::string_view input) {
    if (input.size() > 65536) {
        return std::nullopt;
    }
    
    if (input.empty()) {
        return std::string{};
    }
    
    try {
        std::string decl(input);
        
        // Normalize whitespace
        decl = normalize_whitespace(decl);
        
        // Remove extern "C" blocks
        decl = remove_extern_c(decl);
        
        // Remove __declspec(...)
        decl = remove_declspec(decl);
        
        // Remove DECLSPEC_ALIGN(...)
        decl = remove_declspec_align(decl);
        
        // Strip words (API prefixes, modifiers)
        decl = strip_words(decl);
        
        // Remove storage class keywords (inline, static, extern)
        decl = remove_storage_class(decl);
        
        // Map calling conventions
        decl = map_calling_conventions(decl);
        
        // Remove bracket annotations
        decl = remove_bracket_annotations(decl);
        
        // Remove SAL annotations
        decl = remove_sal_annotations(decl);
        
        // Normalize whitespace again
        decl = normalize_whitespace(decl);
        
        // Remove closing brace at end
        decl = remove_closing_brace(decl);
        
        // Replace VOID with void
        decl = replace_void(decl);
        
        // Normalize whitespace again
        decl = normalize_whitespace(decl);
        
        // Ensure trailing semicolon
        if (!decl.empty() && decl.back() != ';') {
            decl += ';';
        }
        
        return decl;
        
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace preprocessing