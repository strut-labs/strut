#include "generated_runtime.h"

#include <ostream>

namespace strut::generated_runtime {

void emit_bytes(std::ostream& out) {
    out << R"STRUT_BYTES(
class strut_bytes {
public:
    strut_bytes()=default;
    explicit strut_bytes(std::int64_t size):value_(checked_size(size)){}
    strut_bytes(std::initializer_list<std::uint8_t> value):value_(value){}
    static strut_bytes from_string(const strut_string& value){strut_bytes out;out.value_.assign(value.v.begin(),value.v.end());return out;}
    strut_string to_string() const{return value_.empty()?strut_string():strut_string(std::string(reinterpret_cast<const char*>(value_.data()),value_.size()));}
    std::int64_t size() const{if(value_.size()>static_cast<std::size_t>(INT64_MAX))throw std::length_error("bytes length exceeds int_64");return static_cast<std::int64_t>(value_.size());}
    bool empty() const noexcept{return value_.empty();}
    std::uint8_t& at(std::int64_t index){return value_.at(checked_index(index));}
    const std::uint8_t& at(std::int64_t index) const{return value_.at(checked_index(index));}
    strut_bytes slice(std::int64_t begin,std::int64_t end) const{if(begin<0||end<begin||static_cast<std::uint64_t>(end)>value_.size())throw std::out_of_range("bytes slice out of range");strut_bytes out;out.value_.assign(value_.begin()+begin,value_.begin()+end);return out;}
    std::uint8_t* data() noexcept{return value_.data();}
    const std::uint8_t* data() const noexcept{return value_.data();}
    std::size_t native_size() const noexcept{return value_.size();}
    void resize_native(std::size_t size){value_.resize(size);}
    void append_native(const char* data,std::size_t size){value_.insert(value_.end(),reinterpret_cast<const std::uint8_t*>(data),reinterpret_cast<const std::uint8_t*>(data)+size);}
    friend bool operator==(const strut_bytes& a,const strut_bytes& b){return a.value_==b.value_;}
    friend bool operator!=(const strut_bytes& a,const strut_bytes& b){return !(a==b);}
private:
    static std::size_t checked_size(std::int64_t size){if(size<0)throw std::length_error("bytes size cannot be negative");if(static_cast<std::uint64_t>(size)>SIZE_MAX)throw std::length_error("bytes size exceeds native address space");return static_cast<std::size_t>(size);}
    std::size_t checked_index(std::int64_t index) const{if(index<0||static_cast<std::uint64_t>(index)>=value_.size())throw std::out_of_range("bytes index out of range");return static_cast<std::size_t>(index);}
    std::vector<std::uint8_t> value_;
};
)STRUT_BYTES";
}

void emit_streams(std::ostream& out) {
    out << R"STRUT_STREAMS(
#include <algorithm>
#include <limits>
class strut_ostream {
public:
    strut_ostream()=default;explicit strut_ostream(std::ostream& stream):p_(&stream){}
    template<class T> strut_ostream& write_value(const T& value){require_open();(*p_)<<value;if(!*p_)throw strut_checked_error("StreamError","output stream write failed");return *this;}
    strut_ostream& write_value(std::int8_t value){return write_value(static_cast<std::int32_t>(value));}
    strut_ostream& write_value(std::uint8_t value){return write_value(static_cast<std::uint32_t>(value));}
    void write(const strut_string& value){write_value(value);}
    void write_bytes(const strut_bytes& value){require_open();std::size_t offset=0;const auto maximum=static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());while(offset<value.native_size()){const auto count=std::min(maximum,value.native_size()-offset);p_->write(reinterpret_cast<const char*>(value.data()+offset),static_cast<std::streamsize>(count));if(!*p_)throw strut_checked_error("StreamError","output stream byte write failed");offset+=count;}}
    void flush(){if(!p_)return;p_->flush();if(!*p_)throw strut_checked_error("StreamError","output stream flush failed");}
    void close() noexcept{p_=nullptr;}
protected:
    void attach(std::ostream& stream) noexcept{p_=&stream;}
    void require_open() const{if(!p_)throw strut_checked_error("StreamError","output stream is not open");}
    std::ostream* p_=nullptr;
};
class strut_istream {
public:
    strut_istream()=default;explicit strut_istream(std::istream& stream):p_(&stream){}
    template<class T> strut_istream& read_value(T& value){require_open();(*p_)>>value;if(p_->eof())eof_=true;return *this;}
    strut_bytes read_bytes(std::int64_t max_bytes){require_open();if(max_bytes<0)throw strut_checked_error("StreamError","read size cannot be negative");if(max_bytes==0||eof_)return {};if(static_cast<std::uint64_t>(max_bytes)>static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))throw strut_checked_error("StreamError","read size exceeds native stream limit");strut_bytes out(max_bytes);p_->read(reinterpret_cast<char*>(out.data()),static_cast<std::streamsize>(max_bytes));const auto count=p_->gcount();if(p_->bad())throw strut_checked_error("StreamError","input stream byte read failed");if(p_->eof())eof_=true;else if(p_->fail())throw strut_checked_error("StreamError","input stream byte read failed");out.resize_native(static_cast<std::size_t>(count));return out;}
    strut_bytes read_all_bytes(std::int64_t limit=INT64_MAX){require_open();if(limit<0)throw strut_checked_error("StreamError","read limit cannot be negative");strut_bytes out;char buffer[8192];while(!eof_){p_->read(buffer,sizeof(buffer));const auto count=p_->gcount();if(p_->bad())throw strut_checked_error("StreamError","input stream byte read failed");if(count>0){if(static_cast<std::uint64_t>(count)>static_cast<std::uint64_t>(limit)-out.native_size())throw strut_checked_error("StreamError","input stream byte limit exceeded");out.append_native(buffer,static_cast<std::size_t>(count));}if(p_->eof())eof_=true;else if(p_->fail())throw strut_checked_error("StreamError","input stream byte read failed");}return out;}
    bool eof() const noexcept{return eof_;}
    void close() noexcept{p_=nullptr;}
protected:
    void attach(std::istream& stream) noexcept{p_=&stream;eof_=false;}
    void require_open() const{if(!p_)throw strut_checked_error("StreamError","input stream is not open");}
    std::istream* p_=nullptr;bool eof_=false;
};
class strut_ofstream : public strut_ostream {
public:
    strut_ofstream()=default;explicit strut_ofstream(const strut_string& path){open(path);}strut_ofstream(const strut_string& path,bool binary){open(path,binary);}
    void open(const strut_string& path,bool binary=false){close();auto mode=std::ios::out|(binary?std::ios::binary:std::ios::openmode(0));file_.open(path.v,mode);if(!file_)throw strut_checked_error("StreamError","ofstream.open failed");attach(file_);}
    void close(){if(file_.is_open()){file_.close();if(file_.fail())throw strut_checked_error("StreamError","ofstream.close failed");}strut_ostream::close();}
    bool is_open() const{return file_.is_open();}
    ~strut_ofstream(){if(file_.is_open())file_.close();}
private:std::ofstream file_;
};
class strut_ifstream : public strut_istream {
public:
    strut_ifstream()=default;explicit strut_ifstream(const strut_string& path){open(path);}strut_ifstream(const strut_string& path,bool binary){open(path,binary);}
    void open(const strut_string& path,bool binary=false){close();auto mode=std::ios::in|(binary?std::ios::binary:std::ios::openmode(0));file_.open(path.v,mode);if(!file_)throw strut_checked_error("StreamError","ifstream.open failed");attach(file_);}
    void close(){if(file_.is_open())file_.close();strut_istream::close();}
    bool is_open() const{return file_.is_open();}
    strut_string read_all(){if(!file_.is_open())throw strut_checked_error("StreamError","ifstream.read_all on closed stream");std::ostringstream out;out<<file_.rdbuf();if(file_.bad())throw strut_checked_error("StreamError","ifstream.read_all failed");eof_=true;return strut_string(out.str());}
    ~strut_ifstream(){if(file_.is_open())file_.close();}
private:std::ifstream file_;
};
class strut_sstream : public strut_ostream {
public:
    strut_sstream(){attach(stream_);}
    void write(const strut_string& value){write_value(value);}
    strut_string str() const{return stream_.str();}
    strut_string read_all() const{return stream_.str();}
    bool is_open() const{return p_!=nullptr;}
    void close() noexcept{strut_ostream::close();}
private:std::stringstream stream_;
};
struct strut_endl_t{};
[[maybe_unused]] static strut_endl_t endl{};
template<class T> strut_ostream& operator<<(strut_ostream& stream,const T& value){return stream.write_value(value);}
inline strut_ostream& operator<<(strut_ostream& stream,strut_endl_t){stream.write_value('\n');stream.flush();return stream;}
template<class T> strut_istream& operator>>(strut_istream& stream,T& value){return stream.read_value(value);}
inline strut_istream& operator>>(strut_istream& stream,strut_string& value){std::string temporary;stream.read_value(temporary);value=strut_string(temporary);return stream;}
static strut_istream in{std::cin};
static strut_ostream out{std::cout};
static strut_ostream err{std::cerr};
inline strut_string strut_input(){std::string value;std::getline(std::cin,value);return value;}
template<class T> void strut_input(T& value){std::cin>>value;}
inline void strut_input(strut_string& value){std::string temporary;std::cin>>temporary;value=strut_string(temporary);}
)STRUT_STREAMS";
}

