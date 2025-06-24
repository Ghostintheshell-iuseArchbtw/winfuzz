#pragma once

#include <vector>
#include <string>
#include <random>
#include <cstdint>

namespace winuzzf {

class Mutator {
public:
    // Random byte-level mutations
    static std::vector<uint8_t> RandomMutate(const std::vector<uint8_t>& input, std::mt19937& rng);
    
    // Deterministic mutations (bit flips, arithmetic, etc.)
    static std::vector<uint8_t> DeterministicMutate(const std::vector<uint8_t>& input, uint64_t iteration);
    
    // Dictionary-based mutations
    static std::vector<uint8_t> DictionaryMutate(const std::vector<uint8_t>& input, 
                                                const std::vector<std::string>& dictionary, 
                                                std::mt19937& rng);
    
    // Havoc mutations (multiple random mutations)
    static std::vector<uint8_t> HavocMutate(const std::vector<uint8_t>& input, std::mt19937& rng);
    
    // Splice mutations (combine two inputs)
    static std::vector<uint8_t> SpliceMutate(const std::vector<uint8_t>& input1, 
                                           const std::vector<uint8_t>& input2, 
                                           std::mt19937& rng);

private:
    // Individual mutation operations
    static std::vector<uint8_t> BitFlip(const std::vector<uint8_t>& input, size_t pos, size_t count);
    static std::vector<uint8_t> ByteFlip(const std::vector<uint8_t>& input, size_t pos, size_t count);
    static std::vector<uint8_t> ArithmeticAdd(const std::vector<uint8_t>& input, size_t pos, int8_t value);
    static std::vector<uint8_t> ArithmeticSub(const std::vector<uint8_t>& input, size_t pos, int8_t value);
    static std::vector<uint8_t> InsertByte(const std::vector<uint8_t>& input, size_t pos, uint8_t value);
    static std::vector<uint8_t> DeleteByte(const std::vector<uint8_t>& input, size_t pos);
    static std::vector<uint8_t> OverwriteByte(const std::vector<uint8_t>& input, size_t pos, uint8_t value);
    static std::vector<uint8_t> InsertBlock(const std::vector<uint8_t>& input, size_t pos, const std::vector<uint8_t>& block);
    static std::vector<uint8_t> DeleteBlock(const std::vector<uint8_t>& input, size_t pos, size_t length);
    static std::vector<uint8_t> DuplicateBlock(const std::vector<uint8_t>& input, size_t pos, size_t length);
    
    // Utility functions
    static size_t ChooseLength(std::mt19937& rng, size_t input_size);
    static size_t ChooseOffset(std::mt19937& rng, size_t input_size);
    static uint8_t GenerateInterestingByte(std::mt19937& rng);
    static uint16_t GenerateInterestingWord(std::mt19937& rng);
    static uint32_t GenerateInterestingDword(std::mt19937& rng);
};

// Grammar-based mutator for structured inputs
class GrammarMutator {
public:
    struct Rule {
        std::string name;
        std::vector<std::string> productions;
    };
    
    explicit GrammarMutator(const std::vector<Rule>& grammar);
    
    std::vector<uint8_t> Generate(std::mt19937& rng, size_t max_depth = 10);
    std::vector<uint8_t> Mutate(const std::vector<uint8_t>& input, std::mt19937& rng);
    
private:
    std::vector<Rule> grammar_;
    std::string GenerateFromRule(const std::string& rule_name, std::mt19937& rng, size_t depth);
};

} // namespace winuzzf
