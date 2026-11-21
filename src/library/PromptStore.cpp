#include "library/PromptStore.h"
#include <sqlite3.h>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <cwctype>
#include <windows.h>
namespace {
void verify(sqlite3 *db, int code) {
    if (code != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db));
}
void sql(sqlite3 *db, const char *command) {
    verify(db, sqlite3_exec(db, command, nullptr, nullptr, nullptr));
}
class Statement {
    sqlite3 *db;
    sqlite3_stmt *st{};

  public:
    Statement(sqlite3 *d, const char *q) : db(d) {
        verify(db, sqlite3_prepare_v2(db, q, -1, &st, nullptr));
    }
    ~Statement() { sqlite3_finalize(st); }
    void bind(int n, Id i) { verify(db, sqlite3_bind_int64(st, n, i)); }
    void bind(int n, std::string_view s) {
        verify(db, sqlite3_bind_text(st, n, s.empty() ? "" : s.data(), int(s.size()), SQLITE_TRANSIENT));
    }
    void bind(int n, std::wstring_view s) {
        verify(db, sqlite3_bind_text16(st, n, s.empty() ? L"" : s.data(),
                                       int(s.size() * sizeof(wchar_t)), SQLITE_TRANSIENT));
    }
    bool next() {
        auto rc = sqlite3_step(st);
        if (rc == SQLITE_ROW)
            return true;
        if (rc != SQLITE_DONE)
            verify(db, rc);
        return false;
    }
    Id id(int col = 0) { return sqlite3_column_int64(st, col); }
    std::wstring text(int col) {
        auto ptr = static_cast<const wchar_t *>(sqlite3_column_text16(st, col));
        return ptr ? std::wstring(ptr, sqlite3_column_bytes16(st, col) / 2) : L"";
    }
};
class Transaction {
    sqlite3 *db;
    bool done = false;