void emit_cancellation(std::ostream& out) {
    out << R"STRUT_CANCEL(
#define STRUT_CANCELLATION_RUNTIME_DEFINED 1
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
struct strut_cancellation_callback {
    std::mutex mutex;
    std::condition_variable cv;
    std::function<void()> function;
    bool active=true;
    bool running=false;
    std::thread::id running_thread;
};
struct strut_cancellation_state {
    std::atomic<bool> cancelled{false};
    std::mutex mutex;
    std::condition_variable cv;
    std::unordered_map<std::uint64_t,std::shared_ptr<strut_cancellation_callback>> callbacks;
    std::uint64_t next_id=1;
};
class strut_cancellation_subscription {
public:
    strut_cancellation_subscription()=default;
    strut_cancellation_subscription(std::shared_ptr<strut_cancellation_state> state,std::uint64_t id,std::shared_ptr<strut_cancellation_callback> callback):state_(std::move(state)),id_(id),callback_(std::move(callback)){}
    strut_cancellation_subscription(const strut_cancellation_subscription&)=delete;
    strut_cancellation_subscription& operator=(const strut_cancellation_subscription&)=delete;
    strut_cancellation_subscription(strut_cancellation_subscription&& other) noexcept{move_from(other);}
    strut_cancellation_subscription& operator=(strut_cancellation_subscription&& other) noexcept{if(this!=&other){reset();move_from(other);}return *this;}
    ~strut_cancellation_subscription(){reset();}
    void reset() noexcept{
        auto state=std::move(state_);auto callback=std::move(callback_);const auto id=id_;id_=0;if(!callback)return;
        if(state){std::lock_guard<std::mutex> lock(state->mutex);state->callbacks.erase(id);}
        std::unique_lock<std::mutex> lock(callback->mutex);callback->active=false;if(callback->running&&callback->running_thread==std::this_thread::get_id())return;callback->cv.wait(lock,[&]{return !callback->running;});
    }
private:
    void move_from(strut_cancellation_subscription& other) noexcept{state_=std::move(other.state_);id_=other.id_;callback_=std::move(other.callback_);other.id_=0;}
    std::shared_ptr<strut_cancellation_state> state_;std::uint64_t id_=0;std::shared_ptr<strut_cancellation_callback> callback_;
};
class strut_cancellation_token {
public:
    strut_cancellation_token():state_(std::make_shared<strut_cancellation_state>()){}
    explicit strut_cancellation_token(std::shared_ptr<strut_cancellation_state> state):state_(std::move(state)){}
    bool cancelled() const noexcept{return state_->cancelled.load(std::memory_order_acquire);}
    void wait() const{std::mutex mutex;std::condition_variable cv;bool done=false;auto subscription=subscribe([&]{{std::lock_guard<std::mutex> lock(mutex);done=true;}cv.notify_one();});std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return done;});}
    void throw_if_cancelled() const{if(cancelled())throw strut_checked_error("CancellationError","operation cancelled");}
    template<class F> strut_cancellation_subscription subscribe(F&& function) const{
        std::function<void()> stored(std::forward<F>(function));if(!stored)return {};auto callback=std::make_shared<strut_cancellation_callback>();callback->function=std::move(stored);std::uint64_t id=0;bool invoke=false;
        {std::lock_guard<std::mutex> lock(state_->mutex);if(state_->cancelled.load(std::memory_order_acquire))invoke=true;else{id=state_->next_id++;state_->callbacks.emplace(id,callback);}}
        if(invoke){std::function<void()> call;{std::lock_guard<std::mutex> lock(callback->mutex);if(callback->active){call=std::move(callback->function);callback->running=true;callback->running_thread=std::this_thread::get_id();}}try{call();}catch(...){ }{std::lock_guard<std::mutex> lock(callback->mutex);callback->running=false;callback->running_thread={};callback->active=false;}callback->cv.notify_all();return {};}
        return strut_cancellation_subscription(state_,id,std::move(callback));
    }
private:std::shared_ptr<strut_cancellation_state> state_;friend class strut_cancellation_source;
};
class strut_cancellation_source {
public:
    strut_cancellation_source():state_(std::make_shared<strut_cancellation_state>()){}
    strut_cancellation_token token() const{return strut_cancellation_token(state_);}
    void cancel() const{
        bool expected=false;if(!state_->cancelled.compare_exchange_strong(expected,true,std::memory_order_acq_rel))return;
        std::vector<std::shared_ptr<strut_cancellation_callback>> callbacks;{std::lock_guard<std::mutex> lock(state_->mutex);callbacks.reserve(state_->callbacks.size());for(auto& item:state_->callbacks)callbacks.push_back(std::move(item.second));state_->callbacks.clear();}state_->cv.notify_all();
        for(auto& callback:callbacks){std::function<void()> call;{std::lock_guard<std::mutex> lock(callback->mutex);if(callback->active){call=std::move(callback->function);callback->running=true;callback->running_thread=std::this_thread::get_id();}}if(call){try{call();}catch(...){ }}{std::lock_guard<std::mutex> lock(callback->mutex);callback->running=false;callback->running_thread={};callback->active=false;}callback->cv.notify_all();}
    }
private:std::shared_ptr<strut_cancellation_state> state_;
};
)STRUT_CANCEL";
}

void emit_executor(std::ostream& out) {
    out << R"STRUT_ASYNC(
#ifndef STRUT_EXECUTION_CONTEXT_DEFINED
#define STRUT_EXECUTION_CONTEXT_DEFINED
inline thread_local const void* strut_execution_context=nullptr;
#endif
class strut_executor {
public:
    strut_executor(){auto n=std::thread::hardware_concurrency();if(n<2)n=2;for(unsigned i=0;i<n;++i)workers_.emplace_back([this]{worker();});}
    ~strut_executor(){{std::lock_guard<std::mutex> g(m_);stopping_=true;}cv_.notify_all();for(auto& t:workers_)if(t.joinable())t.join();}
    template<class F> auto submit(F&& f)->std::future<std::invoke_result_t<F>>{using R=std::invoke_result_t<F>;auto task=std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));auto fut=task->get_future();const void* context=strut_execution_context;{std::lock_guard<std::mutex> g(m_);q_.emplace([task,context]{const void* previous=strut_execution_context;strut_execution_context=context;(*task)();strut_execution_context=previous;});}cv_.notify_one();return fut;}
private:
    void worker(){for(;;){std::function<void()> job;{std::unique_lock<std::mutex> l(m_);cv_.wait(l,[this]{return stopping_||!q_.empty();});if(stopping_&&q_.empty())return;job=std::move(q_.front());q_.pop();}job();}}
    std::vector<std::thread> workers_;std::queue<std::function<void()>> q_;std::mutex m_;std::condition_variable cv_;bool stopping_=false;
};
inline strut_executor& strut_global_executor(){static strut_executor ex;return ex;}
template<class T> class strut_future { public: strut_future()=default; explicit strut_future(std::future<T>&& f):f_(std::move(f)){} T get(){return f_.get();} bool valid() const{return f_.valid();} private: std::future<T> f_; };
template<> class strut_future<void> { public: strut_future()=default; explicit strut_future(std::future<void>&& f):f_(std::move(f)){} void get(){f_.get();} bool valid() const{return f_.valid();} private: std::future<void> f_; };
template<class F> auto strut_async(F&& f)->strut_future<std::invoke_result_t<F>>{using R=std::invoke_result_t<F>;return strut_future<R>(strut_global_executor().submit(std::forward<F>(f)));}
template<class T> T strut_await(strut_future<T>& f){return f.get();}
template<class T> T strut_await(strut_future<T>&& f){return f.get();}
inline void strut_await(strut_future<void>& f){f.get();}
inline void strut_await(strut_future<void>&& f){f.get();}

)STRUT_ASYNC";
}

void emit_tcp(std::ostream& out, bool connect, bool async) {
    out << R"STRUT_TCP(
#include <atomic>
#include <cerrno>
#ifdef _WIN32
using strut_socket_handle=SOCKET; constexpr strut_socket_handle strut_invalid_socket=INVALID_SOCKET;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket){shutdown(h,SD_BOTH);closesocket(h);}}
inline bool strut_socket_set_blocking(strut_socket_handle h,bool blocking){u_long mode=blocking?0:1;return ioctlsocket(h,FIONBIO,&mode)==0;}
inline bool strut_socket_would_block(){const int error=WSAGetLastError();return error==WSAEWOULDBLOCK||error==WSAEINPROGRESS;}
inline int strut_socket_poll_read(strut_socket_handle h,int timeout_ms){WSAPOLLFD descriptor{h,POLLIN,0};return WSAPoll(&descriptor,1,timeout_ms);}
struct strut_winsock_runtime{strut_winsock_runtime(){WSADATA d{};if(WSAStartup(MAKEWORD(2,2),&d)!=0)throw strut_checked_error("NetworkError","WSAStartup failed");}~strut_winsock_runtime(){WSACleanup();}};
inline void strut_socket_init(){static strut_winsock_runtime runtime;(void)runtime;}
#else
#include <fcntl.h>
#include <poll.h>
using strut_socket_handle=int; constexpr strut_socket_handle strut_invalid_socket=-1;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket){::shutdown(h,SHUT_RDWR);::close(h);}}
inline bool strut_socket_set_blocking(strut_socket_handle h,bool blocking){const int flags=fcntl(h,F_GETFL,0);return flags>=0&&fcntl(h,F_SETFL,blocking?(flags&~O_NONBLOCK):(flags|O_NONBLOCK))==0;}
inline bool strut_socket_would_block(){return errno==EAGAIN||errno==EWOULDBLOCK;}
inline int strut_socket_poll_read(strut_socket_handle h,int timeout_ms){pollfd descriptor{h,POLLIN,0};int result;do{result=poll(&descriptor,1,timeout_ms);}while(result<0&&(errno==EINTR||errno==EAGAIN));return result;}
inline void strut_socket_init(){}
#endif
inline void strut_socket_prepare(strut_socket_handle h){
#ifdef SO_NOSIGPIPE
    int enabled=1;setsockopt(h,SOL_SOCKET,SO_NOSIGPIPE,reinterpret_cast<const char*>(&enabled),sizeof(enabled));
#else
    (void)h;
#endif
}
struct strut_socket_state;
struct strut_socket_operation{std::shared_ptr<strut_socket_state> state;strut_socket_handle handle=strut_invalid_socket;strut_socket_operation()=default;strut_socket_operation(std::shared_ptr<strut_socket_state> value,strut_socket_handle native):state(std::move(value)),handle(native){}strut_socket_operation(const strut_socket_operation&)=delete;strut_socket_operation& operator=(const strut_socket_operation&)=delete;strut_socket_operation(strut_socket_operation&& other) noexcept:state(std::move(other.state)),handle(other.handle){other.handle=strut_invalid_socket;}~strut_socket_operation();};
struct strut_socket_state:std::enable_shared_from_this<strut_socket_state>{mutable std::mutex mutex;std::condition_variable cv;strut_socket_handle handle=strut_invalid_socket;std::size_t operations=0;bool closing=false;~strut_socket_state(){strut_socket_close(handle);}strut_socket_operation acquire(){std::lock_guard<std::mutex> lock(mutex);if(closing||handle==strut_invalid_socket)throw strut_checked_error("NetworkError","operation on closed socket");++operations;return {shared_from_this(),handle};}void release(){std::lock_guard<std::mutex> lock(mutex);if(operations>0)--operations;if(operations==0)cv.notify_all();}void interrupt(){std::lock_guard<std::mutex> lock(mutex);if(handle==strut_invalid_socket)return;
#ifdef _WIN32
    ::shutdown(handle,SD_BOTH);
#else
    ::shutdown(handle,SHUT_RDWR);
#endif
}void close(){std::unique_lock<std::mutex> lock(mutex);if(closing){cv.wait(lock,[&]{return !closing;});return;}if(handle==strut_invalid_socket)return;closing=true;const auto native=handle;
#ifdef _WIN32
    ::shutdown(native,SD_BOTH);
#else
    ::shutdown(native,SHUT_RDWR);
#endif
    cv.wait(lock,[&]{return operations==0;});handle=strut_invalid_socket;lock.unlock();strut_socket_close(native);lock.lock();closing=false;lock.unlock();cv.notify_all();}strut_socket_handle peek() const{std::lock_guard<std::mutex> lock(mutex);return handle;}};
