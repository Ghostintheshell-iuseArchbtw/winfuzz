#include "mutator.h"
#include <algorithm>
#include <cassert>

namespace winuzzf {

// Interesting values for mutations
static const uint8_t INTERESTING_8[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x20, 0x40, 0x7F, 0x80, 0x81, 0xFF
};

static const uint16_t INTERESTING_16[] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
    0x0010, 0x0020, 0x0040, 0x007F, 0x0080, 0x0081, 0x00FF, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x7FFF, 0x8000, 0x8001, 0xFFFF
};

static const uint32_t INTERESTING_32[] = {
    0x00000000, 0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005, 0x00000006, 0x00000007,
    0x00000008, 0x00000009, 0x0000000A, 0x0000000B, 0x0000000C, 0x0000000D, 0x0000000E, 0x0000000F,
    0x00000010, 0x00000020, 0x00000040, 0x0000007F, 0x00000080, 0x00000081, 0x000000FF, 0x00000100,
    0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00002000, 0x00004000, 0x00007FFF, 0x00008000,
    0x00008001, 0x0000FFFF, 0x00010000, 0x00020000, 0x00040000, 0x00080000, 0x00100000, 0x00200000,
    0x00400000, 0x00800000, 0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000,
    0x40000000, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFF
};

std::vector<uint8_t> Mutator::RandomMutate(const std::vector<uint8_t>& input, std::mt19937& rng) {
    if (input.empty()) {        // Generate random data
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t size = size_dist(rng);
        
        std::vector<uint8_t> result(size);
        std::uniform_int_distribution<unsigned int> byte_dist(0, 255);
        
        for (auto& byte : result) {
            byte = static_cast<uint8_t>(byte_dist(rng));
        }
        
        return result;
    }
    
    auto result = input;
    
    // Choose mutation type
    std::uniform_int_distribution<int> mutation_dist(0, 9);
    int mutation_type = mutation_dist(rng);
    
    switch (mutation_type) {
        case 0: // Bit flip
            if (!result.empty()) {
                size_t pos = ChooseOffset(rng, result.size() * 8);
                result = BitFlip(result, pos / 8, 1);
            }
            break;
            
        case 1: // Byte flip
            if (!result.empty()) {
                size_t pos = ChooseOffset(rng, result.size());
                result = ByteFlip(result, pos, 1);
            }
            break;
              case 2: // Arithmetic add
            if (!result.empty()) {
                size_t pos = ChooseOffset(rng, result.size());
                std::uniform_int_distribution<int> val_dist(-35, 35);
                result = ArithmeticAdd(result, pos, static_cast<int8_t>(val_dist(rng)));
            }
            break;
            
        case 3: // Insert byte
            {
                size_t pos = ChooseOffset(rng, result.size() + 1);
                uint8_t value = GenerateInterestingByte(rng);
                result = InsertByte(result, pos, value);
            }
            break;
            
        case 4: // Delete byte
            if (!result.empty()) {
                size_t pos = ChooseOffset(rng, result.size());
                result = DeleteByte(result, pos);
            }
            break;
            
        case 5: // Overwrite byte
            if (!result.empty()) {
                size_t pos = ChooseOffset(rng, result.size());
                uint8_t value = GenerateInterestingByte(rng);
                result = OverwriteByte(result, pos, value);
            }
            break;
              case 6: // Insert block
            {
                size_t pos = ChooseOffset(rng, result.size() + 1);
                size_t block_size = ChooseLength(rng, 256);
                std::vector<uint8_t> block(block_size);
                std::uniform_int_distribution<unsigned int> byte_dist(0, 255);
                for (auto& byte : block) {
                    byte = static_cast<uint8_t>(byte_dist(rng));
                }
                result = InsertBlock(result, pos, block);
            }
            break;
            
        case 7: // Delete block
            if (!result.empty()) {
                size_t pos = ChooseOffset(rng, result.size());
                size_t length = ChooseLength(rng, result.size() - pos);
                result = DeleteBlock(result, pos, length);
            }
            break;
            
        case 8: // Duplicate block
            if (!result.empty()) {
                size_t pos = ChooseOffset(rng, result.size());
                size_t length = ChooseLength(rng, result.size() - pos);
                result = DuplicateBlock(result, pos, length);
            }
            break;
            
        case 9: // Replace with interesting value
            if (result.size() >= 4) {
                size_t pos = ChooseOffset(rng, result.size() - 3);
                uint32_t value = GenerateInterestingDword(rng);
                memcpy(&result[pos], &value, sizeof(value));
            } else if (result.size() >= 2) {
                size_t pos = ChooseOffset(rng, result.size() - 1);
                uint16_t value = GenerateInterestingWord(rng);
                memcpy(&result[pos], &value, sizeof(value));
            } else if (!result.empty()) {
                size_t pos = ChooseOffset(rng, result.size());
                result[pos] = GenerateInterestingByte(rng);
            }
            break;
    }
    
    return result;
}

