#include "library/PromptStore.h"
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("query assertion");}
int main(){
    PromptStore s(L":memory:");auto a=s.save({0,L"课程录音",L"保留术语 CODE 10%",{L"学习",L"写作"}});
    auto b=s.save({0,L"修改",L"术语_X",{L"写作"}});auto c=s.save({0,L"备忘",L"其他",{}});
    for(auto t:s.tags())check(s.query(t.id,L"术语").size()==(t.name==L"学习"?1:2));
    check(s.query(0,L"code").at(0).id==a&&s.query(0,L"%").size()==1&&s.query(0,L"_").at(0).id==b);
    s.markUsed(a,100);s.markUsed(b,100);auto rows=s.query(0,L"");
    check(rows.size()==3&&rows[0].id==b&&rows[1].id==a&&rows[2].id==c);
}
