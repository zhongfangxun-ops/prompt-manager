#include "library/PromptStore.h"
#include <sqlite3.h>
#include <chrono>
#include <stdexcept>
#include <cwctype>
namespace {
void verify(sqlite3* db,int code){if(code!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));}
void sql(sqlite3* db,const char* command){verify(db,sqlite3_exec(db,command,nullptr,nullptr,nullptr));}
class Statement{
    sqlite3* db;sqlite3_stmt* st{};
public:
    Statement(sqlite3* d,const char* q):db(d){verify(db,sqlite3_prepare_v2(db,q,-1,&st,nullptr));}
    ~Statement(){sqlite3_finalize(st);}
    void bind(int n,Id i){verify(db,sqlite3_bind_int64(st,n,i));}
    void bind(int n,std::wstring_view s){verify(db,sqlite3_bind_text16(st,n,s.empty()?L"":s.data(),int(s.size()*sizeof(wchar_t)),SQLITE_TRANSIENT));}
    bool next(){auto rc=sqlite3_step(st);if(rc==SQLITE_ROW)return true;if(rc!=SQLITE_DONE)verify(db,rc);return false;}
    Id id(int col=0){return sqlite3_column_int64(st,col);}
    std::wstring text(int col){auto ptr=static_cast<const wchar_t*>(sqlite3_column_text16(st,col));return ptr?std::wstring(ptr,sqlite3_column_bytes16(st,col)/2):L"";}
};
class Transaction{
    sqlite3* db;bool done=false;
public:
    explicit Transaction(sqlite3* d):db(d){sql(db,"BEGIN IMMEDIATE");}
    void commit(){sql(db,"COMMIT");done=true;}
    ~Transaction(){if(!done)sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);}
};
void prune(sqlite3* db){sql(db,"DELETE FROM tags WHERE NOT EXISTS(SELECT 1 FROM prompt_tags WHERE tag_id=tags.id)");}
}
std::wstring trimmed(std::wstring_view s){
    auto blank=[](wchar_t c){return std::iswspace(c)||c==0x3000||c==0xa0;};
    while(!s.empty()&&blank(s.front()))s.remove_prefix(1);
    while(!s.empty()&&blank(s.back()))s.remove_suffix(1);
    return std::wstring(s);
}
Id utcMillis(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
PromptStore::PromptStore(const std::filesystem::path& path){
    try{
        verify(db,sqlite3_open16(path.c_str(),&db));sqlite3_busy_timeout(db,1500);
        sql(db,"PRAGMA foreign_keys=ON");int version;
        {Statement q(db,"PRAGMA user_version");q.next();version=q.id();}
        if(version>1)throw std::runtime_error("此资料库来自较新版本，请使用更新的提示词管理器。");
        if(!version){
            Transaction tx(db);
            sql(db,R"(CREATE TABLE prompts(id INTEGER PRIMARY KEY,title TEXT NOT NULL,body TEXT NOT NULL,created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL,last_used_at INTEGER);
CREATE TABLE tags(id INTEGER PRIMARY KEY,name TEXT NOT NULL COLLATE NOCASE UNIQUE);
CREATE TABLE prompt_tags(prompt_id INTEGER NOT NULL REFERENCES prompts(id) ON DELETE CASCADE,tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,PRIMARY KEY(prompt_id,tag_id));
CREATE INDEX tags_lookup ON prompt_tags(tag_id,prompt_id);
PRAGMA user_version=1;)");
            tx.commit();
        }
    }catch(...){sqlite3_close(db);throw;}
}
PromptStore::~PromptStore(){sqlite3_close(db);}
Id PromptStore::save(const Prompt& p){
    auto title=trimmed(p.title);if(title.empty()||trimmed(p.body).empty())throw std::invalid_argument("请填写标题和正文。");
    Transaction tx(db);Id id=p.id,now=utcMillis();
    {Statement q(db,id?"UPDATE prompts SET title=?1,body=?2,updated_at=?3 WHERE id=?4":"INSERT INTO prompts(title,body,created_at,updated_at) VALUES(?1,?2,?3,?3)");
     q.bind(1,title);q.bind(2,p.body);q.bind(3,now);if(id)q.bind(4,id);q.next();
     if(id&&!sqlite3_changes(db))throw std::runtime_error("条目已移除。");
     if(!id)id=sqlite3_last_insert_rowid(db);}
    {Statement clear(db,"DELETE FROM prompt_tags WHERE prompt_id=?");clear.bind(1,id);clear.next();}
    for(const auto& raw:p.tags){auto name=trimmed(raw);if(name.empty())continue;
        Statement t(db,"INSERT OR IGNORE INTO tags(name) VALUES(?)");t.bind(1,name);t.next();
        Statement link(db,"INSERT OR IGNORE INTO prompt_tags SELECT ?1,id FROM tags WHERE name=?2");link.bind(1,id);link.bind(2,name);link.next();}
    prune(db);tx.commit();return id;
}
void PromptStore::erase(Id id){
    Transaction tx(db);Statement q(db,"DELETE FROM prompts WHERE id=?");q.bind(1,id);q.next();prune(db);tx.commit();
}
std::optional<Prompt> PromptStore::get(Id id){
    Statement q(db,"SELECT title,body FROM prompts WHERE id=?");q.bind(1,id);if(!q.next())return {};
    Prompt p{id,q.text(0),q.text(1),{}};Statement t(db,"SELECT name FROM tags JOIN prompt_tags ON tag_id=id WHERE prompt_id=? ORDER BY name");
    t.bind(1,id);while(t.next())p.tags.push_back(t.text(0));return p;
}
std::vector<Tag> PromptStore::tags(){
    std::vector<Tag> result;Statement q(db,"SELECT id,name FROM tags ORDER BY name");while(q.next())result.push_back({q.id(),q.text(1)});return result;
}
std::vector<Summary> PromptStore::query(Id tag,std::wstring_view search){
    Statement q(db,R"(SELECT id,title FROM prompts WHERE (?1=0 OR id IN(SELECT prompt_id FROM prompt_tags WHERE tag_id=?1))
 AND (?2='' OR instr(lower(title),lower(?2)) OR instr(lower(body),lower(?2)))
 ORDER BY last_used_at DESC,CASE WHEN last_used_at IS NULL THEN created_at END DESC,id DESC)");
    q.bind(1,tag);q.bind(2,search);std::vector<Summary> result;while(q.next())result.push_back({q.id(),q.text(1)});return result;
}
void PromptStore::markUsed(Id id,Id time){Statement q(db,"UPDATE prompts SET last_used_at=? WHERE id=?");q.bind(1,time);q.bind(2,id);q.next();}

