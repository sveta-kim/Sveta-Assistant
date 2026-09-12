#include "memory/DailyMemory.h"

#include <windows.h>

#include <filesystem>
#include <fstream>

#include <format>

#include <nlohmann/json.hpp>

#include "core/Logger.h"
#include "core/Paths.h"

namespace sveta::memory {

namespace {

std::filesystem::path DailyDir() {
    std::filesystem::path dir = core::LocalAppDataDir() / "memory" / "daily";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string TodayDateString() {
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    return std::format("{:04}-{:02}-{:02}", localTime.wYear, localTime.wMonth, localTime.wDay);
}

std::string CurrentTimeString() {
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    return std::format("{:02}:{:02}", localTime.wHour, localTime.wMinute);
}

std::filesystem::path PathForDate(const std::string& date) {
    return DailyDir() / (date + ".json");
}

} // namespace

DailyMemory::DailyMemory(std::string date) : date_(std::move(date)) {}

DailyMemory DailyMemory::LoadToday() {
    DailyMemory memory(TodayDateString());

    std::ifstream file(PathForDate(memory.date_));
    if (!file.is_open()) {
        return memory; // no entries yet today -- normal for the first run of the day
    }

    try {
        nlohmann::json parsed;
        file >> parsed;
        for (const auto& entry : parsed.value("entries", nlohmann::json::array())) {
            memory.entries_.push_back({entry.value("time", ""), entry.value("text", "")});
        }
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse daily memory file: ") + e.what());
    }

    return memory;
}

void DailyMemory::AppendEntry(const std::string& text) {
    entries_.push_back({CurrentTimeString(), text});
    Save();
}

void DailyMemory::Save() const {
    nlohmann::json out;
    out["date"] = date_;
    nlohmann::json entries = nlohmann::json::array();
    for (const auto& entry : entries_) {
        entries.push_back({{"time", entry.time}, {"text", entry.text}});
    }
    out["entries"] = entries;

    std::ofstream file(PathForDate(date_));
    if (!file.is_open()) {
        core::Logger::Error("Failed to open daily memory file for writing");
        return;
    }
    file << out.dump(4);
}

void DailyMemory::PruneOlderThan(int days) {
    const std::string cutoff = [&] {
        SYSTEMTIME localTime{};
        GetLocalTime(&localTime);
        FILETIME fileTime{};
        SystemTimeToFileTime(&localTime, &fileTime);
        ULARGE_INTEGER asLarge{};
        asLarge.LowPart = fileTime.dwLowDateTime;
        asLarge.HighPart = fileTime.dwHighDateTime;
        // FILETIME ticks are 100ns; subtract `days` worth of them.
        asLarge.QuadPart -= static_cast<ULONGLONG>(days) * 24ULL * 60ULL * 60ULL * 10'000'000ULL;
        fileTime.dwLowDateTime = asLarge.LowPart;
        fileTime.dwHighDateTime = asLarge.HighPart;
        SYSTEMTIME cutoffTime{};
        FileTimeToSystemTime(&fileTime, &cutoffTime);
        return std::format("{:04}-{:02}-{:02}", cutoffTime.wYear, cutoffTime.wMonth, cutoffTime.wDay);
    }();

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(DailyDir(), ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string stem = entry.path().stem().string(); // "YYYY-MM-DD"
        if (stem.size() == 10 && stem < cutoff) {
            std::filesystem::remove(entry.path(), ec);
        }
    }
}

int DailyMemory::CountDailyFiles() {
    int count = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(DailyDir(), ec)) {
        if (entry.is_regular_file()) {
            ++count;
        }
    }
    return count;
}

} // namespace sveta::memory
