#include "storage.h"

bool Storage::is_expired(const StorageValueVariants& val) const {
    return std::visit(
        [](const auto& v) -> bool {
            const auto& expiry = v.get_expiry();
            return expiry.has_value() && std::chrono::system_clock::now() >= expiry.value();
        },
        val);
}

StorageValueVariants Storage::get(std::string_view key) {
    auto it = this->store.find(std::string{key});
    if (it == this->store.end()) {
        throw std::out_of_range("Key not found");
    }

    if (is_expired(it->second)) {
        this->store.erase(it);
        throw std::out_of_range("Key expired");
    }
    return it->second;
};

void Storage::set(std::string_view key, StorageValueVariants&& value) {
    this->store.insert_or_assign(std::string{key}, std::move(value));
};

Storage::StoreView Storage::get_view() const {
    return this->store;
}

bool Storage::check_validity(std::string_view key) {
    auto it = this->store.find(std::string{key});
    if (it == this->store.end()) {
        return false;
    }

    if (is_expired(it->second)) {
        this->store.erase(it);
        return false;
    }
    return true;
}