inline strut_socket_operation::~strut_socket_operation(){if(state)state->release();}
struct strut_listener_state;
struct strut_listener_operation{std::shared_ptr<strut_listener_state> state;strut_socket_handle handle=strut_invalid_socket;strut_listener_operation(std::shared_ptr<strut_listener_state> value,strut_socket_handle native):state(std::move(value)),handle(native){}strut_listener_operation(const strut_listener_operation&)=delete;strut_listener_operation& operator=(const strut_listener_operation&)=delete;strut_listener_operation(strut_listener_operation&&)=default;~strut_listener_operation();};
struct strut_listener_state:std::enable_shared_from_this<strut_listener_state>{mutable std::mutex mutex;std::condition_variable cv;strut_socket_handle handle=strut_invalid_socket;std::size_t operations=0;bool closing=false;~strut_listener_state(){strut_socket_close(handle);}strut_listener_operation acquire(){std::lock_guard<std::mutex> lock(mutex);if(closing||handle==strut_invalid_socket)throw strut_checked_error("NetworkError","accept on closed listener");++operations;return {shared_from_this(),handle};}void release(){std::lock_guard<std::mutex> lock(mutex);if(operations>0)--operations;if(operations==0)cv.notify_all();}bool interrupted() const{std::lock_guard<std::mutex> lock(mutex);return closing||handle==strut_invalid_socket;}void close(){std::unique_lock<std::mutex> lock(mutex);if(closing){cv.wait(lock,[&]{return !closing;});return;}if(handle==strut_invalid_socket)return;closing=true;cv.wait(lock,[&]{return operations==0;});const auto native=handle;handle=strut_invalid_socket;lock.unlock();strut_socket_close(native);lock.lock();closing=false;lock.unlock();cv.notify_all();}strut_socket_handle peek() const{std::lock_guard<std::mutex> lock(mutex);return handle;}};
inline strut_listener_operation::~strut_listener_operation(){if(state)state->release();}
class strut_tcp_socket {
public:
    strut_tcp_socket():s_(std::make_shared<strut_socket_state>()){} explicit strut_tcp_socket(strut_socket_handle h){try{s_=std::make_shared<strut_socket_state>();strut_socket_prepare(h);s_->handle=h;}catch(...){strut_socket_close(h);throw;}}
    bool is_open() const{return native_handle()!=strut_invalid_socket;}void close(){if(s_)s_->close();}void shutdown_io() const{if(s_)s_->interrupt();}
    void write(const strut_string& data){if(!s_)throw strut_checked_error("NetworkError","write on closed socket");auto operation=s_->acquire();const auto h=operation.handle;std::size_t off=0;while(off<data.v.size()){
#ifdef _WIN32
        const int amount=static_cast<int>(std::min<std::size_t>(data.v.size()-off,static_cast<std::size_t>(std::numeric_limits<int>::max())));int n=::send(h,data.v.data()+off,amount,0);
#else
        ssize_t n=::send(h,data.v.data()+off,data.v.size()-off,
#ifdef MSG_NOSIGNAL
            MSG_NOSIGNAL
#else
            0
#endif
        );
#endif
        if(n<=0)throw strut_checked_error("NetworkError","socket write failed");off+=static_cast<std::size_t>(n);}}
    strut_string read(std::int64_t max_bytes=4096){if(!s_)throw strut_checked_error("NetworkError","read on closed socket");auto operation=s_->acquire();const auto h=operation.handle;if(max_bytes<=0)return {};std::string out(static_cast<std::size_t>(max_bytes),'\0');
#ifdef _WIN32
        int n=::recv(h,out.data(),static_cast<int>(out.size()),0);
#else
        ssize_t n=::recv(h,out.data(),out.size(),0);
#endif
        if(n<0)throw strut_checked_error("NetworkError","socket read failed");out.resize(static_cast<std::size_t>(n));return strut_string(std::move(out));}
    strut_socket_handle native_handle() const{return s_?s_->peek():strut_invalid_socket;}strut_socket_operation pin() const{if(!s_)throw strut_checked_error("NetworkError","operation on closed socket");return s_->acquire();}
private:std::shared_ptr<strut_socket_state> s_;
};
)STRUT_TCP";
    if (connect) {
        out << R"STRUT_TCP(
inline strut_tcp_socket tcp_connect(const strut_string& host,std::int32_t port){strut_socket_init();if(port<1||port>65535)throw strut_checked_error("NetworkError","invalid TCP port");addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;addrinfo* list=nullptr;const std::string service=std::to_string(port);if(getaddrinfo(host.v.c_str(),service.c_str(),&hints,&list)!=0)throw strut_checked_error("NetworkError","host resolution failed");strut_socket_handle h=strut_invalid_socket;for(addrinfo* p=list;p;p=p->ai_next){h=::socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(h==strut_invalid_socket)continue;if(::connect(h,p->ai_addr,static_cast<int>(p->ai_addrlen))==0)break;strut_socket_close(h);h=strut_invalid_socket;}freeaddrinfo(list);if(h==strut_invalid_socket)throw strut_checked_error("NetworkError","TCP connect failed");return strut_tcp_socket(h);}
)STRUT_TCP";
        if (async) out << "inline strut_future<strut_tcp_socket> tcp_connect_async(const strut_string& host,std::int32_t port){return strut_async([host,port]{return tcp_connect(host,port);});}\n";
    }
    out << R"STRUT_TCP(
class strut_tcp_listener {
public:
    strut_tcp_listener():s_(std::make_shared<strut_listener_state>()){} explicit strut_tcp_listener(strut_socket_handle h){try{s_=std::make_shared<strut_listener_state>();s_->handle=h;}catch(...){strut_socket_close(h);throw;}}
    bool is_open() const{return native_handle()!=strut_invalid_socket;}void close(){if(s_)s_->close();}
    strut_tcp_socket accept(){if(!s_)throw strut_checked_error("NetworkError","accept on closed listener");auto operation=s_->acquire();for(;;){auto h=::accept(operation.handle,nullptr,nullptr);if(h!=strut_invalid_socket){if(!strut_socket_set_blocking(h,true)){strut_socket_close(h);throw strut_checked_error("NetworkError","unable to configure accepted socket");}return strut_tcp_socket(h);}if(!strut_socket_would_block())throw strut_checked_error("NetworkError","TCP accept failed");if(s_->interrupted())throw strut_checked_error("NetworkError","accept on closed listener");if(strut_socket_poll_read(operation.handle,50)<0)throw strut_checked_error("NetworkError","TCP accept failed");}}
)STRUT_TCP";
    if (async) out << "    strut_future<strut_tcp_socket> accept_async(){auto copy=*this;return strut_async([copy]() mutable{return copy.accept();});}\n";
    out << R"STRUT_TCP(
private:strut_socket_handle native_handle() const{return s_?s_->peek():strut_invalid_socket;}std::shared_ptr<strut_listener_state> s_;
};
inline strut_tcp_listener tcp_listen(const strut_string& host,std::int32_t port,std::int32_t backlog=128){strut_socket_init();if(port<1||port>65535)throw strut_checked_error("NetworkError","invalid TCP port");addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_flags=AI_PASSIVE;addrinfo* list=nullptr;const std::string service=std::to_string(port);const char* node=host.v.empty()?nullptr:host.v.c_str();if(getaddrinfo(node,service.c_str(),&hints,&list)!=0)throw strut_checked_error("NetworkError","listen address resolution failed");strut_socket_handle h=strut_invalid_socket;for(addrinfo* p=list;p;p=p->ai_next){h=::socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(h==strut_invalid_socket)continue;int yes=1;setsockopt(h,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&yes),sizeof(yes));if(::bind(h,p->ai_addr,static_cast<int>(p->ai_addrlen))==0&&::listen(h,backlog)==0)break;strut_socket_close(h);h=strut_invalid_socket;}freeaddrinfo(list);if(h==strut_invalid_socket)throw strut_checked_error("NetworkError","TCP listen failed: address unavailable or port already in use");if(!strut_socket_set_blocking(h,false)){strut_socket_close(h);throw strut_checked_error("NetworkError","unable to configure TCP listener");}return strut_tcp_listener(h);}
)STRUT_TCP";
}

void emit_http_client(std::ostream& out, bool async, bool curl_global) {
    if (curl_global) out << R"HTTP_CLIENT(
struct strut_curl_global{strut_curl_global(){if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)throw strut_checked_error("HttpError","libcurl global initialization failed");}~strut_curl_global(){curl_global_cleanup();}};
inline void strut_curl_init(){static strut_curl_global g;(void)g;}
)HTTP_CLIENT";
    out << R"HTTP_CLIENT(
