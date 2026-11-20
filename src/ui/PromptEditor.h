#pragma once
#include "library/PromptStore.h"
struct EditorState {
    Prompt initial{};
    std::vector<std::wstring> tags;
    void start(const Prompt& p){initial=p;tags=p.tags;}
    void add(std::wstring name);
    bool changed(std::wstring_view title,std::wstring_view body,std::wstring_view pendingTag) const;
};
