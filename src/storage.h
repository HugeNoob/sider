#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using TimeStamp = std::optional<std::chrono::time_point<std::chrono::system_clock>>;

template <typename T>
class StorageValue {
   public:
    StorageValue(const T& value, const TimeStamp& expiry) : value(value), expiry(expiry) {}

    T get_value() const {
        return this->value;
    };

    TimeStamp get_expiry() const {
        return this->expiry;
    };

   private:
    T value;
    TimeStamp expiry;
};

using StringValue = StorageValue<std::string>;

using Stream = std::vector<std::pair<std::string, std::string>>;
using StreamValue = StorageValue<Stream>;

using StorageValueVariants = std::variant<StringValue, StreamValue>;

class Storage;
using StoragePtr = std::shared_ptr<Storage>;

class Storage {
   public:
    using Store = std::unordered_map<std::string, std::variant<StringValue, StreamValue>>;
    using StoreView = const Store&;

    StorageValueVariants get(std::string_view key);

    void set(std::string_view key, StorageValueVariants&& value);

    StoreView get_view() const;

    bool check_validity(std::string_view key);

   private:
    Store store;

    bool is_expired(const StorageValueVariants& val) const;
};