struct strut_http_response {
    std::int32_t status=0; strut_string body; std::unordered_map<strut_string,strut_string> headers;
    json::Document json() const { json::Document d;json::ParseDiagnostic diag;if(!json::Document::parse(body.v,d,diag))throw strut_checked_error("HttpError","response body is not valid JSON");return d; }
};
inline size_t strut_http_write_cb(char* p,size_t size,size_t nmemb,void* u){auto* body=static_cast<std::string*>(u);body->append(p,size*nmemb);return size*nmemb;}
inline size_t strut_http_header_cb(char* p,size_t size,size_t nmemb,void* u){const size_t n=size*nmemb;std::string line(p,n);auto* headers=static_cast<std::unordered_map<strut_string,strut_string>*>(u);auto colon=line.find(':');if(colon!=std::string::npos){std::string k=line.substr(0,colon),v=line.substr(colon+1);while(!v.empty()&&(v.front()==' '||v.front()=='\t'))v.erase(v.begin());while(!v.empty()&&(v.back()=='\r'||v.back()=='\n'||v.back()==' '||v.back()=='\t'))v.pop_back();(*headers)[strut_string(k)]=strut_string(v);}return n;}
inline strut_http_response http_request(const strut_string& method,const strut_string& url,const json::Document& options){strut_curl_init();CURL* c=curl_easy_init();if(!c)throw strut_checked_error("HttpError","curl_easy_init failed");strut_http_response response;curl_slist* list=nullptr;std::string body;long timeout=30000;bool follow=true;if(options.type!=json::Type::Null&&options.type!=json::Type::Object){curl_easy_cleanup(c);throw strut_checked_error("HttpError","HTTP options must be JSON object or null");}if(options.type==json::Type::Object){if(options.has("timeout_ms")&&options["timeout_ms"].type==json::Type::Number)timeout=static_cast<long>(options["timeout_ms"].num);if(options.has("follow_redirects")&&options["follow_redirects"].type==json::Type::Boolean)follow=options["follow_redirects"].boolean;if(options.has("body")){const auto& b=options["body"];body=b.type==json::Type::String?b.string:b.dump();}if(options.has("headers")){const auto& h=options["headers"];if(h.type!=json::Type::Object){curl_easy_cleanup(c);throw strut_checked_error("HttpError","headers must be JSON object");}for(const auto& kv:h.object){if(kv.second.type!=json::Type::String){curl_easy_cleanup(c);throw strut_checked_error("HttpError","header values must be strings");}const std::string line=kv.first+": "+kv.second.string;list=curl_slist_append(list,line.c_str());}}}
curl_easy_setopt(c,CURLOPT_URL,url.v.c_str());curl_easy_setopt(c,CURLOPT_CUSTOMREQUEST,method.v.c_str());curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,follow?1L:0L);curl_easy_setopt(c,CURLOPT_MAXREDIRS,10L);curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,timeout);curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,strut_http_write_cb);curl_easy_setopt(c,CURLOPT_WRITEDATA,&response.body.v);curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,strut_http_header_cb);curl_easy_setopt(c,CURLOPT_HEADERDATA,&response.headers);if(list)curl_easy_setopt(c,CURLOPT_HTTPHEADER,list);if(!body.empty()){curl_easy_setopt(c,CURLOPT_POSTFIELDS,body.data());curl_easy_setopt(c,CURLOPT_POSTFIELDSIZE,static_cast<long>(body.size()));}auto rc=curl_easy_perform(c);if(rc==CURLE_OK){long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);response.status=static_cast<std::int32_t>(status);}if(list)curl_slist_free_all(list);curl_easy_cleanup(c);if(rc!=CURLE_OK)throw strut_checked_error("HttpError",curl_easy_strerror(rc));return response;}
inline strut_http_response http_request(const strut_string& method,const strut_string& url){return http_request(method,url,json::Document(nullptr));}
inline strut_http_response http_get(const strut_string& url){return http_request(strut_string("GET"),url);}
inline strut_http_response http_get_ca(const strut_string& url,const strut_string& ca_file){strut_curl_init();CURL* c=curl_easy_init();if(!c)throw strut_checked_error("HttpError","curl_easy_init failed");strut_http_response response;curl_easy_setopt(c,CURLOPT_URL,url.v.c_str());curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_CAINFO,ca_file.v.c_str());curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,30000L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,strut_http_write_cb);curl_easy_setopt(c,CURLOPT_WRITEDATA,&response.body.v);curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,strut_http_header_cb);curl_easy_setopt(c,CURLOPT_HEADERDATA,&response.headers);auto rc=curl_easy_perform(c);if(rc==CURLE_OK){long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);response.status=static_cast<std::int32_t>(status);}curl_easy_cleanup(c);if(rc!=CURLE_OK)throw strut_checked_error("HttpError",curl_easy_strerror(rc));return response;}
inline json::Document http_get_json(const strut_string& url){return http_get(url).json();}
)HTTP_CLIENT";
    if (async) out << R"HTTP_CLIENT(
inline strut_future<strut_http_response> http_get_async(const strut_string& url){return strut_async([url]{return http_get(url);});}
inline strut_future<strut_http_response> http_request_async(const strut_string& method,const strut_string& url,const json::Document& options){return strut_async([method,url,options]{return http_request(method,url,options);});}
)HTTP_CLIENT";
}

void emit_http_server_types(std::ostream& out, bool json) {
    out << R"STRUT_HTTP_TYPES(
struct strut_server_request {strut_string method,path,body;std::unordered_map<strut_string,strut_string> headers,query,params;
)STRUT_HTTP_TYPES";
    if (json) out << "    json::Document json() const{return strut_json_parse(body);}\n";
    out << R"STRUT_HTTP_TYPES(};
struct strut_server_response {std::int32_t status=200;strut_string body;strut_string content_type="text/plain; charset=utf-8";std::unordered_map<strut_string,strut_string> headers;};
inline strut_server_response strut_http_text(const strut_string& s){return {200,s,"text/plain; charset=utf-8",{}};}
inline strut_server_response strut_http_html(const strut_string& s){return {200,s,"text/html; charset=utf-8",{}};}
)STRUT_HTTP_TYPES";
    if (json) out << "inline strut_server_response strut_http_json_response(const json::Document& j){return {200,strut_string(j.dump()),\"application/json\",{}};}\n";
    out << R"STRUT_HTTP_TYPES(
