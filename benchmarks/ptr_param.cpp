#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif
NOINLINE int by_value(std::shared_ptr<int> p){return *p;}
NOINLINE int by_ref(const std::shared_ptr<int>& p){return *p;}
int main(){constexpr int iterations=5000000;auto p=std::make_shared<int>(7);volatile std::int64_t sink=0;auto a=std::chrono::steady_clock::now();for(int i=0;i<iterations;++i)sink+=by_value(p);auto b=std::chrono::steady_clock::now();for(int i=0;i<iterations;++i)sink+=by_ref(p);auto c=std::chrono::steady_clock::now();std::cout<<"by_value_ns="<<std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count()<<"\nby_ref_ns="<<std::chrono::duration_cast<std::chrono::nanoseconds>(c-b).count()<<"\nsink="<<sink<<"\n";}
