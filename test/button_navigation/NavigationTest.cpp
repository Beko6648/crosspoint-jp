#include "ButtonNavigator.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <new>
static unsigned allocations=0;
void* operator new(std::size_t n) { ++allocations; if(auto p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
unsigned long now=1000;
int main() {
 using B=MappedInputManager::Button;
 MappedInputManager input;
 ButtonNavigator nav;
 int calls=0;
 nav.onNext([&]{++calls;}); assert(calls==0); // Unbound input.
 ButtonNavigator::setMappedInputManager(input);
 const unsigned before=allocations;
 for(int i=0;i<100;++i) {nav.onNext([&]{++calls;});nav.onPrevious([&]{++calls;});}
 const unsigned idle=allocations-before;
#ifdef BASELINE
 assert(idle==400);
#else
 assert(idle==0);
#endif
 for(auto b:{B::Down,B::Right}) {
  input={};input.pressed[int(b)]=true;
  int start=calls;nav.onNextPress([&]{++calls;});nav.onPreviousPress([&]{++calls;});assert(calls==start+1);
 }
 for(auto b:{B::Up,B::Left}) {
  input={};input.released[int(b)]=true;
  int start=calls;nav.onPreviousRelease([&]{++calls;});nav.onNextRelease([&]{++calls;});assert(calls==start+1);
 }
 input={};input.held[int(B::Down)]=true;input.duration=500;
 int start=calls;nav.onNextContinuous([&]{++calls;});assert(calls==start);
 input.duration=501;nav.onNextContinuous([&]{++calls;});assert(calls==start+1);
 now=1500;nav.onNextContinuous([&]{++calls;});assert(calls==start+1);
 now=1501;nav.onNextContinuous([&]{++calls;});assert(calls==start+2);
 input.held[int(B::Down)]=false;input.released[int(B::Down)]=true;
 nav.onNextRelease([&]{++calls;});assert(calls==start+2); // Suppress release after repeat.
 nav.onNextRelease([&]{++calls;});assert(calls==start+3); // Repeat state was cleared.
 input={};input.pressed[int(B::Left)]=true;input.pressed[int(B::Right)]=true;
 start=calls;nav.onPress({B::Left,B::Right},[&]{++calls;});assert(calls==start+1);
 nav.onPress({},[&]{++calls;});assert(calls==start+1);
 assert(ButtonNavigator::nextIndex(2,3)==0);
 assert(ButtonNavigator::previousIndex(0,3)==2);
 assert(ButtonNavigator::nextPageIndex(29,30,10)==0);
 assert(ButtonNavigator::previousPageIndex(0,30,10)==20);
 assert(ButtonNavigator::nextIndex(0,0)==0);
 std::printf("PASS idle polls=100 allocations=%u; direction, press/release/repeat and wrap behavior\n",idle);
}
