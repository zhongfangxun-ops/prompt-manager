#pragma once
#include <filesystem>
#include <string>
#include <optional>
#include <vector>
#include <cstdint>
struct sqlite3;
using Id = std::int64_t;
struct Prompt {
    Id id;
    std::wstring title, body;
    std::vector<std::wstring> tags;
};
struct Tag {
    Id id;
    std::wstring name;
};
struct Summary {
    Id id;
    std::wstring title;
};
std::wstring trimmed(std::wstring_view text);
Id utcMillis();
class PromptStore {
  public:
    explicit PromptStore(const std::filesystem::path &path);
    ~PromptStore();
    PromptStore(const PromptStore &) = delete;
    PromptStore &operator=(const PromptStore &) = delete;
    Id save(const Prompt &draft);
    void erase(Id id);
    std::optional<Prompt> get(Id id);
    std::vector<Tag> tags();
    std::vector<Summary> query(Id tag, std::wstring_view search);
    void markUsed(Id id, Id time);

  private:
    sqlite3 *db{};
};
