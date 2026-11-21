#include "library/PromptStore.h"
#include <fstream>
#include <stdexcept>
#include <windows.h>

namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void write(const std::filesystem::path &path, const std::string &bytes) {
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << bytes;
}
std::string backup(const std::string &items) {
    return R"({"format":"gemini-voyager.prompts.v1","items":[)" + items + "]}";
}
} // namespace

int main() {
    auto dir =
        std::filesystem::temp_directory_path() /
        ("prompt-import-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(utcMillis()));
    std::filesystem::create_directory(dir);
    auto path = dir / L"备份.json";
    auto database = dir / "library.sqlite";
    const std::string first =
        R"({"id":"alpha","name":"同名","text":"  <p>正文😀</p>\r\n\n","tags":["标签","Study"],"createdAt":100,"updatedAt":200})";
    const std::string second = R"({"id":"beta","name":"同名","text":"Second","tags":[],"createdAt":80})";
    const auto good = "\xef\xbb\xbf" + backup(first + "," + second);
    Id imported;
    {
        PromptStore store(database);
        store.save({0, L"原有", L"手动保存", {L"已有"}});
        write(path, good);
        auto r = store.importVoyager(path);
        check(r.added == 2 && r.skipped == 0, "first import counts");
        auto rows = store.query(0, L"同名");
        check(rows.size() == 2, "distinct source IDs may share a title");
        imported = rows.front().id;
        auto p = store.get(imported).value();
        check(p.body == L"  <p>正文😀</p>\r\n\n", "preserve body and source chronology");
        check(p.tags == std::vector<std::wstring>{L"Study", L"标签"}, "preserve tags");
        store.save({imported, L"本地编辑", L"本地修改", {L"Study"}});
    }
    {
        PromptStore store(database);
        auto r = store.importVoyager(path);
        check(r.added == 0 && r.skipped == 2, "idempotent across restarts");
        check(store.get(imported)->body == L"本地修改", "preserve edits on reimport");
        std::string fresh = R"({"id":"new","name":"新条目","text":"新正文","tags":["新标签"]})";
        for (const auto &invalid :
             {backup(fresh + R"(,{"id":"bad","name":"坏","text":"正文","tags":[false]})"),
              backup(fresh + ",null"),
              backup(fresh + R"(,{"id":"bad","name":"坏","text":"first\u0000second"})"),
              backup(fresh + R"(,{"id":"bad","name":"坏","text":17})"),
              backup(fresh) + std::string(1, '\0') + "junk",
              backup(R"({"id":"invalid","name":"name","text":")" + std::string(1, '\xff') + R"("})"),
              std::string(R"({"format":"wrong","items":[]})"), std::string("{")}) {
            write(path, invalid);
            bool failed = false;
            try {
                store.importVoyager(path);
            } catch (const std::invalid_argument &) {
                failed = true;
            }
            check(failed && store.query(0, {}).size() == 3 && store.tags().size() == 2,
                  "invalid backup rolls back all prompts and tags");
        }
        write(path, backup(fresh + "," + fresh + "," + first));
        r = store.importVoyager(path);
        check(r.added == 1 && r.skipped == 2, "mixed duplicate and new items after rollback");
        store.erase(imported);
        write(path, good);
        r = store.importVoyager(path);
        check(r.added == 1 && r.skipped == 1, "deleted imported entry can be restored");
    }
    std::filesystem::remove(path);
    std::filesystem::remove(database);
    std::filesystem::remove(dir);
}
