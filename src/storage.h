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
    StorageValue(T const& value, TimeStamp const& expiry) : value(value), expiry(expiry) {}

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
    using StoreView = Store const&;

    StorageValueVariants get(std::string const& key);

    void set(std::string const& key, StorageValueVariants&& value);

    StoreView get_view() const;

    bool check_validity(std::string const& key);

   private:
    Store store;
};
