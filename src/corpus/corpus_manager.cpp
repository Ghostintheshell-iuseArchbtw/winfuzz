#include "corpus_manager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace winuzzf {

namespace {
std::vector<uint8_t> ReadAllBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
}

CorpusManager::CorpusManager()
    : corpus_dir_("corpus")
    , minimize_(true)
    , rng_(std::random_device{}()) {}

void CorpusManager::SetCorpusDirectory(const std::string& dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    corpus_dir_ = dir;
}

void CorpusManager::SetMinimizationEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    minimize_ = enabled;
    (void)minimize_;
}

void CorpusManager::AddInput(const std::vector<uint8_t>& input) {
    if (input.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    corpus_.push_back(input);
}

void CorpusManager::LoadFromDirectory(const std::string& dir) {
    std::filesystem::path directory = dir.empty() ? corpus_dir_ : dir;
    if (directory.empty() || !std::filesystem::exists(directory)) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto data = ReadAllBytes(entry.path());
        if (!data.empty()) {
            corpus_.push_back(std::move(data));
        }
    }
}

void CorpusManager::SaveToDirectory(const std::string& dir) const {
    std::filesystem::path directory = dir.empty() ? corpus_dir_ : dir;
    if (directory.empty()) {
        return;
    }

    std::filesystem::create_directories(directory);

    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t index = 0;
    for (const auto& input : corpus_) {
        auto filename = directory / ("input_" + std::to_string(index++) + ".bin");
        SaveInputToFile(filename.string(), input);
    }
}

std::vector<std::vector<uint8_t>> CorpusManager::GetRandomInputs(std::size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::vector<uint8_t>> result;
    if (corpus_.empty() || count == 0) {
        return result;
    }

    std::uniform_int_distribution<std::size_t> dist(0, corpus_.size() - 1);
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back(corpus_[dist(rng_)]);
    }
    return result;
}

std::size_t CorpusManager::GetCorpusSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return corpus_.size();
}

void CorpusManager::SaveInputToFile(const std::string& filename, const std::vector<uint8_t>& input) const {
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    if (!file) {
        return;
    }
    file.write(reinterpret_cast<const char*>(input.data()), static_cast<std::streamsize>(input.size()));
}

} // namespace winuzzf
