#include "storage.h"

StoragePtr Storage::make_storage() {
    struct make_shared_enabler : public Storage {};
    return std::make_shared<make_shared_enabler>();
}

StorageValueVariants Storage::get(std::string const& key) {
    auto it = this->store.find(key);
    if (it == this->store.end()) {
        throw std::out_of_range("Key not found");
    }
    auto& val = it->second;

    bool expired = std::visit(
        [](const auto& v) -> bool {
            const auto& expiry = v.get_expiry();
            return expiry.has_value() && std::chrono::system_clock::now() >= expiry.value();
        },
        val);

    if (expired) {
        this->store.erase(it);
        throw std::out_of_range("Key expired");
    }
    return val;
};

void Storage::set(std::string const& key, StorageValueVariants&& value) {
    this->store.insert_or_assign(key, std::move(value));
};

Storage::StoreView Storage::get_view() const {
    return this->store;
}

bool Storage::check_validity(std::string const& key) {
    auto it = this->store.find(key);
    if (it == this->store.end()) {
        return false;
    }
    auto& val = it->second;

    bool expired = std::visit(
        [](const auto& v) -> bool {
            const auto& expiry = v.get_expiry();
            return expiry.has_value() && std::chrono::system_clock::now() >= expiry.value();
        },
        val);

    if (expired) {
        this->store.erase(it);
        return false;
    }
    return true;
}