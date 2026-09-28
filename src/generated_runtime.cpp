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
class strut_executor {
public:
    strut_executor(){auto n=std::thread::hardware_concurrency();if(n<2)n=2;for(unsigned i=0;i<n;++i)workers_.emplace_back([this]{worker();});}
    ~strut_executor(){{std::lock_guard<std::mutex> g(m_);stopping_=true;}cv_.notify_all();for(auto& t:workers_)if(t.joinable())t.join();}
    template<class F> auto submit(F&& f)->std::future<std::invoke_result_t<F>>{using R=std::invoke_result_t<F>;auto task=std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));auto fut=task->get_future();{std::lock_guard<std::mutex> g(m_);q_.emplace([task]{(*task)();});}cv_.notify_one();return fut;}
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
#ifdef _WIN32
using strut_socket_handle=SOCKET; constexpr strut_socket_handle strut_invalid_socket=INVALID_SOCKET;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket){shutdown(h,SD_BOTH);closesocket(h);}}
struct strut_winsock_runtime{strut_winsock_runtime(){WSADATA d{};if(WSAStartup(MAKEWORD(2,2),&d)!=0)throw strut_checked_error("NetworkError","WSAStartup failed");}~strut_winsock_runtime(){WSACleanup();}};
inline void strut_socket_init(){static strut_winsock_runtime runtime;(void)runtime;}
#else
using strut_socket_handle=int; constexpr strut_socket_handle strut_invalid_socket=-1;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket){::shutdown(h,SHUT_RDWR);::close(h);}}
inline void strut_socket_init(){}
#endif
struct strut_socket_state{strut_socket_handle handle=strut_invalid_socket;~strut_socket_state(){strut_socket_close(handle);}};
class strut_tcp_socket {
public:
    strut_tcp_socket():s_(std::make_shared<strut_socket_state>()){} explicit strut_tcp_socket(strut_socket_handle h):s_(std::make_shared<strut_socket_state>()){s_->handle=h;}
    bool is_open() const{return s_&&s_->handle!=strut_invalid_socket;}void close(){if(is_open()){strut_socket_close(s_->handle);s_->handle=strut_invalid_socket;}}
    void write(const strut_string& data){if(!is_open())throw strut_checked_error("NetworkError","write on closed socket");std::size_t off=0;while(off<data.v.size()){
#ifdef _WIN32
        int n=::send(s_->handle,data.v.data()+off,static_cast<int>(data.v.size()-off),0);
#else
        ssize_t n=::send(s_->handle,data.v.data()+off,data.v.size()-off,0);
#endif
        if(n<=0)throw strut_checked_error("NetworkError","socket write failed");off+=static_cast<std::size_t>(n);}}
    strut_string read(std::int64_t max_bytes=4096){if(!is_open())throw strut_checked_error("NetworkError","read on closed socket");if(max_bytes<=0)return {};std::string out(static_cast<std::size_t>(max_bytes),'\0');
#ifdef _WIN32
        int n=::recv(s_->handle,out.data(),static_cast<int>(out.size()),0);
#else
        ssize_t n=::recv(s_->handle,out.data(),out.size(),0);
#endif
        if(n<0)throw strut_checked_error("NetworkError","socket read failed");out.resize(static_cast<std::size_t>(n));return strut_string(std::move(out));}
    strut_socket_handle native_handle() const{return s_->handle;}
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
    strut_tcp_listener():s_(std::make_shared<strut_socket_state>()){} explicit strut_tcp_listener(strut_socket_handle h):s_(std::make_shared<strut_socket_state>()){s_->handle=h;}
    bool is_open() const{return s_&&s_->handle!=strut_invalid_socket;}void close(){if(is_open()){strut_socket_close(s_->handle);s_->handle=strut_invalid_socket;}}
    strut_tcp_socket accept(){if(!is_open())throw strut_checked_error("NetworkError","accept on closed listener");auto h=::accept(s_->handle,nullptr,nullptr);if(h==strut_invalid_socket)throw strut_checked_error("NetworkError","TCP accept failed");return strut_tcp_socket(h);}
)STRUT_TCP";
    if (async) out << "    strut_future<strut_tcp_socket> accept_async(){auto copy=*this;return strut_async([copy]() mutable{return copy.accept();});}\n";
    out << R"STRUT_TCP(
