#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class executor {
public:
    executor(){auto n=std::thread::hardware_concurrency();if(n<2)n=2;for(unsigned i=0;i<n;++i)workers.emplace_back([this]{work();});}
    ~executor(){{std::lock_guard<std::mutex> g(m);stop=true;}cv.notify_all();for(auto& t:workers)t.join();}
    template<class F> auto submit(F&& f)->std::future<typename std::result_of<F()>::type>{using R=typename std::result_of<F()>::type;auto task=std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));auto fut=task->get_future();{std::lock_guard<std::mutex> g(m);q.emplace([task]{(*task)();});}cv.notify_one();return fut;}
private:
    void work(){for(;;){std::function<void()> job;{std::unique_lock<std::mutex> l(m);cv.wait(l,[this]{return stop||!q.empty();});if(stop&&q.empty())return;job=std::move(q.front());q.pop();}job();}}
    std::vector<std::thread> workers;std::queue<std::function<void()>> q;std::mutex m;std::condition_variable cv;bool stop=false;
};
int main(){constexpr int tasks=10000;executor ex;std::vector<std::future<int>> fs;fs.reserve(tasks);auto start=std::chrono::steady_clock::now();for(int i=0;i<tasks;++i)fs.push_back(ex.submit([i]{return i+1;}));std::int64_t sum=0;for(auto& f:fs)sum+=f.get();auto end=std::chrono::steady_clock::now();auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();std::cout<<"tasks="<<tasks<<"\ntotal_ns="<<ns<<"\nns_per_task="<<(ns/tasks)<<"\nsum="<<sum<<"\n";}
