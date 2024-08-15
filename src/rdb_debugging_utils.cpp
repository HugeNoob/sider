#include <bitset>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

uint8_t hexToByte(const std::string& hex) {
    return static_cast<uint8_t>(std::stoi(hex, nullptr, 16));
}

std::vector<uint8_t> hexStringToBytes(std::string_view hexString) {
    std::vector<uint8_t> bytes;
    std::istringstream stream(std::string{hexString});
    std::string hex;
    while (stream >> hex) {
        bytes.push_back(hexToByte(hex));
    }
    return bytes;
}

void writeBinaryFile(std::string_view filePath, std::string_view hexString) {
    std::vector<uint8_t> bytes = hexStringToBytes(hexString);

    std::ofstream outFile(filePath.data(), std::ios::binary);
    if (!outFile) {
        std::cerr << "Could not open the file for writing." << std::endl;
        return;
    }

    outFile.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    outFile.close();
}

int determine_type(uint8_t c) {
    std::bitset<4> bs(c);
    std::cout << bs.to_string() << std::endl;
    int last_bit = bs[3], second_last_bit = bs[2];

    // Supposed to check "first two" bits aka leftmost, but this is flipped
    // 0x8 gives 0 0 0 1
    if (last_bit == 0 && second_last_bit == 0) {
        std::cout << "length 2" << std::endl;
        return 2;
    } else if (last_bit == 0 && second_last_bit == 1) {
        std::cout << "length 4" << std::endl;
        return 4;
    } else if (last_bit == 1 && second_last_bit == 0) {
        std::cout << "length 10" << std::endl;
        return 10;
    } else {
        std::cout << "string encoding?" << std::endl;
        return -1;
    }
}

void parse_size(std::stringstream& ss) {
    std::cout << "===" << std::endl;

    uint8_t c;
    ss >> c;
    int x = determine_type(c);

    std::string s;
    s.push_back(c);
    for (int i = 1; i < x; i++) {
        ss >> c;
        s.push_back(c);
    }
    std::cout << "original: " << s << std::endl;

    uint64_t bitmask = (uint64_t(1) << (x * 4 - 2)) - 1;
    std::cout << "bitmask: " << bitmask << std::endl;

    uint64_t val;
    std::istringstream iss(s);
    iss >> std::hex >> val;
    std::cout << "val: " << val << std::endl;

    uint64_t res = val & bitmask;
    std::cout << "res: " << res << std::endl;
}

int main() {
    constexpr const std::string_view hexString =
        "52 45 44 49 53 30 30 30 33 fa 0a 72 65 64 69 73 "
        "2d 62 69 74 73 c0 40 fa 09 72 65 64 69 73 2d 76 "
        "65 72 05 37 2e 32 2e 30 fe 00 fb 03 03 fc 00 0c "
        "28 8a c7 01 00 00 00 06 6f 72 61 6e 67 65 06 6f "
        "72 61 6e 67 65 fc 00 9c ef 12 7e 01 00 00 00 05 "
        "67 72 61 70 65 06 62 61 6e 61 6e 61 fc 00 0c 28 "
        "8a c7 01 00 00 00 09 62 6c 75 65 62 65 72 72 79 "
        "05 6d 61 6e 67 6f ff c1 fd c1 7f b8 2e 3f 50 0a ";

    constexpr const std::string_view filePath = "dump.rdb";
    writeBinaryFile(filePath, hexString);
    return 0;
}
