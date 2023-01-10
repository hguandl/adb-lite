#include <map>

#include "error.hpp"

namespace adb {

class adb_errors_category : public std::error_category {
    const char* name() const noexcept override {
        return "adb::adb_errors_category";
    }

    std::string message(int condition) const override {
        switch (condition) {
        case adb_errors::server_unavailable:
            return "adb server is unavailable";
        case adb_errors::push_unacknowledged:
            return "adb push was not acknowledged";
        default: {
            const auto& it = m_messages.find(condition);
            if (it != m_messages.end()) {
                return it->second;
            }
            return "unknown adb error";
        }
        };
    }

    adb_errors_category() = default;

    std::map<int, std::string> m_messages;

  public:
    int add_dynamic_error(std::string_view message) {
        const auto hash = std::hash<std::string_view>()(message);
        const auto key = static_cast<int>(hash);
        if (m_messages.find(key) == m_messages.end()) {
            m_messages[key] = std::string(message);
        }
        return key;
    }

    static adb_errors_category& instance() {
        static adb_errors_category category;
        return category;
    }
};

std::error_code make_error_code(adb_errors condition) {
    return std::error_code(condition, adb_errors_category::instance());
}

std::error_code make_error_code(const std::string_view message) {
    auto condition = adb_errors_category::instance().add_dynamic_error(message);
    return std::error_code(condition, adb_errors_category::instance());
}

} // namespace adb
