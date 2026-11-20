#include "platform/Clipboard.h"
#include <stdexcept>
int main(){
    if(windowsLines(L"😀\n\t A\r\n\nB\r")!=L"😀\r\n\t A\r\n\r\nB\r\n")throw std::runtime_error("newline preservation");
}