std::vector<uint8_t> Mutator::DeterministicMutate(const std::vector<uint8_t>& input, uint64_t iteration) {
    if (input.empty()) {
        return std::vector<uint8_t>{static_cast<uint8_t>(iteration & 0xFF)};
    }
    
    auto result = input;
    size_t pos = iteration % result.size();
    
    // Cycle through different mutation types
    switch (iteration % 4) {
        case 0: // Bit flip
            result = BitFlip(result, pos, 1);
            break;
        case 1: // Byte increment
            result = ArithmeticAdd(result, pos, 1);
            break;
        case 2: // Byte decrement
            result = ArithmeticSub(result, pos, 1);
            break;
        case 3: // XOR with iteration
            result[pos] ^= static_cast<uint8_t>(iteration & 0xFF);
            break;
    }
    
    return result;
}

std::vector<uint8_t> Mutator::DictionaryMutate(const std::vector<uint8_t>& input, 
                                              const std::vector<std::string>& dictionary, 
                                              std::mt19937& rng) {
    if (dictionary.empty()) {
        return RandomMutate(input, rng);
    }
    
    auto result = input;
    
    // Choose a random dictionary entry
    std::uniform_int_distribution<size_t> dict_dist(0, dictionary.size() - 1);
    const auto& dict_entry = dictionary[dict_dist(rng)];
    
    // Choose mutation type
    std::uniform_int_distribution<int> mutation_dist(0, 2);
    int mutation_type = mutation_dist(rng);
    
    std::vector<uint8_t> dict_bytes(dict_entry.begin(), dict_entry.end());
    
    switch (mutation_type) {
        case 0: // Replace random part with dictionary entry
            if (!result.empty()) {
                size_t pos = ChooseOffset(rng, result.size());
                size_t replace_len = std::min(dict_bytes.size(), result.size() - pos);
                std::copy(dict_bytes.begin(), dict_bytes.begin() + replace_len, result.begin() + pos);
            }
            break;
            
        case 1: // Insert dictionary entry
            {
                size_t pos = ChooseOffset(rng, result.size() + 1);
                result = InsertBlock(result, pos, dict_bytes);
            }
            break;
            
        case 2: // Append dictionary entry
            result.insert(result.end(), dict_bytes.begin(), dict_bytes.end());
            break;
    }
    
    return result;
}

std::vector<uint8_t> Mutator::HavocMutate(const std::vector<uint8_t>& input, std::mt19937& rng) {
    auto result = input;
    
    // Apply multiple random mutations
    std::uniform_int_distribution<int> num_mutations_dist(1, 16);
    int num_mutations = num_mutations_dist(rng);
    
    for (int i = 0; i < num_mutations; ++i) {
        result = RandomMutate(result, rng);
    }
    
    return result;
}

std::vector<uint8_t> Mutator::SpliceMutate(const std::vector<uint8_t>& input1, 
                                         const std::vector<uint8_t>& input2, 
                                         std::mt19937& rng) {
    if (input1.empty()) return input2;
    if (input2.empty()) return input1;
    
    // Choose split points
    std::uniform_int_distribution<size_t> split1_dist(0, input1.size());
    std::uniform_int_distribution<size_t> split2_dist(0, input2.size());
    
    size_t split1 = split1_dist(rng);
    size_t split2 = split2_dist(rng);
    
    std::vector<uint8_t> result;
    result.reserve(split1 + (input2.size() - split2));
    
    // First part from input1
    result.insert(result.end(), input1.begin(), input1.begin() + split1);
    
    // Second part from input2
    result.insert(result.end(), input2.begin() + split2, input2.end());
    
    return result;
}