private:std::shared_ptr<strut_socket_state> s_;
};
inline strut_tcp_listener tcp_listen(const strut_string& host,std::int32_t port,std::int32_t backlog=128){strut_socket_init();if(port<1||port>65535)throw strut_checked_error("NetworkError","invalid TCP port");addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_flags=AI_PASSIVE;addrinfo* list=nullptr;const std::string service=std::to_string(port);const char* node=host.v.empty()?nullptr:host.v.c_str();if(getaddrinfo(node,service.c_str(),&hints,&list)!=0)throw strut_checked_error("NetworkError","listen address resolution failed");strut_socket_handle h=strut_invalid_socket;for(addrinfo* p=list;p;p=p->ai_next){h=::socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(h==strut_invalid_socket)continue;int yes=1;setsockopt(h,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&yes),sizeof(yes));if(::bind(h,p->ai_addr,static_cast<int>(p->ai_addrlen))==0&&::listen(h,backlog)==0)break;strut_socket_close(h);h=strut_invalid_socket;}freeaddrinfo(list);if(h==strut_invalid_socket)throw strut_checked_error("NetworkError","TCP listen failed: address unavailable or port already in use");return strut_tcp_listener(h);}
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
inline bool strut_route_match(const std::string& pattern,const std::string& path,std::unordered_map<strut_string,strut_string>& params){std::stringstream a(pattern),b(path);std::string x,y;while(true){bool ax=static_cast<bool>(std::getline(a,x,'/')),by=static_cast<bool>(std::getline(b,y,'/'));if(!ax||!by)return ax==by;if(x.empty()&&y.empty())continue;if(!x.empty()&&x[0]==':')params[strut_string(x.substr(1))]=strut_string(y);else if(x!=y)return false;}}
)STRUT_HTTP_TYPES";
}