inline std::string strut_trim_ascii(std::string s){while(!s.empty()&&(s.back()=='\r'||s.back()==' '||s.back()=='\t'))s.pop_back();std::size_t i=0;while(i<s.size()&&(s[i]==' '||s[i]=='\t'))++i;return s.substr(i);}
inline void strut_parse_query(const std::string& raw,std::unordered_map<strut_string,strut_string>& out){std::size_t p=0;while(p<=raw.size()){auto amp=raw.find('&',p);auto part=raw.substr(p,amp==std::string::npos?std::string::npos:amp-p);auto eq=part.find('=');out[strut_string(part.substr(0,eq))]=strut_string(eq==std::string::npos?"":part.substr(eq+1));if(amp==std::string::npos)break;p=amp+1;}}
enum class strut_http_version{http_1_0,http_1_1};
struct strut_http_header_field{std::string name,lower_name,value;};
struct strut_http_request_head{strut_server_request request;strut_http_version version=strut_http_version::http_1_1;std::size_t content_length=0;std::vector<strut_http_header_field> fields;};
struct strut_http_head_result{strut_http_request_head head;std::int32_t status=0;const char* message=nullptr;explicit operator bool() const{return status==0;}};
inline bool strut_http_token_char(unsigned char c){return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='!'||c=='#'||c=='$'||c=='%'||c=='&'||c=='\''||c=='*'||c=='+'||c=='-'||c=='.'||c=='^'||c=='_'||c=='`'||c=='|'||c=='~';}
inline bool strut_http_token(const std::string& value){return !value.empty()&&std::all_of(value.begin(),value.end(),[](unsigned char c){return strut_http_token_char(c);});}
inline std::string strut_http_lower(std::string value){for(char& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
inline bool strut_http_field_value(const std::string& value){return std::all_of(value.begin(),value.end(),[](unsigned char c){return c!='\0'&&c!='\r'&&c!='\n'&&c!=0x7f&&(c>=0x20||c=='\t');});}
inline bool strut_http_reserved_response_field(const std::string& lower){return lower=="content-type"||lower=="content-length"||lower=="transfer-encoding"||lower=="connection";}
inline const char* strut_http_reason(std::int32_t status){if(status==200)return "OK";if(status==201)return "Created";if(status==204)return "No Content";if(status==301)return "Moved Permanently";if(status==302)return "Found";if(status==304)return "Not Modified";if(status==400)return "Bad Request";if(status==404)return "Not Found";if(status==405)return "Method Not Allowed";if(status==413)return "Payload Too Large";if(status==414)return "URI Too Long";if(status==431)return "Request Header Fields Too Large";if(status==500)return "Internal Server Error";if(status==501)return "Not Implemented";if(status==503)return "Service Unavailable";if(status==505)return "HTTP Version Not Supported";return "Response";}
inline bool strut_http_media_type(const std::string& value){if(!strut_http_field_value(value))return false;std::size_t position=0;auto token=[&](){const std::size_t start=position;while(position<value.size()&&strut_http_token_char(static_cast<unsigned char>(value[position])))++position;return position>start;};auto whitespace=[&](){while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;};if(!token()||position>=value.size()||value[position++]!='/'||!token())return false;for(;;){whitespace();if(position==value.size())return true;if(value[position++]!=';')return false;whitespace();if(!token()){return false;}whitespace();if(position>=value.size()||value[position++]!='=')return false;whitespace();if(position<value.size()&&value[position]=='"'){++position;bool closed=false;while(position<value.size()){const unsigned char c=static_cast<unsigned char>(value[position++]);if(c=='"'){closed=true;break;}if(c=='\\'){if(position>=value.size())return false;++position;}else if(c<0x20&&c!='\t')return false;}if(!closed)return false;}else if(!token())return false;}}
enum class strut_http_response_framing{known_length,chunked,close_delimited,no_body};
inline bool strut_serialize_http_response_head(std::int32_t status,const std::string& content_type,const std::unordered_map<strut_string,strut_string>& headers,strut_http_response_framing framing,std::size_t body_size,strut_http_version version,bool close_connection,std::string& serialized){const bool body_forbidden=status==204||status==205||status==304;if(status<200||status>599||!strut_http_media_type(content_type)||(body_forbidden!=(framing==strut_http_response_framing::no_body))||(version==strut_http_version::http_1_0&&framing==strut_http_response_framing::chunked))return false;std::vector<std::string> names;names.reserve(headers.size());for(const auto& header:headers){if(!strut_http_token(header.first.v)||!strut_http_field_value(header.second.v))return false;const std::string lower=strut_http_lower(header.first.v);if(strut_http_reserved_response_field(lower)||std::find(names.begin(),names.end(),lower)!=names.end())return false;names.push_back(lower);}std::ostringstream out;out<<(version==strut_http_version::http_1_1?"HTTP/1.1 ":"HTTP/1.0 ")<<status<<' '<<strut_http_reason(status)<<"\r\nContent-Type: "<<content_type<<"\r\n";if(framing==strut_http_response_framing::known_length)out<<"Content-Length: "<<body_size<<"\r\n";else if(framing==strut_http_response_framing::chunked)out<<"Transfer-Encoding: chunked\r\n";else if(framing==strut_http_response_framing::no_body&&status==205)out<<"Content-Length: 0\r\n";if(close_connection)out<<"Connection: close\r\n";for(const auto& header:headers)out<<header.first.v<<": "<<header.second.v<<"\r\n";out<<"\r\n";serialized=out.str();return true;}
enum class strut_http_response_phase{uncommitted,committed,finished,aborted};
struct strut_http_response_writer_state{std::mutex mutex;std::function<void(const char*,std::size_t)> send;strut_http_version version=strut_http_version::http_1_1;strut_http_response_phase phase=strut_http_response_phase::uncommitted;strut_http_response_framing framing=strut_http_response_framing::close_delimited;std::int32_t status=200;std::string content_type="text/plain; charset=utf-8";std::unordered_map<strut_string,strut_string> headers;std::size_t declared_length=0,written=0;bool has_declared_length=false,head_started=false,active=true;};
class strut_http_response_writer{
public:
    strut_http_response_writer()=default;explicit strut_http_response_writer(std::shared_ptr<strut_http_response_writer_state> state):state_(std::move(state)){}
    void status(std::int32_t value) const{mutate([&](auto& s){s.status=value;});}
    void header(const strut_string& name,const strut_string& value) const{mutate([&](auto& s){if(s.headers.find(name)!=s.headers.end())fail("duplicate HTTP response header");s.headers.emplace(name,value);});}
    void content_type(const strut_string& value) const{mutate([&](auto& s){s.content_type=value.v;});}
    void content_length(std::int64_t value) const{if(value<0||static_cast<std::uint64_t>(value)>std::numeric_limits<std::size_t>::max())fail("invalid HTTP response content length");mutate([&](auto& s){s.has_declared_length=true;s.declared_length=static_cast<std::size_t>(value);});}
    void write(const strut_string& data) const{write_native(data.v.data(),data.v.size());}
    void write_bytes(const strut_bytes& data) const{write_native(reinterpret_cast<const char*>(data.data()),data.native_size());}
    void flush() const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);require_active(*s);if(s->phase==strut_http_response_phase::finished)return;if(s->phase==strut_http_response_phase::aborted)fail("HTTP response is aborted");commit_locked(*s);}
    void finish() const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);require_active(*s);if(s->phase==strut_http_response_phase::finished)return;if(s->phase==strut_http_response_phase::aborted)fail("HTTP response is aborted");if(s->has_declared_length&&s->written!=s->declared_length)fail("HTTP response body does not match declared content length");commit_locked(*s);s->phase=strut_http_response_phase::finished;if(s->framing==strut_http_response_framing::chunked)send_locked(*s,"0\r\n\r\n",5);}
    bool committed() const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);return s->head_started;}
    bool abort() const{if(!state_)return false;std::lock_guard<std::mutex> lock(state_->mutex);const bool committed=state_->head_started;state_->phase=strut_http_response_phase::aborted;state_->active=false;state_->send={};return committed;}
    void invalidate() const{if(!state_)return;std::lock_guard<std::mutex> lock(state_->mutex);state_->active=false;state_->send={};}
