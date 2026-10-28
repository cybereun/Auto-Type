#include "PendingWord.h"
#include <fstream>
#include <iostream>
#include <iterator>
using namespace autotype;
int main(int argc,char** argv) {
    std::ifstream f(argc>1?argv[1]:"resources/model-v2.dat",std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(f)),{}); Model model;
    if(!model.load(reinterpret_cast<const uint8_t*>(data.data()),data.size())) return 2;
    PendingWord pending(model); int failures=0;
    auto check=[&](bool ok,const char* what){ if(!ok){std::cout<<"FAIL "<<what<<'\n';++failures;} };
    for(char k:std::string("dkssud")) pending.append(k);
    check(pending.preview()==U"안녕","live Korean preview");
    check(pending.finish()==U"안녕","commit whole word once");
    check(pending.empty(),"commit clears buffer");
    for(char k:std::string("hello")) pending.append(k);
    check(pending.finish()==U"hello","English recognition");
    pending.toggle(); pending.append('d');
    check(pending.preview()==U"d","English starting hint");
    for(char k:std::string("kssud")) pending.append(k);
    check(pending.finish()==U"안녕","English hint still detects Korean");
    pending.toggle(); pending.append('g'); pending.append('k'); pending.append('s');
    pending.backspace(); check(pending.preview()==U"하","backspace follows keystrokes");
    pending.clear(); check(pending.empty(),"cancel clears buffer");
    if(!failures) std::cout<<"all pending word tests passed\n";
    return failures?1:0;
}