void emit_http_server(std::ostream& out, bool async_handlers, bool tls) {
    if (tls) out << "#define STRUT_USE_SERVER_TLS 1\n#include <openssl/ssl.h>\n#include <openssl/err.h>\n";
    out << R"STRUT_SERVER(
#include <atomic>
#include <csignal>
#ifndef _WIN32
#ifndef STRUT_SIGPIPE_MUTEX_DEFINED
#define STRUT_SIGPIPE_MUTEX_DEFINED
inline std::mutex strut_sigpipe_mutex;
#endif
#endif
class strut_http_server {
public:
    using handler=std::function<strut_server_response(strut_server_request)>;
    strut_http_server():s_(std::make_shared<state>()){}
    void get(const strut_string& path,handler h) const{add_route("GET",path,std::move(h));}
    void post(const strut_string& path,handler h) const{add_route("POST",path,std::move(h));}
)STRUT_SERVER";
    if (async_handlers) out << R"STRUT_SERVER(
    void get_async(const strut_string& path,std::function<strut_future<strut_server_response>(strut_server_request)> h) const{get(path,[h=std::move(h)](strut_server_request r){return strut_await(h(std::move(r)));});}
    void post_async(const strut_string& path,std::function<strut_future<strut_server_response>(strut_server_request)> h) const{post(path,[h=std::move(h)](strut_server_request r){return strut_await(h(std::move(r)));});}
)STRUT_SERVER";
    out << R"STRUT_SERVER(
    void serve_static(const strut_string& prefix,const std::unordered_map<strut_string,strut_string>& files,const strut_string& fallback=strut_string()) const{require_stopped("configure static files");s_->static_prefix=prefix.v;s_->static_files=files;s_->static_fallback=fallback.v;}
    void timeouts(std::int32_t read_ms,std::int32_t write_ms,std::int32_t idle_ms,std::int32_t shutdown_ms) const{require_stopped("configure timeouts");if(read_ms<=0||write_ms<=0||idle_ms<=0||shutdown_ms<0)throw strut_checked_error("NetworkError","HTTP timeouts must be positive (shutdown may be zero)");s_->read_timeout_ms=read_ms;s_->write_timeout_ms=write_ms;s_->idle_timeout_ms=idle_ms;s_->shutdown_timeout_ms=shutdown_ms;}
    void limits(std::int64_t body_bytes,std::int64_t header_bytes,std::int32_t header_count,std::int32_t connections) const{require_stopped("configure limits");if(body_bytes<0||header_bytes<1024||header_count<=0||connections<=0)throw strut_checked_error("NetworkError","invalid HTTP server limits");s_->max_body_bytes=static_cast<std::size_t>(body_bytes);s_->max_header_bytes=static_cast<std::size_t>(header_bytes);s_->max_header_count=header_count;s_->max_connections=connections;}
    bool running() const{return s_->running.load();}
    void stop() const{auto s=s_;if(!s->running.load())return;s->stopping.store(true);s->listener.close();std::unique_lock<std::mutex> lock(s->mutex);if(!s->cv.wait_for(lock,std::chrono::milliseconds(s->shutdown_timeout_ms),[&]{return s->active==0;})){for(auto& socket:s->active_sockets)socket.close();}}
    void listen(const strut_string& host,std::int32_t port,std::int32_t max_requests=0) const{
#ifndef _WIN32
        {std::lock_guard<std::mutex> signal_lock(strut_sigpipe_mutex);std::signal(SIGPIPE,SIG_IGN);}
#endif
        auto s=s_;bool expected=false;if(!s->running.compare_exchange_strong(expected,true))throw strut_checked_error("NetworkError","HTTP server is already running");
        {std::lock_guard<std::mutex> lock(s->mutex);if(s->active!=0){s->running.store(false);throw strut_checked_error("NetworkError","HTTP server shutdown is still in progress");}}
        s->stopping.store(false);try{s->listener=tcp_listen(host,port);}catch(...){s->running.store(false);throw;}
        std::vector<std::thread> workers;std::int32_t served=0;
        try{while(!s->stopping.load()&&(max_requests<=0||served<max_requests)){
            strut_tcp_socket socket;try{socket=s->listener.accept();}catch(...){if(s->stopping.load())break;throw;}
            bool saturated=false;{std::lock_guard<std::mutex> lock(s->mutex);saturated=s->active>=s->max_connections;if(!saturated){++s->active;s->active_sockets.push_back(socket);}}
            if(saturated){send_error(socket,503,"Service Unavailable");continue;}
            workers.emplace_back([s,socket]() mutable{serve_one(s,socket);{std::lock_guard<std::mutex> lock(s->mutex);--s->active;}s->cv.notify_all();});++served;
        }}catch(...){s->stopping.store(true);s->listener.close();finish_workers(s,workers);s->running.store(false);throw;}
        s->stopping.store(true);s->listener.close();finish_workers(s,workers);s->running.store(false);s->stopping.store(false);
    }
)STRUT_SERVER";
    if (tls) out << R"STRUT_SERVER(