  public:
    explicit Transaction(sqlite3 *d) : db(d) { sql(db, "BEGIN IMMEDIATE"); }
    void commit() {
        sql(db, "COMMIT");
        done = true;
    }
    ~Transaction() {
        if (!done)
            sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    }
};
void prune(sqlite3 *db) {
    sql(db, "DELETE FROM tags WHERE NOT EXISTS(SELECT 1 FROM prompt_tags WHERE tag_id=tags.id)");
}
} // namespace
std::wstring trimmed(std::wstring_view s) {
    auto blank = [](wchar_t c) { return std::iswspace(c) || c == 0x3000 || c == 0xa0; };
    while (!s.empty() && blank(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && blank(s.back()))
        s.remove_suffix(1);
    return std::wstring(s);
}
Id utcMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
PromptStore::PromptStore(const std::filesystem::path &path) {
    try {
        auto opened = sqlite3_open16(path.c_str(), &db);
        verify(db, opened);
        sqlite3_busy_timeout(db, 1500);
        sql(db, "PRAGMA foreign_keys=ON");
        int version;
        {
            Statement q(db, "PRAGMA user_version");
            q.next();
            version = q.id();
        }
        if (version > 1)
            throw std::runtime_error("此资料库来自较新版本，请使用更新的提示词管理器。");
        if (!version) {
            Transaction tx(db);
            sql(db,
                R"(CREATE TABLE prompts(id INTEGER PRIMARY KEY,title TEXT NOT NULL,body TEXT NOT NULL,created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL,last_used_at INTEGER);
CREATE TABLE tags(id INTEGER PRIMARY KEY,name TEXT NOT NULL COLLATE NOCASE UNIQUE);
CREATE TABLE prompt_tags(prompt_id INTEGER NOT NULL REFERENCES prompts(id) ON DELETE CASCADE,tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,PRIMARY KEY(prompt_id,tag_id));
CREATE INDEX tags_lookup ON prompt_tags(tag_id,prompt_id);
PRAGMA user_version=1;)");
            tx.commit();
        }
    } catch (...) {
        sqlite3_close(db);
        throw;
    }
}
PromptStore::~PromptStore() { sqlite3_close(db); }
namespace {
Id savePrompt(sqlite3 *db, const Prompt &p, Id created, Id updated) {
    auto title = trimmed(p.title);
    if (title.empty() || trimmed(p.body).empty())
        throw std::invalid_argument("请填写标题和正文。");
    Id id = p.id;
    {
        Statement q(db,
                    id ? "UPDATE prompts SET title=?1,body=?2,updated_at=?3 WHERE id=?4"
                       : "INSERT INTO prompts(title,body,updated_at,created_at) VALUES(?1,?2,?3,?4)");
        q.bind(1, title);
        q.bind(2, p.body);
        q.bind(3, updated);
        q.bind(4, id ? id : created);
        q.next();
        if (id && !sqlite3_changes(db))
            throw std::runtime_error("条目已移除。");
        if (!id)
            id = sqlite3_last_insert_rowid(db);
    }
    {
        Statement clear(db, "DELETE FROM prompt_tags WHERE prompt_id=?");
        clear.bind(1, id);
        clear.next();
    }
    for (const auto &raw : p.tags) {
        auto name = trimmed(raw);
        if (name.empty())
            continue;
        Statement t(db, "INSERT OR IGNORE INTO tags(name) VALUES(?)");
        t.bind(1, name);
        t.next();
        Statement link(db, "INSERT OR IGNORE INTO prompt_tags SELECT ?1,id FROM tags WHERE name=?2");
        link.bind(1, id);
        link.bind(2, name);
        link.next();
    }
    return id;
}
std::string readBackup(const std::filesystem::path &path, int limit) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const std::streamoff length = file.tellg();
    if (length < 0)
        throw std::runtime_error("无法读取导入文件。");
    if (length > limit)
        throw std::runtime_error("导入文件过大。");
    std::string bytes(static_cast<size_t>(length), '\0');
    file.seekg(0);
    if (!file.read(bytes.data(), length))
        throw std::runtime_error("读取导入文件时发生错误。");
    if (bytes.starts_with("\xef\xbb\xbf"))
        bytes.erase(0, 3);
    if (bytes.find('\0') != std::string::npos ||
        (!bytes.empty() && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                                                static_cast<int>(bytes.size()), nullptr, 0)))
        throw std::invalid_argument("导入文件不是有效的 UTF-8 JSON。");
    return bytes;
}
void importItem(sqlite3 *db, const std::wstring &json, ImportResult &result) {
    Statement fields(
        db, R"(SELECT json_extract(?1,'$.id'),json_extract(?1,'$.name'),json_extract(?1,'$.text'),
        coalesce(json_extract(?1,'$.createdAt'),?2),coalesce(json_extract(?1,'$.updatedAt'),?2)
        WHERE json_type(?1,'$.id')='text' AND json_type(?1,'$.name')='text'
        AND json_type(?1,'$.text')='text'
        AND coalesce(json_type(?1,'$.tags'),'array')='array'
        AND coalesce(json_type(?1,'$.createdAt'),'integer')='integer'
        AND coalesce(json_type(?1,'$.updatedAt'),'integer')='integer')");
    fields.bind(1, json);
    fields.bind(2, utcMillis());
    if (!fields.next())
        throw std::invalid_argument("备份条目的字段格式有误。");
    auto sourceId = fields.text(0);
    Prompt p{0, fields.text(1), fields.text(2), {}};
    auto checkText = [](const std::wstring &s) {
        if (s.find(L'\0') != s.npos)
            throw std::invalid_argument("备份条目含有无法显示的空字符。");
    };
    checkText(sourceId);
    checkText(p.title);
    checkText(p.body);
    if (trimmed(sourceId).empty() || trimmed(p.title).empty() || trimmed(p.body).empty())
        throw std::invalid_argument("备份条目缺少标识、标题或正文。");
    Statement tags(db, "SELECT type,value FROM json_each(?1,'$.tags')");
    tags.bind(1, json);
    while (tags.next()) {
        if (tags.text(0) != L"text")
            throw std::invalid_argument("备份条目的标签格式有误。");
        p.tags.push_back(tags.text(1));
        checkText(p.tags.back());
    }
    Statement existing(db, "SELECT 1 FROM voyager_imports WHERE source_id=?");
    existing.bind(1, sourceId);
    if (existing.next()) {
        ++result.skipped;
        return;
    }
    Id id = savePrompt(db, p, fields.id(3), fields.id(4));
    Statement record(db, "INSERT INTO voyager_imports(source_id,prompt_id) VALUES(?1,?2)");
    record.bind(1, sourceId);
    record.bind(2, id);
    record.next();
    ++result.added;
}
} // namespace
Id PromptStore::save(const Prompt &p) {
    Transaction tx(db);
    Id now = utcMillis();
    Id id = savePrompt(db, p, now, now);
    prune(db);
    tx.commit();
    return id;
}
ImportResult PromptStore::importVoyager(const std::filesystem::path &path) {
    auto json = readBackup(path, sqlite3_limit(db, SQLITE_LIMIT_LENGTH, -1));
    {
        Statement header(db, R"(SELECT CASE WHEN NOT json_valid(?1) THEN 0
            WHEN json_extract(?1,'$.format')='gemini-voyager.prompts.v1'
                 AND json_type(?1,'$.items')='array' THEN 1 ELSE -1 END)");
        header.bind(1, json);
        header.next();
        if (header.id() == 0)
            throw std::invalid_argument("导入文件不是有效的 JSON。");
        if (header.id() == -1)
            throw std::invalid_argument("请选择 Gemini Voyager 提示词备份文件。");
    }
    Transaction tx(db);
    sql(db, R"(CREATE TABLE IF NOT EXISTS voyager_imports(source_id TEXT PRIMARY KEY,
               prompt_id INTEGER NOT NULL UNIQUE REFERENCES prompts(id) ON DELETE CASCADE))");
    Statement items(db, "SELECT type,value FROM json_each(?1,'$.items')");
    items.bind(1, json);
    ImportResult result;
    while (items.next()) {
        if (items.text(0) != L"object")
            throw std::invalid_argument("备份中的提示词条目格式有误。");
        importItem(db, items.text(1), result);
    }
    tx.commit();
    return result;
}
void PromptStore::erase(Id id) {
    Transaction tx(db);
    Statement q(db, "DELETE FROM prompts WHERE id=?");
    q.bind(1, id);
    q.next();
    prune(db);
    tx.commit();
}
std::optional<Prompt> PromptStore::get(Id id) {
    Statement q(db, "SELECT title,body FROM prompts WHERE id=?");
    q.bind(1, id);
    if (!q.next())
        return {};
    Prompt p{id, q.text(0), q.text(1), {}};
    Statement t(db,
                "SELECT name FROM tags JOIN prompt_tags ON tag_id=id WHERE prompt_id=? ORDER BY name");
    t.bind(1, id);
    while (t.next())
        p.tags.push_back(t.text(0));
    return p;
}
std::vector<Tag> PromptStore::tags() {
    std::vector<Tag> result;
    Statement q(db, "SELECT id,name FROM tags ORDER BY name");
    while (q.next())
        result.push_back({q.id(), q.text(1)});
    return result;
}
std::vector<Summary> PromptStore::query(Id tag, std::wstring_view search) {
    Statement q(
        db,
        R"(SELECT id,title FROM prompts WHERE (?1=0 OR id IN(SELECT prompt_id FROM prompt_tags WHERE tag_id=?1))
 AND (?2='' OR instr(lower(title),lower(?2)) OR instr(lower(body),lower(?2)))
 ORDER BY last_used_at DESC,CASE WHEN last_used_at IS NULL THEN created_at END DESC,id DESC)");
    q.bind(1, tag);
    q.bind(2, search);
    std::vector<Summary> result;
    while (q.next())
        result.push_back({q.id(), q.text(1)});
    return result;
}
void PromptStore::markUsed(Id id, Id time) {
    Statement q(db, "UPDATE prompts SET last_used_at=? WHERE id=?");
    q.bind(1, time);
    q.bind(2, id);
    q.next();
}
