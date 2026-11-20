#include "library/PromptStore.h"
#include <sqlite3.h>
#include <stdexcept>
#include <filesystem>
#include <windows.h>
void check(bool condition) {
    if (!condition)
        throw std::runtime_error("store assertion failed");
}
int main() {
    auto path = std::filesystem::temp_directory_path() /
                ("prompt-store-" + std::to_string(GetCurrentProcessId()) + ".sqlite");
    Id id;
    const std::wstring body = L"\t中😀\r\n  code\n\n";
    {
        PromptStore s(path);
        id = s.save({0, L"测试", body, {L" 写作 ", L"Study", L"study"}});
    }
    {
        PromptStore s(path);
        auto p = s.get(id).value();
        check(p.body == body && p.tags.size() == 2);
        auto b = s.save({0, L"测试", L"共同", {L"写作"}});
        s.save({id, L"编辑", body, {L"写作"}});
        s.save({id, L"编辑", body, {L"写作"}});
        check(s.tags().size() == 1);
        sqlite3 *db{};
        sqlite3_open16(path.c_str(), &db);
        sqlite3_exec(
            db,
            "CREATE TRIGGER fail BEFORE INSERT ON prompt_tags BEGIN SELECT RAISE(ABORT,'forced'); END;",
            nullptr, nullptr, nullptr);
        sqlite3_close(db);
        bool failed = false;
        try {
            s.save({id, L"失败", L"new", {L"other"}});
        } catch (...) {
            failed = true;
        }
        check(failed && s.get(id)->title == L"编辑" && s.get(id)->body == body && s.tags().size() == 1);
        s.erase(id);
        check(s.get(b).has_value() && s.tags().size() == 1);
        try {
            s.save({0, L" \t ", body, {}});
            check(false);
        } catch (const std::invalid_argument &) {
        }
        s.erase(b);
        check(s.tags().empty());
    }
    std::filesystem::remove(path);
}
