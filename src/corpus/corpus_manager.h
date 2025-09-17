#pragma once

#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace winuzzf {

class CorpusManager {
public:
    CorpusManager();

    void SetCorpusDirectory(const std::string& dir);
    void SetMinimizationEnabled(bool enabled);

    void AddInput(const std::vector<uint8_t>& input);
    void LoadFromDirectory(const std::string& dir);
    void SaveToDirectory(const std::string& dir) const;

    std::vector<std::vector<uint8_t>> GetRandomInputs(std::size_t count);
    std::size_t GetCorpusSize() const;

private:
    std::string corpus_dir_;
    bool minimize_;
    mutable std::mutex mutex_;
    std::vector<std::vector<uint8_t>> corpus_;
    mutable std::mt19937 rng_;

    void SaveInputToFile(const std::string& filename, const std::vector<uint8_t>& input) const;
};

} // namespace winuzzf