private:
    [[noreturn]] static void fail(const char* message){throw strut_checked_error("NetworkError",message);}
    std::shared_ptr<strut_http_response_writer_state> require() const{if(!state_)fail("HTTP response writer is not initialized");return state_;}
    static void require_active(const strut_http_response_writer_state& s){if(!s.active)fail("HTTP response writer is no longer active");}
    template<class F>void mutate(F change) const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);require_active(*s);if(s->phase!=strut_http_response_phase::uncommitted)fail("HTTP response metadata is already committed");change(*s);}
    static void send_locked(strut_http_response_writer_state& s,const char* data,std::size_t size){try{s.send(data,size);}catch(...){s.phase=strut_http_response_phase::aborted;throw;}}
    static void commit_locked(strut_http_response_writer_state& s){if(s.phase!=strut_http_response_phase::uncommitted)return;const bool no_body=s.status==204||s.status==205||s.status==304;s.framing=no_body?strut_http_response_framing::no_body:s.has_declared_length?strut_http_response_framing::known_length:s.version==strut_http_version::http_1_1?strut_http_response_framing::chunked:strut_http_response_framing::close_delimited;std::string head;if(!strut_serialize_http_response_head(s.status,s.content_type,s.headers,s.framing,s.declared_length,s.version,true,head))fail("invalid HTTP response metadata");s.phase=strut_http_response_phase::committed;s.head_started=true;send_locked(s,head.data(),head.size());}
    void write_native(const char* data,std::size_t size) const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);require_active(*s);if(s->phase==strut_http_response_phase::finished)fail("HTTP response is finished");if(s->phase==strut_http_response_phase::aborted)fail("HTTP response is aborted");const bool no_body=s->status==204||s->status==205||s->status==304;if(no_body&&size!=0)fail("HTTP status forbids a response body");if(s->has_declared_length&&(s->written>s->declared_length||size>s->declared_length-s->written))fail("HTTP response exceeds declared content length");commit_locked(*s);if(size==0)return;if(s->framing==strut_http_response_framing::chunked){std::ostringstream chunk;chunk<<std::hex<<size<<"\r\n";const std::string prefix=chunk.str();send_locked(*s,prefix.data(),prefix.size());send_locked(*s,data,size);send_locked(*s,"\r\n",2);}else send_locked(*s,data,size);s->written+=size;}
    std::shared_ptr<strut_http_response_writer_state> state_;
};
inline bool strut_http_uri_char(unsigned char c,bool query){if((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z'))return true;const std::string allowed=query?"-._~!$&'()*+,;=:@/?":"-._~!$&'()*+,;=:@/";return allowed.find(static_cast<char>(c))!=std::string::npos;}
inline bool strut_http_target(const std::string& target){if(target.empty()||target.front()!='/')return false;bool query=false;for(std::size_t i=0;i<target.size();++i){const unsigned char c=static_cast<unsigned char>(target[i]);if(c=='?'){query=true;continue;}if(c=='%'){if(i+2>=target.size()||!std::isxdigit(static_cast<unsigned char>(target[i+1]))||!std::isxdigit(static_cast<unsigned char>(target[i+2])))return false;i+=2;continue;}if(!strut_http_uri_char(c,query))return false;}return true;}
inline bool strut_http_port(const std::string& value){if(value.empty())return false;unsigned port=0;for(unsigned char c:value){if(c<'0'||c>'9')return false;port=port*10+static_cast<unsigned>(c-'0');if(port>65535)return false;}return true;}
inline bool strut_http_reg_name(const std::string& value){if(value.empty())return false;for(std::size_t i=0;i<value.size();++i){const unsigned char c=static_cast<unsigned char>(value[i]);if(c=='%'){if(i+2>=value.size()||!std::isxdigit(static_cast<unsigned char>(value[i+1]))||!std::isxdigit(static_cast<unsigned char>(value[i+2])))return false;i+=2;continue;}if(!((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||std::string("-._~!$&'()*+;=").find(static_cast<char>(c))!=std::string::npos))return false;}return true;}
inline bool strut_http_ip(int family,const std::string& value,void* address){
#ifdef _WIN32
    return InetPtonA(family,value.c_str(),address)==1;
#else
    return inet_pton(family,value.c_str(),address)==1;
#endif
}
inline bool strut_http_host(const std::string& value){if(value.empty())return false;std::string host,port;if(value.front()=='['){const auto close=value.find(']');if(close==std::string::npos||close==1||value.find('[',1)!=std::string::npos||value.find(']',close+1)!=std::string::npos)return false;host=value.substr(1,close-1);unsigned char address[16]{};if(!strut_http_ip(AF_INET6,host,address))return false;if(close+1<value.size()){if(value[close+1]!=':')return false;port=value.substr(close+2);}}else{const auto colon=value.rfind(':');if(colon!=std::string::npos){if(value.find(':')!=colon)return false;host=value.substr(0,colon);port=value.substr(colon+1);}else host=value;if(!strut_http_reg_name(host))return false;const bool numeric=host.find_first_not_of("0123456789.")==std::string::npos&&host.find('.')!=std::string::npos;if(numeric){unsigned char address[4]{};if(!strut_http_ip(AF_INET,host,address))return false;}}return port.empty()?(value.back()!=':'):strut_http_port(port);}
inline bool strut_http_transfer_encoding(const std::string& value){std::size_t position=0;bool chunked=false;for(;;){while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;const std::size_t start=position;while(position<value.size()&&strut_http_token_char(static_cast<unsigned char>(value[position])))++position;if(start==position)return false;const std::string coding=strut_http_lower(value.substr(start,position-start));bool parameters=false;while(true){while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;if(position>=value.size()||value[position]==',')break;if(value[position++]!=';')return false;parameters=true;while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;const std::size_t name=position;while(position<value.size()&&strut_http_token_char(static_cast<unsigned char>(value[position])))++position;if(name==position)return false;while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;if(position>=value.size()||value[position++]!='=')return false;while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;if(position<value.size()&&value[position]=='\"'){++position;bool closed=false;while(position<value.size()){const unsigned char c=static_cast<unsigned char>(value[position++]);if(c=='\"'){closed=true;break;}if(c=='\\'){if(position>=value.size())return false;++position;}else if(c<0x20&&c!='\t')return false;}if(!closed)return false;}else{const std::size_t token=position;while(position<value.size()&&strut_http_token_char(static_cast<unsigned char>(value[position])))++position;if(token==position)return false;}}
        if(coding=="chunked"){if(chunked||parameters)return false;chunked=true;}while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;if(position==value.size())return chunked;if(value[position++]!=','||chunked)return false;}
}
inline strut_http_head_result strut_parse_http_request_head(const std::string& bytes,std::size_t max_body_bytes,std::int32_t max_header_count){
    strut_http_head_result result;auto fail=[&](std::int32_t status,const char* message){result.status=status;result.message=message;return result;};
    const auto line_end=bytes.find("\r\n");if(line_end==std::string::npos)return fail(400,"Bad Request");if(line_end>8192)return fail(414,"URI Too Long");const std::string line=bytes.substr(0,line_end);const auto first=line.find(' '),second=first==std::string::npos?std::string::npos:line.find(' ',first+1);if(first==std::string::npos||second==std::string::npos||line.find(' ',second+1)!=std::string::npos||first==0||second==first+1||second+1==line.size())return fail(400,"Bad Request");
    const std::string method=line.substr(0,first),target=line.substr(first+1,second-first-1),version=line.substr(second+1);if(!strut_http_token(method)||!strut_http_target(target))return fail(400,"Bad Request");if(version=="HTTP/1.1")result.head.version=strut_http_version::http_1_1;else if(version=="HTTP/1.0")result.head.version=strut_http_version::http_1_0;else if(version.size()==8&&version.rfind("HTTP/",0)==0&&std::isdigit(static_cast<unsigned char>(version[5]))&&version[6]=='.'&&std::isdigit(static_cast<unsigned char>(version[7])))return fail(505,"HTTP Version Not Supported");else return fail(400,"Bad Request");
    result.head.request.method=strut_string(method);const auto query=target.find('?');result.head.request.path=strut_string(target.substr(0,query));if(query!=std::string::npos)strut_parse_query(target.substr(query+1),result.head.request.query);
    std::size_t position=line_end+2;std::int32_t count=0,host_count=0,content_length_count=0,transfer_encoding_count=0;bool transfer_encoding_valid=true,body_too_large=false;
    while(position<bytes.size()){
        const auto end=bytes.find("\r\n",position);if(end==std::string::npos)return fail(400,"Bad Request");if(end-position>8192)return fail(431,"Request Header Fields Too Large");const std::string header=bytes.substr(position,end-position);if(header.empty()||header.front()==' '||header.front()=='\t')return fail(400,"Bad Request");if(count>=max_header_count)return fail(431,"Request Header Fields Too Large");++count;const auto colon=header.find(':');if(colon==std::string::npos||colon==0||!strut_http_token(header.substr(0,colon)))return fail(400,"Bad Request");const std::string name=header.substr(0,colon),lower=strut_http_lower(name),value=strut_trim_ascii(header.substr(colon+1));for(unsigned char c:value)if(c==0x7f||(c<0x20&&c!='\t'))return fail(400,"Bad Request");if(std::any_of(result.head.fields.begin(),result.head.fields.end(),[&](const auto& field){return field.lower_name==lower;}))return fail(400,"Bad Request");result.head.fields.push_back({name,lower,value});result.head.request.headers[strut_string(lower)]=strut_string(value);
        if(lower=="host"){++host_count;if(!strut_http_host(value))return fail(400,"Bad Request");}
        else if(lower=="content-length"){
            if(++content_length_count>1||value.empty())return fail(400,"Bad Request");std::size_t length=0;for(unsigned char c:value){if(c<'0'||c>'9')return fail(400,"Bad Request");const std::size_t digit=static_cast<std::size_t>(c-'0');if(body_too_large||length>max_body_bytes/10||(length==max_body_bytes/10&&digit>max_body_bytes%10)){body_too_large=true;continue;}length=length*10+digit;}result.head.content_length=length;
        }else if(lower=="transfer-encoding"){
            if(++transfer_encoding_count>1||value.empty())return fail(400,"Bad Request");transfer_encoding_valid=strut_http_transfer_encoding(value);
        }
        position=end+2;
    }
    if((result.head.version==strut_http_version::http_1_1&&host_count!=1)||(result.head.version==strut_http_version::http_1_0&&host_count>1))return fail(400,"Bad Request");if(transfer_encoding_count&&content_length_count)return fail(400,"Bad Request");if(transfer_encoding_count){if(!transfer_encoding_valid||result.head.version==strut_http_version::http_1_0)return fail(400,"Bad Request");return fail(501,"Not Implemented");}if(body_too_large)return fail(413,"Payload Too Large");return result;
}
inline bool strut_route_match(const std::string& pattern,const std::string& path,std::unordered_map<strut_string,strut_string>& params){std::stringstream a(pattern),b(path);std::string x,y;while(true){bool ax=static_cast<bool>(std::getline(a,x,'/')),by=static_cast<bool>(std::getline(b,y,'/'));if(!ax||!by)return ax==by;if(x.empty()&&y.empty())continue;if(!x.empty()&&x[0]==':')params[strut_string(x.substr(1))]=strut_string(y);else if(x!=y)return false;}}
)STRUT_HTTP_TYPES";
}

void emit_http_server(std::ostream& out, bool async_handlers, bool tls) {
    if (tls) out << "#define STRUT_USE_SERVER_TLS 1\n#include <openssl/ssl.h>\n#include <openssl/err.h>\n#include <cerrno>\n#ifndef _WIN32\n#include <fcntl.h>\n#include <poll.h>\n#endif\n";
    out << R"STRUT_SERVER(
#include <atomic>
#include <csignal>
#include <deque>
#ifndef STRUT_EXECUTION_CONTEXT_DEFINED
#define STRUT_EXECUTION_CONTEXT_DEFINED
inline thread_local const void* strut_execution_context=nullptr;
#endif
#ifndef _WIN32
#ifndef STRUT_SIGPIPE_MUTEX_DEFINED
#define STRUT_SIGPIPE_MUTEX_DEFINED
inline std::mutex strut_sigpipe_mutex;
#endif
#endif
class strut_http_server {
public:
    using handler=std::function<strut_server_response(strut_server_request)>;
    using stream_handler=std::function<void(strut_server_request,strut_http_response_writer)>;
    strut_http_server():s_(std::make_shared<state>()){}
    void get(const strut_string& path,handler h) const{add_route("GET",path,std::move(h));}
    void post(const strut_string& path,handler h) const{add_route("POST",path,std::move(h));}
    void get_stream(const strut_string& path,stream_handler h) const{add_stream_route("GET",path,std::move(h));}
    void post_stream(const strut_string& path,stream_handler h) const{add_stream_route("POST",path,std::move(h));}
)STRUT_SERVER";
    if (async_handlers) out << R"STRUT_SERVER(
    void get_async(const strut_string& path,std::function<strut_future<strut_server_response>(strut_server_request)> h) const{get(path,[h=std::move(h)](strut_server_request r){return strut_await(h(std::move(r)));});}
    void post_async(const strut_string& path,std::function<strut_future<strut_server_response>(strut_server_request)> h) const{post(path,[h=std::move(h)](strut_server_request r){return strut_await(h(std::move(r)));});}
)STRUT_SERVER";
    out << R"STRUT_SERVER(
    void serve_static(const strut_string& prefix,const std::unordered_map<strut_string,strut_string>& files,const strut_string& fallback=strut_string()) const{std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("configure static files");s_->static_prefix=prefix.v;s_->static_files=files;s_->static_fallback=fallback.v;}
    void timeouts(std::int32_t read_ms,std::int32_t write_ms,std::int32_t idle_ms,std::int32_t shutdown_ms) const{if(read_ms<=0||write_ms<=0||idle_ms<=0||shutdown_ms<0)throw strut_checked_error("NetworkError","HTTP timeouts must be positive (shutdown may be zero)");std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("configure timeouts");s_->read_timeout_ms=read_ms;s_->write_timeout_ms=write_ms;s_->idle_timeout_ms=idle_ms;s_->shutdown_timeout_ms=shutdown_ms;}
    void limits(std::int64_t body_bytes,std::int64_t header_bytes,std::int32_t header_count,std::int32_t connections) const{if(body_bytes<0||header_bytes<1024||header_count<=0||connections<=0||static_cast<std::uint64_t>(body_bytes)>std::numeric_limits<std::size_t>::max()||static_cast<std::uint64_t>(header_bytes)>std::numeric_limits<std::size_t>::max())throw strut_checked_error("NetworkError","invalid HTTP server limits");std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("configure limits");s_->max_body_bytes=static_cast<std::size_t>(body_bytes);s_->max_header_bytes=static_cast<std::size_t>(header_bytes);s_->max_header_count=header_count;s_->max_connections=connections;}
    bool running() const{return s_->running.load();}
    void stop() const{std::shared_ptr<run_state> run;{std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);if(s_->phase==lifecycle_phase::stopped)return;run=s_->current;s_->phase=lifecycle_phase::stopping;}if(!run)return;request_stop(run);if(worker_run_!=run.get()&&strut_execution_context!=run.get()){wait_for_drain(run);publish_lifecycle(s_,run);}}
    void listen(const strut_string& host,std::int32_t port,std::int32_t max_requests=0) const{
#ifndef _WIN32
        {std::lock_guard<std::mutex> signal_lock(strut_sigpipe_mutex);std::signal(SIGPIPE,SIG_IGN);}
#endif
        auto s=s_;run_server(host,port,max_requests,[s](strut_tcp_socket& socket){serve_one(s,socket);},[s](strut_tcp_socket& socket){set_socket_timeouts(socket,s->write_timeout_ms,s->write_timeout_ms);send_error(socket,503,"Service Unavailable");});
    }
)STRUT_SERVER";
    if (tls) out << R"STRUT_SERVER(