private:
    class tls_socket{public:tls_socket(strut_tcp_socket socket,SSL* ssl):socket_(std::move(socket)),ssl_(ssl){}tls_socket(const tls_socket&)=delete;tls_socket& operator=(const tls_socket&)=delete;tls_socket(tls_socket&& other) noexcept:socket_(std::move(other.socket_)),ssl_(other.ssl_){other.ssl_=nullptr;}~tls_socket(){close();}strut_socket_handle native_handle() const{return socket_.native_handle();}strut_string read(std::int64_t max_bytes=4096){if(max_bytes<=0)return {};std::string out(static_cast<std::size_t>(max_bytes),'\0');int n=SSL_read(ssl_,out.data(),static_cast<int>(out.size()));if(n<=0)throw strut_checked_error("TlsError","TLS request read failed");out.resize(static_cast<std::size_t>(n));return strut_string(std::move(out));}void write(const strut_string& data){std::size_t offset=0;while(offset<data.v.size()){int n=SSL_write(ssl_,data.v.data()+offset,static_cast<int>(data.v.size()-offset));if(n<=0)throw strut_checked_error("TlsError","TLS response write failed");offset+=static_cast<std::size_t>(n);}}void close(){if(ssl_){SSL_shutdown(ssl_);SSL_free(ssl_);ssl_=nullptr;}socket_.close();}private:strut_tcp_socket socket_;SSL* ssl_=nullptr;};
public:
    void listen_tls(const strut_string& host,std::int32_t port,const strut_string& certificate,const strut_string& private_key,std::int32_t max_requests=0) const{
#ifndef _WIN32
        {std::lock_guard<std::mutex> signal_lock(strut_sigpipe_mutex);std::signal(SIGPIPE,SIG_IGN);}
#endif
        SSL_CTX* raw=SSL_CTX_new(TLS_server_method());if(!raw)throw strut_checked_error("TlsError","unable to initialize the TLS server context");std::shared_ptr<SSL_CTX> context(raw,SSL_CTX_free);SSL_CTX_set_min_proto_version(raw,TLS1_2_VERSION);
        if(SSL_CTX_use_certificate_chain_file(raw,certificate.v.c_str())!=1)throw strut_checked_error("TlsError","unable to load TLS certificate chain from '"+certificate.v+"'");
        if(SSL_CTX_use_PrivateKey_file(raw,private_key.v.c_str(),SSL_FILETYPE_PEM)!=1)throw strut_checked_error("TlsError","unable to load TLS private key from '"+private_key.v+"'");
        if(SSL_CTX_check_private_key(raw)!=1)throw strut_checked_error("TlsError","TLS certificate and private key do not match");
        auto s=s_;bool expected=false;if(!s->running.compare_exchange_strong(expected,true))throw strut_checked_error("NetworkError","HTTP server is already running");{std::lock_guard<std::mutex> lock(s->mutex);if(s->active!=0){s->running.store(false);throw strut_checked_error("NetworkError","HTTP server shutdown is still in progress");}}s->stopping.store(false);try{s->listener=tcp_listen(host,port);}catch(...){s->running.store(false);throw;}
        std::vector<std::thread> workers;std::int32_t served=0;try{while(!s->stopping.load()&&(max_requests<=0||served<max_requests)){strut_tcp_socket socket;try{socket=s->listener.accept();}catch(...){if(s->stopping.load())break;throw;}bool saturated=false;{std::lock_guard<std::mutex> lock(s->mutex);saturated=s->active>=s->max_connections;if(!saturated){++s->active;s->active_sockets.push_back(socket);}}if(saturated){socket.close();continue;}workers.emplace_back([s,socket,context]() mutable{SSL* ssl=SSL_new(context.get());if(ssl){SSL_set_fd(ssl,static_cast<int>(socket.native_handle()));if(SSL_accept(ssl)==1){tls_socket secure(std::move(socket),ssl);serve_one(s,std::move(secure));ssl=nullptr;}if(ssl)SSL_free(ssl);}socket.close();{std::lock_guard<std::mutex> lock(s->mutex);--s->active;}s->cv.notify_all();});++served;}}catch(...){s->stopping.store(true);s->listener.close();finish_workers(s,workers);s->running.store(false);throw;}s->stopping.store(true);s->listener.close();finish_workers(s,workers);s->running.store(false);s->stopping.store(false);
    }
)STRUT_SERVER";
    out << R"STRUT_SERVER(