// Individual mutation operations
std::vector<uint8_t> Mutator::BitFlip(const std::vector<uint8_t>& input, size_t pos, size_t count) {
    auto result = input;
    if (pos >= result.size()) return result;
    
    for (size_t i = 0; i < count && (pos + i) < result.size(); ++i) {
        result[pos + i] ^= (1 << (i % 8));
    }
    
    return result;
}

std::vector<uint8_t> Mutator::ByteFlip(const std::vector<uint8_t>& input, size_t pos, size_t count) {
    auto result = input;
    if (pos >= result.size()) return result;
    
    for (size_t i = 0; i < count && (pos + i) < result.size(); ++i) {
        result[pos + i] ^= 0xFF;
    }
    
    return result;
}

std::vector<uint8_t> Mutator::ArithmeticAdd(const std::vector<uint8_t>& input, size_t pos, int8_t value) {
    auto result = input;
    if (pos < result.size()) {
        result[pos] = static_cast<uint8_t>(static_cast<int16_t>(result[pos]) + value);
    }
    return result;
}

std::vector<uint8_t> Mutator::ArithmeticSub(const std::vector<uint8_t>& input, size_t pos, int8_t value) {
    auto result = input;
    if (pos < result.size()) {
        result[pos] = static_cast<uint8_t>(static_cast<int16_t>(result[pos]) - value);
    }
    return result;
}

std::vector<uint8_t> Mutator::InsertByte(const std::vector<uint8_t>& input, size_t pos, uint8_t value) {
    auto result = input;
    if (pos <= result.size()) {
        result.insert(result.begin() + pos, value);
    }
    return result;
}

std::vector<uint8_t> Mutator::DeleteByte(const std::vector<uint8_t>& input, size_t pos) {
    auto result = input;
    if (pos < result.size()) {
        result.erase(result.begin() + pos);
    }
    return result;
}

std::vector<uint8_t> Mutator::OverwriteByte(const std::vector<uint8_t>& input, size_t pos, uint8_t value) {
    auto result = input;
    if (pos < result.size()) {
        result[pos] = value;
    }
    return result;
}

std::vector<uint8_t> Mutator::InsertBlock(const std::vector<uint8_t>& input, size_t pos, const std::vector<uint8_t>& block) {
    auto result = input;
    if (pos <= result.size()) {
        result.insert(result.begin() + pos, block.begin(), block.end());
    }
    return result;
}

std::vector<uint8_t> Mutator::DeleteBlock(const std::vector<uint8_t>& input, size_t pos, size_t length) {
    auto result = input;
    if (pos < result.size()) {
        size_t end_pos = std::min(pos + length, result.size());
        result.erase(result.begin() + pos, result.begin() + end_pos);
    }
    return result;
}

std::vector<uint8_t> Mutator::DuplicateBlock(const std::vector<uint8_t>& input, size_t pos, size_t length) {
    auto result = input;
    if (pos < result.size()) {
        size_t end_pos = std::min(pos + length, result.size());
        std::vector<uint8_t> block(result.begin() + pos, result.begin() + end_pos);
        result.insert(result.begin() + end_pos, block.begin(), block.end());
    }
    return result;
}

size_t Mutator::ChooseLength(std::mt19937& rng, size_t input_size) {
    if (input_size == 0) return 1;
    std::uniform_int_distribution<size_t> dist(1, std::max(static_cast<size_t>(1), input_size / 4));
    return dist(rng);
}

size_t Mutator::ChooseOffset(std::mt19937& rng, size_t input_size) {
    if (input_size == 0) return 0;
    std::uniform_int_distribution<size_t> dist(0, input_size - 1);
    return dist(rng);
}

uint8_t Mutator::GenerateInterestingByte(std::mt19937& rng) {
    std::uniform_int_distribution<size_t> dist(0, sizeof(INTERESTING_8) / sizeof(INTERESTING_8[0]) - 1);
    return INTERESTING_8[dist(rng)];
}

uint16_t Mutator::GenerateInterestingWord(std::mt19937& rng) {
    std::uniform_int_distribution<size_t> dist(0, sizeof(INTERESTING_16) / sizeof(INTERESTING_16[0]) - 1);
    return INTERESTING_16[dist(rng)];
}

uint32_t Mutator::GenerateInterestingDword(std::mt19937& rng) {
    std::uniform_int_distribution<size_t> dist(0, sizeof(INTERESTING_32) / sizeof(INTERESTING_32[0]) - 1);
    return INTERESTING_32[dist(rng)];
}

} // namespace winuzzf