private:
    class tls_socket{public:tls_socket(strut_tcp_socket socket,SSL* ssl):socket_(std::move(socket)),ssl_(ssl){}tls_socket(const tls_socket&)=delete;tls_socket& operator=(const tls_socket&)=delete;tls_socket(tls_socket&& other) noexcept:socket_(std::move(other.socket_)),ssl_(other.ssl_){other.ssl_=nullptr;}~tls_socket(){close();}strut_socket_handle native_handle() const{return socket_.native_handle();}strut_socket_operation pin() const{return socket_.pin();}strut_string read(std::int64_t max_bytes=4096){if(max_bytes<=0)return {};auto operation=socket_.pin();std::string out(static_cast<std::size_t>(max_bytes),'\0');int n=SSL_read(ssl_,out.data(),static_cast<int>(out.size()));if(n==0)return {};if(n<0)throw strut_checked_error("TlsError","TLS request read failed");out.resize(static_cast<std::size_t>(n));return strut_string(std::move(out));}void write(const strut_string& data){auto operation=socket_.pin();std::size_t offset=0;while(offset<data.v.size()){const int amount=static_cast<int>(std::min<std::size_t>(data.v.size()-offset,static_cast<std::size_t>(std::numeric_limits<int>::max())));int n=SSL_write(ssl_,data.v.data()+offset,amount);if(n<=0)throw strut_checked_error("TlsError","TLS response write failed");offset+=static_cast<std::size_t>(n);}}void close(){if(ssl_){try{auto operation=socket_.pin();SSL_shutdown(ssl_);}catch(...){ }SSL_free(ssl_);ssl_=nullptr;}socket_.close();}private:strut_tcp_socket socket_;SSL* ssl_=nullptr;};
    static bool tls_accept_until(const strut_tcp_socket& socket,SSL* ssl,std::int32_t timeout_ms){auto operation=socket.pin();const auto handle=operation.handle;
#ifdef _WIN32
        u_long nonblocking=1;if(ioctlsocket(handle,FIONBIO,&nonblocking)!=0)return false;
#else
        const int flags=fcntl(handle,F_GETFL,0);if(flags<0||fcntl(handle,F_SETFL,flags|O_NONBLOCK)!=0)return false;
#endif
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);bool accepted=false;for(;;){ERR_clear_error();const int result=SSL_accept(ssl);if(result==1){accepted=std::chrono::steady_clock::now()<=deadline;break;}const int error=SSL_get_error(ssl,result);if(error!=SSL_ERROR_WANT_READ&&error!=SSL_ERROR_WANT_WRITE)break;const auto now=std::chrono::steady_clock::now();if(now>=deadline)break;const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-now+std::chrono::milliseconds(1)).count();
#ifdef _WIN32
        WSAPOLLFD descriptor{handle,static_cast<SHORT>(error==SSL_ERROR_WANT_WRITE?POLLOUT:POLLIN),0};const int selected=WSAPoll(&descriptor,1,static_cast<INT>(std::min<std::int64_t>(remaining,std::numeric_limits<int>::max())));if(selected==SOCKET_ERROR&&WSAGetLastError()==WSAEINTR)continue;
#else
        pollfd descriptor{handle,static_cast<short>(error==SSL_ERROR_WANT_WRITE?POLLOUT:POLLIN),0};const int selected=poll(&descriptor,1,static_cast<int>(std::min<std::int64_t>(remaining,std::numeric_limits<int>::max())));if(selected<0&&(errno==EINTR||errno==EAGAIN))continue;
#endif
        if(selected<=0)break;}
#ifdef _WIN32
        u_long blocking=0;if(ioctlsocket(handle,FIONBIO,&blocking)!=0)accepted=false;
#else
        if(fcntl(handle,F_SETFL,flags)!=0)accepted=false;
#endif
        return accepted;}
public:
    void listen_tls(const strut_string& host,std::int32_t port,const strut_string& certificate,const strut_string& private_key,std::int32_t max_requests=0) const{
#ifndef _WIN32
        {std::lock_guard<std::mutex> signal_lock(strut_sigpipe_mutex);std::signal(SIGPIPE,SIG_IGN);}
#endif
        SSL_CTX* raw=SSL_CTX_new(TLS_server_method());if(!raw)throw strut_checked_error("TlsError","unable to initialize the TLS server context");std::shared_ptr<SSL_CTX> context(raw,SSL_CTX_free);SSL_CTX_set_min_proto_version(raw,TLS1_2_VERSION);
        if(SSL_CTX_use_certificate_chain_file(raw,certificate.v.c_str())!=1)throw strut_checked_error("TlsError","unable to load TLS certificate chain from '"+certificate.v+"'");
        if(SSL_CTX_use_PrivateKey_file(raw,private_key.v.c_str(),SSL_FILETYPE_PEM)!=1)throw strut_checked_error("TlsError","unable to load TLS private key from '"+private_key.v+"'");
        if(SSL_CTX_check_private_key(raw)!=1)throw strut_checked_error("TlsError","TLS certificate and private key do not match");
        auto s=s_;run_server(host,port,max_requests,[s,context](strut_tcp_socket& socket){std::unique_ptr<SSL,decltype(&SSL_free)> ssl(SSL_new(context.get()),SSL_free);if(!ssl)return;if(SSL_set_fd(ssl.get(),static_cast<int>(socket.native_handle()))!=1||!tls_accept_until(socket,ssl.get(),std::min({s->read_timeout_ms,s->write_timeout_ms,s->idle_timeout_ms})))return;tls_socket secure(socket,ssl.release());serve_one(s,secure);},[](strut_tcp_socket& socket){socket.close();});
    }
)STRUT_SERVER";
    out << R"STRUT_SERVER(
private:
    struct route{std::string method,path;handler fn;stream_handler stream;};
    enum class lifecycle_phase{stopped,starting,running,stopping};
    struct connection{explicit connection(strut_tcp_socket value):socket(std::move(value)){}strut_tcp_socket socket;bool running=false;};
    struct run_state{std::mutex mutex;std::condition_variable work_cv,drain_cv;std::deque<std::shared_ptr<connection>> queue,connections;std::shared_ptr<connection> rejecting;strut_tcp_listener listener;std::size_t in_flight=0;bool accepting=true,startup_complete=false,was_running=false,workers_stopping=false,forced=false,deadline_set=false;std::chrono::steady_clock::time_point deadline;std::int32_t shutdown_timeout_ms=0;};
    struct state{std::vector<route> routes;std::string static_prefix,static_fallback;std::unordered_map<strut_string,strut_string> static_files;std::atomic<bool> running{false};std::mutex lifecycle_mutex;std::shared_ptr<run_state> current;lifecycle_phase phase=lifecycle_phase::stopped;std::int32_t read_timeout_ms=30000,write_timeout_ms=30000,idle_timeout_ms=5000,shutdown_timeout_ms=5000,max_header_count=100,max_connections=1024;std::size_t max_body_bytes=1024*1024,max_header_bytes=64*1024;};
    std::shared_ptr<state> s_;
    inline static thread_local run_state* worker_run_=nullptr;
    void require_stopped_locked(const char* action) const{if(s_->phase!=lifecycle_phase::stopped)throw strut_checked_error("NetworkError",std::string("cannot ")+action+" while HTTP server is running");if(s_->current){std::lock_guard<std::mutex> run_lock(s_->current->mutex);if(s_->current->in_flight!=0)throw strut_checked_error("NetworkError",std::string("cannot ")+action+" while HTTP server shutdown is still in progress");}}
    void add_route(const char* method,const strut_string& path,handler h) const{std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("register routes");s_->routes.push_back({method,path.v,std::move(h),{}});}
    void add_stream_route(const char* method,const strut_string& path,stream_handler h) const{std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("register routes");s_->routes.push_back({method,path.v,{},std::move(h)});}
    static void request_stop(const std::shared_ptr<run_state>& run){{std::lock_guard<std::mutex> lock(run->mutex);run->accepting=false;if(!run->deadline_set){run->deadline_set=true;run->deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(run->shutdown_timeout_ms);}if(run->rejecting)run->rejecting->socket.close();}run->listener.close();run->work_cv.notify_all();}
    static void force_shutdown_locked(const std::shared_ptr<run_state>& run){if(run->forced)return;run->forced=true;while(!run->queue.empty()){auto connection=run->queue.front();run->queue.pop_front();connection->socket.close();auto found=std::find(run->connections.begin(),run->connections.end(),connection);if(found!=run->connections.end())run->connections.erase(found,found+1);--run->in_flight;}for(const auto& connection:run->connections)if(connection->running)connection->socket.close();run->workers_stopping=true;run->work_cv.notify_all();run->drain_cv.notify_all();}
    static bool wait_for_drain(const std::shared_ptr<run_state>& run){std::unique_lock<std::mutex> lock(run->mutex);if(!run->deadline_set){run->deadline_set=true;run->deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(run->shutdown_timeout_ms);}bool drained=run->drain_cv.wait_until(lock,run->deadline,[&]{return run->startup_complete&&run->in_flight==0;});if(!drained){force_shutdown_locked(run);drained=run->startup_complete&&run->in_flight==0;}else{run->workers_stopping=true;run->work_cv.notify_all();}return drained;}
    static void publish_lifecycle(const std::shared_ptr<state>& s,const std::shared_ptr<run_state>& run){std::lock_guard<std::mutex> lifecycle_lock(s->lifecycle_mutex);if(s->current!=run)return;std::lock_guard<std::mutex> run_lock(run->mutex);const bool drained=run->startup_complete&&run->in_flight==0;s->phase=drained?lifecycle_phase::stopped:lifecycle_phase::stopping;s->running.store(!drained&&run->was_running);}
    static void worker_loop(const std::shared_ptr<state>& s,const std::shared_ptr<run_state>& run,const std::function<void(strut_tcp_socket&)>& process){for(;;){std::shared_ptr<connection> connection;{std::unique_lock<std::mutex> lock(run->mutex);run->work_cv.wait(lock,[&]{return run->workers_stopping||!run->queue.empty();});if(run->queue.empty()){if(run->workers_stopping)return;continue;}connection=run->queue.front();run->queue.pop_front();connection->running=true;}const void* previous_context=strut_execution_context;worker_run_=run.get();strut_execution_context=run.get();try{process(connection->socket);}catch(...){connection->socket.close();}strut_execution_context=previous_context;worker_run_=nullptr;bool retired_drained=false;{std::lock_guard<std::mutex> lock(run->mutex);auto found=std::find(run->connections.begin(),run->connections.end(),connection);if(found!=run->connections.end())run->connections.erase(found);if(run->in_flight>0)--run->in_flight;if(run->in_flight==0){retired_drained=run->forced;run->drain_cv.notify_all();}}if(retired_drained)publish_lifecycle(s,run);}}
    void run_server(const strut_string& host,std::int32_t port,std::int32_t max_requests,const std::function<void(strut_tcp_socket&)>& process,const std::function<void(strut_tcp_socket&)>& reject) const{auto s=s_;auto run=std::make_shared<run_state>();{std::lock_guard<std::mutex> lock(s->lifecycle_mutex);require_stopped_locked("start HTTP server");run->shutdown_timeout_ms=s->shutdown_timeout_ms;s->current=run;s->phase=lifecycle_phase::starting;}strut_tcp_listener listener;try{listener=tcp_listen(host,port);}catch(...){{std::lock_guard<std::mutex> lock(run->mutex);run->startup_complete=true;run->drain_cv.notify_all();}std::lock_guard<std::mutex> lock(s->lifecycle_mutex);if(s->current==run){s->current.reset();s->phase=lifecycle_phase::stopped;s->running.store(false);}throw;}bool accepting=false;{std::lock_guard<std::mutex> lock(run->mutex);accepting=run->accepting;if(accepting)run->listener=std::move(listener);else listener.close();run->startup_complete=true;run->drain_cv.notify_all();}{std::lock_guard<std::mutex> lock(s->lifecycle_mutex);if(s->current==run&&s->phase==lifecycle_phase::starting&&accepting){std::lock_guard<std::mutex> run_lock(run->mutex);run->was_running=true;s->phase=lifecycle_phase::running;s->running.store(true);}else accepting=false;}std::vector<std::thread> workers;std::uint64_t admitted=0;std::exception_ptr failure;if(accepting)try{while(max_requests<=0||admitted<static_cast<std::uint64_t>(max_requests)){strut_tcp_socket socket;try{socket=run->listener.accept();}catch(...){std::lock_guard<std::mutex> lock(run->mutex);if(!run->accepting)break;throw;}auto connection=std::make_shared<strut_http_server::connection>(std::move(socket));bool saturated=false;{std::lock_guard<std::mutex> lock(run->mutex);if(!run->accepting){connection->socket.close();break;}saturated=run->in_flight>=static_cast<std::size_t>(s->max_connections);if(saturated)run->rejecting=connection;else{if(workers.size()<=run->in_flight&&workers.size()<static_cast<std::size_t>(s->max_connections))workers.emplace_back([s,run,process]{worker_loop(s,run,process);});run->connections.push_back(connection);try{run->queue.push_back(connection);}catch(...){run->connections.pop_back();throw;}++run->in_flight;}}if(saturated){reject(connection->socket);std::lock_guard<std::mutex> lock(run->mutex);if(run->rejecting==connection)run->rejecting.reset();continue;}++admitted;run->work_cv.notify_one();}}catch(...){failure=std::current_exception();}request_stop(run);const bool drained=wait_for_drain(run);if(drained)publish_lifecycle(s,run);for(auto& worker:workers)if(worker.joinable()){if(drained)worker.join();else worker.detach();}publish_lifecycle(s,run);if(failure)std::rethrow_exception(failure);}
    template<class Socket> static void set_socket_timeouts(const Socket& socket,std::int32_t read_ms,std::int32_t write_ms){
        auto operation=socket.pin();const auto handle=operation.handle;
#ifdef _WIN32
        DWORD read=static_cast<DWORD>(read_ms),write=static_cast<DWORD>(write_ms);setsockopt(handle,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&read),sizeof(read));setsockopt(handle,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&write),sizeof(write));