private:
    struct route{std::string method,path;handler fn;};
    struct state{std::vector<route> routes;std::string static_prefix,static_fallback;std::unordered_map<strut_string,strut_string> static_files;std::atomic<bool> running{false},stopping{false};strut_tcp_listener listener;std::mutex mutex;std::condition_variable cv;std::int32_t active=0;std::vector<strut_tcp_socket> active_sockets;std::int32_t read_timeout_ms=30000,write_timeout_ms=30000,idle_timeout_ms=5000,shutdown_timeout_ms=5000,max_header_count=100,max_connections=1024;std::size_t max_body_bytes=1024*1024,max_header_bytes=64*1024;};
    std::shared_ptr<state> s_;
    void require_stopped(const char* action) const{if(s_->running.load())throw strut_checked_error("NetworkError",std::string("cannot ")+action+" while HTTP server is running");}
    void add_route(const char* method,const strut_string& path,handler h) const{require_stopped("register routes");s_->routes.push_back({method,path.v,std::move(h)});}
    static void finish_workers(const std::shared_ptr<state>& s,std::vector<std::thread>& workers){bool drained;{std::unique_lock<std::mutex> lock(s->mutex);drained=s->cv.wait_for(lock,std::chrono::milliseconds(s->shutdown_timeout_ms),[&]{return s->active==0;});if(!drained)for(auto& socket:s->active_sockets)socket.close();s->active_sockets.clear();}for(auto& worker:workers){if(!worker.joinable())continue;if(drained)worker.join();else worker.detach();}}
    template<class Socket> static void set_socket_timeouts(const Socket& socket,std::int32_t read_ms,std::int32_t write_ms){
#ifdef _WIN32
        DWORD read=static_cast<DWORD>(read_ms),write=static_cast<DWORD>(write_ms);setsockopt(socket.native_handle(),SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&read),sizeof(read));setsockopt(socket.native_handle(),SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&write),sizeof(write));
#else
        timeval read{read_ms/1000,(read_ms%1000)*1000},write{write_ms/1000,(write_ms%1000)*1000};setsockopt(socket.native_handle(),SOL_SOCKET,SO_RCVTIMEO,&read,sizeof(read));setsockopt(socket.native_handle(),SOL_SOCKET,SO_SNDTIMEO,&write,sizeof(write));
#endif
    }
    template<class Socket> static void send_error(Socket& socket,std::int32_t status,const char* message){try{std::string body=message;std::ostringstream out;out<<"HTTP/1.1 "<<status<<' '<<message<<"\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\n\r\n"<<body;socket.write(strut_string(out.str()));socket.close();}catch(...){socket.close();}}
    static strut_string mime(const std::string& p){auto dot=p.rfind('.');auto e=dot==std::string::npos?std::string():p.substr(dot);if(e==".html")return "text/html; charset=utf-8";if(e==".css")return "text/css; charset=utf-8";if(e==".js")return "application/javascript";if(e==".json")return "application/json";if(e==".svg")return "image/svg+xml";if(e==".png")return "image/png";return "application/octet-stream";}
    static std::string etag(const std::string& data){std::uint64_t h=1469598103934665603ull;for(unsigned char c:data){h^=c;h*=1099511628211ull;}std::ostringstream out;out<<'"'<<std::hex<<h<<'"';return out.str();}
    template<class Socket> static void serve_one(const std::shared_ptr<state>& s,Socket socket){
        try{set_socket_timeouts(socket,std::min(s->read_timeout_ms,s->idle_timeout_ms),s->write_timeout_ms);std::string raw;std::size_t header_end=std::string::npos;for(;;){auto chunk=socket.read(4096).v;if(chunk.empty()){send_error(socket,400,"Bad Request");return;}raw+=chunk;if(raw.size()>s->max_header_bytes){send_error(socket,431,"Request Header Fields Too Large");return;}header_end=raw.find("\r\n\r\n");if(header_end!=std::string::npos)break;}
            strut_server_request req;auto line_end=raw.find("\r\n");if(line_end==std::string::npos){send_error(socket,400,"Bad Request");return;}std::istringstream first(raw.substr(0,line_end));std::string target,version,extra;if(!(first>>req.method.v>>target>>version)||(first>>extra)||version.rfind("HTTP/",0)!=0){send_error(socket,400,"Bad Request");return;}auto q=target.find('?');req.path=strut_string(target.substr(0,q));if(q!=std::string::npos)strut_parse_query(target.substr(q+1),req.query);
            std::size_t p=line_end+2,content_len=0;std::int32_t count=0;while(p<header_end){auto e=raw.find("\r\n",p);if(e==std::string::npos||e>header_end){send_error(socket,400,"Bad Request");return;}auto line=raw.substr(p,e-p);auto colon=line.find(':');if(colon==std::string::npos||++count>s->max_header_count){send_error(socket,count>s->max_header_count?431:400,count>s->max_header_count?"Request Header Fields Too Large":"Bad Request");return;}auto key=strut_trim_ascii(line.substr(0,colon)),value=strut_trim_ascii(line.substr(colon+1));req.headers[strut_string(key)]=strut_string(value);if(key=="Content-Length"){if(value.empty()||!std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isdigit(c); })){send_error(socket,400,"Bad Request");return;}content_len=static_cast<std::size_t>(std::strtoull(value.c_str(),nullptr,10));if(content_len>s->max_body_bytes){send_error(socket,413,"Payload Too Large");return;}}p=e+2;}
            req.body=strut_string(raw.substr(header_end+4));if(req.body.v.size()>content_len)req.body.v.resize(content_len);while(req.body.v.size()<content_len){auto more=socket.read(static_cast<std::int64_t>(content_len-req.body.v.size())).v;if(more.empty()){send_error(socket,400,"Bad Request");return;}req.body.v+=more;}
            strut_server_response response;bool found=false,method_mismatch=false;for(auto& route:s->routes){req.params.clear();if(!strut_route_match(route.path,req.path.v,req.params))continue;if(route.method!=req.method.v){method_mismatch=true;continue;}try{response=route.fn(req);}catch(const std::exception&){send_error(socket,500,"Internal Server Error");return;}catch(...){send_error(socket,500,"Internal Server Error");return;}found=true;break;}
            if(!found&&!s->static_files.empty()&&req.method.v=="GET"){std::string key=req.path.v;if(!s->static_prefix.empty()&&key.rfind(s->static_prefix,0)==0)key=key.substr(s->static_prefix.size());while(!key.empty()&&key.front()=='/')key.erase(key.begin());if(key.empty())key="index.html";if(key.find("..")!=std::string::npos){response.status=400;response.body="Bad Request";found=true;}else{auto it=s->static_files.find(strut_string(key));if(it==s->static_files.end()&&!s->static_fallback.empty())it=s->static_files.find(strut_string(s->static_fallback));if(it!=s->static_files.end()){response.status=200;response.body=it->second;response.content_type=mime(key);response.headers[strut_string("ETag")]=strut_string(etag(response.body.v));response.headers[strut_string("Cache-Control")]=strut_string("public, max-age=0, must-revalidate");found=true;}}}
            if(!found){response.status=method_mismatch?405:404;response.body=method_mismatch?"Method Not Allowed":"Not Found";}std::ostringstream out;out<<"HTTP/1.1 "<<response.status<<' '<<(response.status==200?"OK":response.status==404?"Not Found":response.status==405?"Method Not Allowed":"Response")<<"\r\nContent-Type: "<<response.content_type.v<<"\r\nContent-Length: "<<response.body.v.size()<<"\r\nConnection: close\r\n";for(auto& header:response.headers)out<<header.first.v<<": "<<header.second.v<<"\r\n";out<<"\r\n"<<response.body.v;socket.write(strut_string(out.str()));socket.close();
        }catch(...){socket.close();}
    }
};
)STRUT_SERVER";
}

} // namespace strut::generated_runtime