#else
        timeval read{read_ms/1000,(read_ms%1000)*1000},write{write_ms/1000,(write_ms%1000)*1000};setsockopt(handle,SOL_SOCKET,SO_RCVTIMEO,&read,sizeof(read));setsockopt(handle,SOL_SOCKET,SO_SNDTIMEO,&write,sizeof(write));
#endif
    }
    template<class Socket> static strut_http_response_writer response_writer(Socket& socket,strut_http_version version){auto state=std::make_shared<strut_http_response_writer_state>();state->version=version;state->send=[&socket](const char* data,std::size_t size){socket.write(strut_string(std::string(data,size)));};return strut_http_response_writer(std::move(state));}
    static void write_buffered(const strut_http_response_writer& writer,const strut_server_response& response){writer.status(response.status);writer.content_type(response.content_type);for(const auto& header:response.headers)writer.header(header.first,header.second);writer.content_length(static_cast<std::int64_t>(response.body.v.size()));writer.write(response.body);writer.finish();}
    template<class Socket> static void send_error(Socket& socket,std::int32_t status,const char* message,strut_http_version version=strut_http_version::http_1_1){try{auto writer=response_writer(socket,version);strut_server_response response{status,strut_string(message),"text/plain; charset=utf-8",{}};write_buffered(writer,response);writer.invalidate();socket.close();}catch(...){socket.close();}}
    static strut_string mime(const std::string& p){auto dot=p.rfind('.');auto e=dot==std::string::npos?std::string():p.substr(dot);if(e==".html")return "text/html; charset=utf-8";if(e==".css")return "text/css; charset=utf-8";if(e==".js")return "application/javascript";if(e==".json")return "application/json";if(e==".svg")return "image/svg+xml";if(e==".png")return "image/png";return "application/octet-stream";}
    static std::string etag(const std::string& data){std::uint64_t h=1469598103934665603ull;for(unsigned char c:data){h^=c;h*=1099511628211ull;}std::ostringstream out;out<<'"'<<std::hex<<h<<'"';return out.str();}
    template<class Socket> static void serve_one(const std::shared_ptr<state>& s,Socket& socket){
        try{set_socket_timeouts(socket,std::min(s->read_timeout_ms,s->idle_timeout_ms),s->write_timeout_ms);std::string raw;std::size_t header_end=std::string::npos;for(;;){auto chunk=socket.read(4096).v;if(chunk.empty()){send_error(socket,400,"Bad Request");return;}raw+=chunk;header_end=raw.find("\r\n\r\n");const std::size_t inspected=header_end==std::string::npos?raw.size():header_end+4;bool invalid_line_end=false;for(std::size_t i=0;i<inspected;++i){if(raw[i]=='\n'&&(i==0||raw[i-1]!='\r'))invalid_line_end=true;if(raw[i]=='\r'&&i+1<inspected&&raw[i+1]!='\n')invalid_line_end=true;}if(invalid_line_end){send_error(socket,400,"Bad Request");return;}if(header_end!=std::string::npos){if(header_end+4>s->max_header_bytes){send_error(socket,431,"Request Header Fields Too Large");return;}break;}if(raw.size()>s->max_header_bytes){send_error(socket,431,"Request Header Fields Too Large");return;}}
            auto parsed=strut_parse_http_request_head(raw.substr(0,header_end+2),s->max_body_bytes,s->max_header_count);if(!parsed){send_error(socket,parsed.status,parsed.message,parsed.head.version);return;}const auto version=parsed.head.version;strut_server_request req=std::move(parsed.head.request);const std::size_t content_len=parsed.head.content_length;
            req.body=strut_string(raw.substr(header_end+4,std::min(content_len,raw.size()-(header_end+4))));while(req.body.v.size()<content_len){const auto remaining=content_len-req.body.v.size();auto more=socket.read(static_cast<std::int64_t>(std::min<std::size_t>(remaining,8192))).v;if(more.empty()){send_error(socket,400,"Bad Request",version);return;}if(more.size()>remaining)more.resize(remaining);req.body.v+=more;}
            strut_server_response response;bool found=false,method_mismatch=false;for(auto& route:s->routes){req.params.clear();if(!strut_route_match(route.path,req.path.v,req.params))continue;if(route.method!=req.method.v){method_mismatch=true;continue;}if(route.stream){auto writer=response_writer(socket,version);try{route.stream(req,writer);writer.finish();writer.invalidate();socket.close();}catch(...){const bool was_committed=writer.abort();if(was_committed)socket.close();else send_error(socket,500,"Internal Server Error",version);}return;}try{response=route.fn(req);}catch(const std::exception&){send_error(socket,500,"Internal Server Error",version);return;}catch(...){send_error(socket,500,"Internal Server Error",version);return;}found=true;break;}
            if(!found&&!s->static_files.empty()&&req.method.v=="GET"){std::string key=req.path.v;if(!s->static_prefix.empty()&&key.rfind(s->static_prefix,0)==0)key=key.substr(s->static_prefix.size());while(!key.empty()&&key.front()=='/')key.erase(key.begin());if(key.empty())key="index.html";if(key.find("..")!=std::string::npos){response.status=400;response.body="Bad Request";found=true;}else{auto it=s->static_files.find(strut_string(key));if(it==s->static_files.end()&&!s->static_fallback.empty())it=s->static_files.find(strut_string(s->static_fallback));if(it!=s->static_files.end()){response.status=200;response.body=it->second;response.content_type=mime(key);response.headers[strut_string("ETag")]=strut_string(etag(response.body.v));response.headers[strut_string("Cache-Control")]=strut_string("public, max-age=0, must-revalidate");found=true;}}}
            if(!found){response.status=method_mismatch?405:404;response.body=method_mismatch?"Method Not Allowed":"Not Found";}auto writer=response_writer(socket,version);try{write_buffered(writer,response);writer.invalidate();socket.close();}catch(...){const bool was_committed=writer.abort();if(was_committed)socket.close();else send_error(socket,500,"Internal Server Error",version);}
        }catch(...){socket.close();}
    }
};
)STRUT_SERVER";
}

} // namespace strut::generated_runtime
