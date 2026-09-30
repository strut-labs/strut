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

void emit_encoding(std::ostream& out) {
    out << R"STRUT_ENCODING(
inline int strut_base64_value(unsigned char c,bool url) noexcept{
    if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;
    if(url){if(c=='-')return 62;if(c=='_')return 63;}else{if(c=='+')return 62;if(c=='/')return 63;}return -1;
}
inline strut_string strut_base64_encode(const strut_bytes& value,bool url){
    static constexpr char standard[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static constexpr char safe[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    const char* alphabet=url?safe:standard;std::string result;result.reserve(((value.native_size()+2)/3)*4);
    std::size_t i=0;while(i+3<=value.native_size()){const unsigned a=value.data()[i++],b=value.data()[i++],c=value.data()[i++];result.push_back(alphabet[a>>2]);result.push_back(alphabet[((a&3)<<4)|(b>>4)]);result.push_back(alphabet[((b&15)<<2)|(c>>6)]);result.push_back(alphabet[c&63]);}
    const auto remaining=value.native_size()-i;if(remaining==1){const unsigned a=value.data()[i];result.push_back(alphabet[a>>2]);result.push_back(alphabet[(a&3)<<4]);if(!url)result+="==";}else if(remaining==2){const unsigned a=value.data()[i],b=value.data()[i+1];result.push_back(alphabet[a>>2]);result.push_back(alphabet[((a&3)<<4)|(b>>4)]);result.push_back(alphabet[(b&15)<<2]);if(!url)result.push_back('=');}
    return strut_string(std::move(result));
}
[[noreturn]] inline void strut_base64_error(bool url){throw strut_checked_error("EncodingError",url?"invalid canonical Base64url":"invalid canonical Base64");}
inline strut_bytes strut_base64_decode(const strut_string& text,bool url){
    const auto& input=text.v;strut_bytes result;if(url){if(input.size()%4==1)strut_base64_error(true);}else if(input.size()%4!=0)strut_base64_error(false);
    const auto append=[&](unsigned value){const auto old=result.native_size();result.resize_native(old+1);result.data()[old]=static_cast<std::uint8_t>(value);};
    std::size_t i=0;while(i+4<=input.size()){
        const bool final=i+4==input.size();const int a=strut_base64_value(static_cast<unsigned char>(input[i]),url),b=strut_base64_value(static_cast<unsigned char>(input[i+1]),url);if(a<0||b<0)strut_base64_error(url);
        const unsigned char cch=static_cast<unsigned char>(input[i+2]),dch=static_cast<unsigned char>(input[i+3]);
        if(!url&&cch=='='){if(!final||dch!='='||(b&15)!=0)strut_base64_error(false);append((static_cast<unsigned>(a)<<2)|(static_cast<unsigned>(b)>>4));i+=4;continue;}
        const int c=strut_base64_value(cch,url);if(c<0)strut_base64_error(url);append((static_cast<unsigned>(a)<<2)|(static_cast<unsigned>(b)>>4));
        if(!url&&dch=='='){if(!final||(c&3)!=0)strut_base64_error(false);append(((static_cast<unsigned>(b)&15)<<4)|(static_cast<unsigned>(c)>>2));i+=4;continue;}
        const int d=strut_base64_value(dch,url);if(d<0)strut_base64_error(url);append(((static_cast<unsigned>(b)&15)<<4)|(static_cast<unsigned>(c)>>2));append(((static_cast<unsigned>(c)&3)<<6)|static_cast<unsigned>(d));i+=4;
    }
    if(url){const auto remaining=input.size()-i;if(remaining==2){const int a=strut_base64_value(static_cast<unsigned char>(input[i]),true),b=strut_base64_value(static_cast<unsigned char>(input[i+1]),true);if(a<0||b<0||(b&15)!=0)strut_base64_error(true);append((static_cast<unsigned>(a)<<2)|(static_cast<unsigned>(b)>>4));}else if(remaining==3){const int a=strut_base64_value(static_cast<unsigned char>(input[i]),true),b=strut_base64_value(static_cast<unsigned char>(input[i+1]),true),c=strut_base64_value(static_cast<unsigned char>(input[i+2]),true);if(a<0||b<0||c<0||(c&3)!=0)strut_base64_error(true);append((static_cast<unsigned>(a)<<2)|(static_cast<unsigned>(b)>>4));append(((static_cast<unsigned>(b)&15)<<4)|(static_cast<unsigned>(c)>>2));}else if(remaining!=0)strut_base64_error(true);}
    return result;
}
inline strut_string base64_encode(const strut_bytes& value){return strut_base64_encode(value,false);}
inline strut_bytes base64_decode(const strut_string& value){return strut_base64_decode(value,false);}
inline strut_string base64url_encode(const strut_bytes& value){return strut_base64_encode(value,true);}
inline strut_bytes base64url_decode(const strut_string& value){return strut_base64_decode(value,true);}
)STRUT_ENCODING";
}

void emit_crypto(std::ostream& out) {
    out << R"STRUT_CRYPTO(
#include <climits>
#include <algorithm>
#include <memory>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/rand.h>
[[noreturn]] inline void strut_crypto_error(const char* operation){ERR_clear_error();throw strut_checked_error("CryptoError",std::string(operation)+" failed");}
inline strut_bytes secure_random_bytes(std::int64_t count){
    if(count<0||static_cast<std::uint64_t>(count)>SIZE_MAX)throw strut_checked_error("CryptoError","secure random byte count is out of range");strut_bytes result;try{result.resize_native(static_cast<std::size_t>(count));}catch(const std::exception&){throw strut_checked_error("CryptoError","secure random byte allocation failed");}
    std::size_t offset=0;while(offset<result.native_size()){const auto chunk=static_cast<int>(std::min<std::size_t>(result.native_size()-offset,static_cast<std::size_t>(INT_MAX)));if(RAND_bytes(result.data()+offset,chunk)!=1)strut_crypto_error("secure random generation");offset+=static_cast<std::size_t>(chunk);}return result;
}
inline strut_bytes sha256(const strut_bytes& value){
    strut_bytes result(32);std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(),EVP_MD_CTX_free);if(!context)strut_crypto_error("SHA-256 context allocation");
    if(EVP_DigestInit_ex(context.get(),EVP_sha256(),nullptr)!=1||(!value.empty()&&EVP_DigestUpdate(context.get(),value.data(),value.native_size())!=1))strut_crypto_error("SHA-256");unsigned int length=0;if(EVP_DigestFinal_ex(context.get(),result.data(),&length)!=1||length!=32)strut_crypto_error("SHA-256");return result;
}
inline strut_bytes hmac_sha256(const strut_bytes& key,const strut_bytes& value){
    std::unique_ptr<EVP_MAC,decltype(&EVP_MAC_free)> algorithm(EVP_MAC_fetch(nullptr,"HMAC",nullptr),EVP_MAC_free);if(!algorithm)strut_crypto_error("HMAC-SHA-256 algorithm selection");std::unique_ptr<EVP_MAC_CTX,decltype(&EVP_MAC_CTX_free)> context(EVP_MAC_CTX_new(algorithm.get()),EVP_MAC_CTX_free);if(!context)strut_crypto_error("HMAC-SHA-256 context allocation");
    char digest[]="SHA256";OSSL_PARAM parameters[]={OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,digest,0),OSSL_PARAM_construct_end()};const unsigned char empty=0;const auto* key_data=key.empty()?&empty:key.data();if(EVP_MAC_init(context.get(),key_data,key.native_size(),parameters)!=1||(!value.empty()&&EVP_MAC_update(context.get(),value.data(),value.native_size())!=1))strut_crypto_error("HMAC-SHA-256");strut_bytes result(32);std::size_t length=0;if(EVP_MAC_final(context.get(),result.data(),&length,result.native_size())!=1||length!=32)strut_crypto_error("HMAC-SHA-256");return result;
}
inline bool constant_time_equal(const strut_bytes& left,const strut_bytes& right) noexcept{return left.native_size()==right.native_size()&&(left.empty()||CRYPTO_memcmp(left.data(),right.data(),left.native_size())==0);}
)STRUT_CRYPTO";
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
        std::unordered_map<std::uint64_t,std::shared_ptr<strut_cancellation_callback>> callbacks;{std::lock_guard<std::mutex> lock(state_->mutex);callbacks.swap(state_->callbacks);}state_->cv.notify_all();
        for(auto& item:callbacks){auto& callback=item.second;std::function<void()> call;{std::lock_guard<std::mutex> lock(callback->mutex);if(callback->active){call=std::move(callback->function);callback->running=true;callback->running_thread=std::this_thread::get_id();}}if(call){try{call();}catch(...){ }}{std::lock_guard<std::mutex> lock(callback->mutex);callback->running=false;callback->running_thread={};callback->active=false;}callback->cv.notify_all();}
    }
private:std::shared_ptr<strut_cancellation_state> state_;
};
)STRUT_CANCEL";
}

void emit_pty(std::ostream& out) {
    out << R"STRUT_PTY(
#define STRUT_PTY_RUNTIME 1
#ifdef _WIN32
class strut_pty {
public:
    strut_bytes read_bytes(std::int64_t) const{throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
    void write_bytes(const strut_bytes&) const{throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
    bool eof() const noexcept{return true;}
    void resize(std::int32_t,std::int32_t) const{throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
    void interrupt() const{throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
    void terminate() const{throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
    void kill() const{throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
    void hangup() const{throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
    std::int32_t wait() const{throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
    bool running() const noexcept{return false;}
    std::int32_t exit_code() const noexcept{return -1;}
    void close() const noexcept{}
};
inline strut_pty strut_pty_spawn(const strut_string&,const std::vector<strut_string>&){throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
inline strut_pty strut_pty_spawn(const strut_string&,const std::vector<strut_string>&,const json::Document&){throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
inline strut_pty strut_pty_spawn(const strut_string&,const std::vector<strut_string>&,const strut_process_cancellation_token&){throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
inline strut_pty strut_pty_spawn(const strut_string&,const std::vector<strut_string>&,const json::Document&,const strut_process_cancellation_token&){throw strut_checked_error("PtyError","PTY unsupported on Windows until P9 ConPTY");}
#else
#include <cstdlib>
#include <limits>
#include <sys/ioctl.h>
#include <termios.h>
#ifdef __APPLE__
#include <crt_externs.h>
#include <mach-o/dyld.h>
#endif
#if defined(__GLIBC__)
#if defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2,34)
#ifndef POSIX_SPAWN_SETSID
#define POSIX_SPAWN_SETSID 0x80
#endif
#define STRUT_PTY_POSIX_SPAWN_SETSID POSIX_SPAWN_SETSID
#endif
#endif
#elif defined(__APPLE__) && defined(POSIX_SPAWN_SETSID_NP)
#define STRUT_PTY_POSIX_SPAWN_SETSID POSIX_SPAWN_SETSID_NP
#elif defined(__APPLE__) && defined(POSIX_SPAWN_SETSID)
#define STRUT_PTY_POSIX_SPAWN_SETSID POSIX_SPAWN_SETSID
#endif

struct strut_pty_options {
    strut_exec_options launch;
    unsigned short rows=24;
    unsigned short columns=80;
};
inline strut_pty_options strut_parse_pty_options(const json::Document& value){
    strut_pty_options out;if(value.type==json::Type::Null)return out;if(value.type!=json::Type::Object)throw strut_checked_error("PtyError","PTY options must be a JSON object");
    for(const auto& item:value.object)if(item.first!="cwd"&&item.first!="env"&&item.first!="rows"&&item.first!="columns")throw strut_checked_error("PtyError","unknown PTY option: "+item.first);
    if(value.has("cwd")){if(value["cwd"].type!=json::Type::String)throw strut_checked_error("PtyError","PTY cwd option must be a string");out.launch.cwd=value["cwd"].string;}
    if(value.has("env")){const auto& environment=value["env"];if(environment.type!=json::Type::Object)throw strut_checked_error("PtyError","PTY env option must be an object");for(const auto& item:environment.object){if(item.second.type!=json::Type::String)throw strut_checked_error("PtyError","PTY environment values must be strings");out.launch.env.emplace_back(item.first,item.second.string);}}
    auto dimension=[&](const char* name,unsigned short& target){if(!value.has(name))return;const auto& item=value[name];if(item.type!=json::Type::Number||item.num<1||item.num>65535||item.num!=static_cast<double>(static_cast<unsigned short>(item.num)))throw strut_checked_error("PtyError",std::string("PTY ")+name+" must be an integer from 1 through 65535");target=static_cast<unsigned short>(item.num);};
    dimension("rows",out.rows);dimension("columns",out.columns);return out;
}
inline std::string strut_pty_cwd(){std::vector<char> buffer(256);for(;;){if(::getcwd(buffer.data(),buffer.size()))return buffer.data();if(errno!=ERANGE)throw strut_checked_error("PtyError",std::string("getcwd failed: ")+std::strerror(errno));buffer.resize(buffer.size()*2);}}
inline std::string strut_pty_join_path(const std::string& left,const std::string& right){if(left.empty())return right;if(right.empty())return left;return left+(left.back()=='/'?"":"/")+right;}
inline std::string strut_pty_child_directory(const strut_exec_options& options){if(options.cwd.v.empty())return strut_pty_cwd();if(options.cwd.v.front()=='/')return options.cwd.v;return strut_pty_join_path(strut_pty_cwd(),options.cwd.v);}
inline std::string strut_pty_resolve_executable(const strut_string& program,const strut_posix_spawn_data& data,const strut_exec_options& options){
    if(program.v.empty())throw strut_checked_error("PtyError","PTY program is empty");strut_reject_nul(program.v,"program");const auto cwd=strut_pty_child_directory(options);
    if(program.v.find('/')!=std::string::npos){const auto candidate=program.v.front()=='/'?program.v:strut_pty_join_path(cwd,program.v);if(::access(candidate.c_str(),X_OK)==0)return candidate;throw strut_checked_error("PtyError",std::string("PTY executable is not accessible: ")+std::strerror(errno));}
    std::string path="/usr/bin:/bin";for(const auto& entry:data.environment)if(entry.rfind("PATH=",0)==0){path=entry.substr(5);break;}std::size_t begin=0;for(;;){const auto end=path.find(':',begin);auto directory=path.substr(begin,end==std::string::npos?std::string::npos:end-begin);if(directory.empty())directory=cwd;else if(directory.front()!='/')directory=strut_pty_join_path(cwd,directory);const auto candidate=strut_pty_join_path(directory,program.v);if(::access(candidate.c_str(),X_OK)==0)return candidate;if(end==std::string::npos)break;begin=end+1;}throw strut_checked_error("PtyError","PTY executable not found in PATH");
}
)STRUT_PTY"; out << R"STRUT_PTY(
struct strut_pty_operation {
    std::atomic<bool> closed{false};int wake[2]{-1,-1};
    strut_pty_operation(){
#ifdef __linux__
        if(pipe2(wake,O_NONBLOCK|O_CLOEXEC)!=0)throw strut_checked_error("PtyError","PTY cancellation wakeup setup failed");
#else
        std::lock_guard<std::mutex> spawn_lock(strut_process_spawn_mutex);if(pipe(wake)!=0)throw strut_checked_error("PtyError","PTY cancellation wakeup setup failed");for(int descriptor:wake){const int status=fcntl(descriptor,F_GETFL,0),flags=fcntl(descriptor,F_GETFD,0);if(status<0||flags<0||fcntl(descriptor,F_SETFL,status|O_NONBLOCK)<0||fcntl(descriptor,F_SETFD,flags|FD_CLOEXEC)<0){const int failure=errno;strut_close_pipe(wake);errno=failure;throw strut_checked_error("PtyError","PTY cancellation wakeup setup failed");}}
#endif
    }
    void signal() noexcept{const char byte=0;ssize_t result;do{result=::write(wake[1],&byte,1);}while(result<0&&errno==EINTR);}
    ~strut_pty_operation(){strut_close_pipe(wake);}
};
struct strut_pty_state {
    mutable std::mutex io_mutex,lifecycle_mutex,native_mutex,read_mutex,write_mutex;std::condition_variable lifecycle_cv;int master=-1;pid_t pid=-1;bool closed=true,eof_seen=false,running_value=false,close_requested=false;int lifecycle_owner=0,lifecycle_failure=0;std::int32_t code=-1;strut_process_cancellation_token token;std::weak_ptr<strut_pty_operation> reader,writer;
    void close_native_if_idle() noexcept{if(closed&&reader.expired()&&writer.expired())strut_close_fd(master);}
    void close_io() noexcept{std::shared_ptr<strut_pty_operation> read,write;{std::lock_guard<std::mutex> lock(io_mutex);if(closed)return;closed=true;read=reader.lock();write=writer.lock();close_native_if_idle();}if(read){read->closed.store(true,std::memory_order_release);read->signal();}if(write){write->closed.store(true,std::memory_order_release);write->signal();}}
    static int observe(pid_t owned) noexcept{siginfo_t information{};for(;;){if(waitid(P_PID,static_cast<id_t>(owned),&information,WEXITED|WNOWAIT|WNOHANG)==0)return information.si_pid==0?0:1;if(errno!=EINTR)return -errno;}}
    void finish(pid_t owned,int status,int failure) noexcept{{std::lock_guard<std::mutex> lock(lifecycle_mutex);if(pid==owned){if(failure)lifecycle_failure=failure;else code=WIFEXITED(status)?WEXITSTATUS(status):(WIFSIGNALED(status)?128+WTERMSIG(status):-1);pid=-1;running_value=false;}lifecycle_owner=0;}lifecycle_cv.notify_all();}
    bool run_owner(bool blocking,bool cancellable=false) noexcept{pid_t owned=-1;{std::lock_guard<std::mutex> lock(lifecycle_mutex);owned=pid;}bool term_sent=false,kill_sent=false;int attempts=0;for(;;){int observation=0,status=0,failure=0;{std::lock_guard<std::mutex> native(native_mutex);observation=observe(owned);if(observation>0){(void)::kill(-owned,SIGKILL);for(;;){const pid_t result=waitpid(owned,&status,0);if(result==owned)break;if(result<0&&errno==EINTR)continue;failure=result<0?errno:ECHILD;break;}}}if(observation<0){finish(owned,0,-observation);return true;}if(observation>0){finish(owned,status,failure);return true;}bool requested=false;{std::unique_lock<std::mutex> lock(lifecycle_mutex);requested=close_requested;if(cancellable&&token.cancelled()&&!requested){lifecycle_owner=0;lock.unlock();lifecycle_cv.notify_all();return false;}if(!requested&&!blocking){lifecycle_owner=0;lock.unlock();lifecycle_cv.notify_all();return true;}}if(requested){if(!term_sent){(void)::kill(-owned,SIGTERM);term_sent=true;attempts=0;}else if(!kill_sent&&++attempts>=20){(void)::kill(-owned,SIGKILL);kill_sent=true;attempts=0;}else if(kill_sent&&++attempts>=20){try{std::thread([owned](){int status=0;while(waitpid(owned,&status,0)<0&&errno==EINTR){}}).detach();}catch(...){int ignored=0;while(waitpid(owned,&ignored,0)<0&&errno==EINTR){}}finish(owned,0,ETIMEDOUT);return true;}}std::unique_lock<std::mutex> lock(lifecycle_mutex);lifecycle_cv.wait_for(lock,std::chrono::milliseconds(10),[&]{return (close_requested&&!requested)||(cancellable&&token.cancelled()&&!close_requested);});}}
    void cleanup() noexcept{bool owner=false,running=false;{std::lock_guard<std::mutex> lock(lifecycle_mutex);close_requested=true;running=running_value;if(running&&lifecycle_owner==0){lifecycle_owner=2;owner=true;}}lifecycle_cv.notify_all();close_io();if(!running)return;if(!owner){std::unique_lock<std::mutex> lock(lifecycle_mutex);(void)lifecycle_cv.wait_for(lock,std::chrono::seconds(1),[&]{return !running_value;});return;}(void)run_owner(true);}
    ~strut_pty_state(){cleanup();}
};
class strut_pty_guard {
public:
    strut_pty_guard(std::shared_ptr<strut_pty_state> state,bool reading):state_(std::move(state)),reading_(reading),direction_(reading_?state_->read_mutex:state_->write_mutex,std::defer_lock),operation_(std::make_shared<strut_pty_operation>()){
        if(!direction_.try_lock())throw strut_checked_error("PtyError",reading_?"PTY already has an active reader":"PTY already has an active writer");std::lock_guard<std::mutex> lock(state_->io_mutex);if(state_->closed)throw strut_checked_error("PtyError","PTY is closed");(reading_?state_->reader:state_->writer)=operation_;descriptor=state_->master;token=state_->token;
    }
    ~strut_pty_guard(){std::lock_guard<std::mutex> lock(state_->io_mutex);auto& active=reading_?state_->reader:state_->writer;if(auto operation=active.lock();operation==operation_)active.reset();state_->close_native_if_idle();}
    void interrupted() const{if(token.cancelled())throw strut_checked_error("PtyError","PTY I/O cancelled",strut_process_cancelled_code);if(operation_->closed.load(std::memory_order_acquire))throw strut_checked_error("PtyError","PTY is closed");}
    short poll(short events){for(;;){interrupted();pollfd descriptors[2]={{descriptor,events,0},{operation_->wake[0],POLLIN,0}};const int result=::poll(descriptors,2,-1);if(result<0&&errno==EINTR)continue;if(result<0)throw strut_checked_error("PtyError",std::string("PTY poll failed: ")+std::strerror(errno));if(descriptors[0].revents)return descriptors[0].revents;interrupted();}}
    std::shared_ptr<strut_pty_state> state_;bool reading_;std::unique_lock<std::mutex> direction_;std::shared_ptr<strut_pty_operation> operation_;strut_process_cancellation_token token;int descriptor=-1;
};
inline void strut_pty_mark_eof(const std::shared_ptr<strut_pty_state>& state){
#ifdef __APPLE__
    state->close_io();
#endif
    std::lock_guard<std::mutex> lock(state->io_mutex);state->eof_seen=true;
}
)STRUT_PTY"; out << R"STRUT_PTY(
class strut_pty {
public:
    strut_pty():state_(std::make_shared<strut_pty_state>()){}
    explicit strut_pty(std::shared_ptr<strut_pty_state> state):state_(std::move(state)){}
    strut_bytes read_bytes(std::int64_t maximum) const{if(maximum<0)throw strut_checked_error("PtyError","negative PTY read size");{std::lock_guard<std::mutex> lock(state_->io_mutex);if(state_->eof_seen)return {};if(state_->closed)throw strut_checked_error("PtyError","PTY is closed");if(maximum==0)return {};}strut_pty_guard guard(state_,true);[[maybe_unused]] auto subscription=guard.token.subscribe([operation=guard.operation_]{operation->signal();});const auto size=static_cast<std::size_t>(std::min<std::int64_t>(maximum,65536));strut_bytes output(static_cast<std::int64_t>(size));for(;;){const short events=guard.poll(POLLIN);const ssize_t count=::read(guard.descriptor,output.data(),size);if(count>0){output.resize_native(static_cast<std::size_t>(count));return output;}if(count==0||(count<0&&errno==EIO)){guard.interrupted();strut_pty_mark_eof(state_);output.resize_native(0);return output;}if(count<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)){if(events&(POLLHUP|POLLERR)){guard.interrupted();strut_pty_mark_eof(state_);output.resize_native(0);return output;}continue;}guard.interrupted();throw strut_checked_error("PtyError",std::string("PTY read failed: ")+std::strerror(errno));}}
    void write_bytes(const strut_bytes& value) const{strut_pty_guard guard(state_,false);[[maybe_unused]] auto subscription=guard.token.subscribe([operation=guard.operation_]{operation->signal();});guard.interrupted();std::size_t offset=0;while(offset<value.native_size()){const short events=guard.poll(POLLOUT);if(events&(POLLHUP|POLLERR))throw strut_checked_error("PtyError","PTY slave is closed");int failure=0;const ssize_t count=strut_process_write_once(guard.descriptor,reinterpret_cast<const char*>(value.data())+offset,value.native_size()-offset,failure);if(count<0&&(failure==EAGAIN||failure==EWOULDBLOCK||failure==EINTR))continue;if(count<=0){guard.interrupted();throw strut_checked_error("PtyError",std::string("PTY write failed: ")+std::strerror(failure));}offset+=static_cast<std::size_t>(count);}}
    bool eof() const{std::lock_guard<std::mutex> lock(state_->io_mutex);return state_->eof_seen;}
    void resize(std::int32_t rows,std::int32_t columns) const{if(rows<1||rows>65535||columns<1||columns>65535)throw strut_checked_error("PtyError","PTY dimensions must be integers from 1 through 65535");std::lock_guard<std::mutex> lock(state_->io_mutex);if(state_->closed)throw strut_checked_error("PtyError","PTY is closed");winsize size{};size.ws_row=static_cast<unsigned short>(rows);size.ws_col=static_cast<unsigned short>(columns);if(ioctl(state_->master,TIOCSWINSZ,&size)!=0)throw strut_checked_error("PtyError",std::string("PTY resize failed: ")+std::strerror(errno));}
    void interrupt() const{signal_owned(SIGINT);}
    void terminate() const{signal_owned(SIGTERM);}
    void kill() const{signal_owned(SIGKILL);}
    void hangup() const{signal_owned(SIGHUP);}
    std::int32_t wait() const{auto state=state_;[[maybe_unused]] auto subscription=state->token.subscribe([state]{state->lifecycle_cv.notify_all();});for(;;){bool owner=false;{std::unique_lock<std::mutex> lock(state->lifecycle_mutex);if(!state->running_value){if(state->lifecycle_failure)throw strut_checked_error("PtyError",std::string("PTY wait failed: ")+std::strerror(state->lifecycle_failure));return state->code;}if(!state->close_requested&&state->token.cancelled())throw strut_checked_error("PtyError","PTY wait cancelled",strut_process_cancelled_code);if(state->lifecycle_owner==0){state->lifecycle_owner=1;owner=true;}else state->lifecycle_cv.wait(lock,[&]{return !state->running_value||state->lifecycle_owner==0||(!state->close_requested&&state->token.cancelled());});}if(owner&&!state->run_owner(true,true))throw strut_checked_error("PtyError","PTY wait cancelled",strut_process_cancelled_code);}}
    bool running() const{auto state=state_;bool owner=false;{std::lock_guard<std::mutex> lock(state->lifecycle_mutex);if(!state->running_value)return false;if(state->lifecycle_owner==0){state->lifecycle_owner=1;owner=true;}}if(owner)(void)state->run_owner(false);std::lock_guard<std::mutex> lock(state->lifecycle_mutex);return state->running_value;}
    std::int32_t exit_code() const{auto state=state_;bool owner=false;{std::lock_guard<std::mutex> lock(state->lifecycle_mutex);if(state->running_value&&state->lifecycle_owner==0){state->lifecycle_owner=1;owner=true;}}if(owner)(void)state->run_owner(false);std::lock_guard<std::mutex> lock(state->lifecycle_mutex);return state->running_value||state->lifecycle_failure?-1:state->code;}
    void close() const{auto state=state_;state->cleanup();}
private:
    void signal_owned(int value) const{auto state_=this->state_;std::unique_lock<std::mutex> native(state_->native_mutex);std::unique_lock<std::mutex> lifecycle(state_->lifecycle_mutex);if(!state_->running_value)return;const pid_t leader=state_->pid;const int observation=strut_pty_state::observe(leader);if(observation>0)return;if(observation<0){if(-observation==ECHILD||-observation==ESRCH)return;throw strut_checked_error("PtyError",std::string("PTY signal ownership check failed: ")+std::strerror(-observation));}errno=0;const pid_t session=getsid(leader);if(session!=leader){if(session<0&&errno==ESRCH)return;throw strut_checked_error("PtyError","PTY signal target no longer owns its session");}pid_t target=leader;{std::lock_guard<std::mutex> io(state_->io_mutex);if(!state_->closed&&tcgetsid(state_->master)==leader){const pid_t foreground=tcgetpgrp(state_->master);if(foreground>0)target=foreground;}}if(target==leader){errno=0;if(getpgid(leader)!=leader){if(errno==ESRCH)return;throw strut_checked_error("PtyError","PTY leader group identity changed");}}if(::kill(-target,value)!=0&&errno!=ESRCH)throw strut_checked_error("PtyError",std::string("PTY signal failed: ")+std::strerror(errno));}
    std::shared_ptr<strut_pty_state> state_;
};
inline int strut_pty_parent_fd(int descriptor){if(descriptor<0)return descriptor;if(descriptor>STDERR_FILENO)return descriptor;const int replacement=fcntl(descriptor,F_DUPFD_CLOEXEC,STDERR_FILENO+1);const int failure=errno;::close(descriptor);if(replacement<0){errno=failure;throw strut_checked_error("PtyError","PTY descriptor setup failed");}return replacement;}
inline bool strut_pty_child_main(int argc,char** argv){
#ifdef __APPLE__
    if(argc<6||std::strcmp(argv[1],"--strut-internal-pty-launcher-v1")!=0)return false;
    auto descriptor=[](const char* value){char* end=nullptr;errno=0;const long parsed=std::strtol(value,&end,10);return errno||!end||*end||parsed<=STDERR_FILENO||parsed>std::numeric_limits<int>::max()?-1:static_cast<int>(parsed);};
    const int slave=descriptor(argv[2]),status=descriptor(argv[3]);if(slave<0||status<0||fcntl(slave,F_GETFD)<0||fcntl(status,F_GETFD)<0||!isatty(slave))return false;
    auto fail=[&](int error) noexcept {const int value=error?error:EIO;ssize_t count;do{count=::write(status,&value,sizeof(value));}while(count<0&&errno==EINTR);_exit(126);};
    if(geteuid()!=getuid()||getegid()!=getgid())fail(EPERM);if(getsid(0)!=getpid()||getpgrp()!=getpid())fail(EPERM);const int status_flags=fcntl(status,F_GETFD,0);if(status_flags<0||fcntl(status,F_SETFD,status_flags|FD_CLOEXEC)<0)fail(errno);if(ioctl(slave,TIOCSCTTY,0)!=0||tcsetpgrp(slave,getpgrp())!=0)fail(errno);for(int target=STDIN_FILENO;target<=STDERR_FILENO;++target)if(dup2(slave,target)<0)fail(errno);::close(slave);execve(argv[4],argv+5,environ);fail(errno);return true;
#else
    (void)argc;(void)argv;return false;
#endif
}
#ifdef __APPLE__
__attribute__((constructor(101))) static void strut_pty_early_child_main(){int* argc=_NSGetArgc();char*** argv=_NSGetArgv();if(argc&&argv&&*argv)(void)strut_pty_child_main(*argc,*argv);}
#endif
inline std::string strut_pty_self_executable(){
#ifdef __APPLE__
    std::vector<char> path(1024);std::uint32_t size=static_cast<std::uint32_t>(path.size());if(_NSGetExecutablePath(path.data(),&size)!=0){path.resize(size);if(_NSGetExecutablePath(path.data(),&size)!=0)throw strut_checked_error("PtyError","unable to resolve PTY launcher executable");}return path.data();
#else
    return {};
#endif
}
)STRUT_PTY"; out << R"STRUT_PTY(
inline strut_pty strut_pty_spawn_impl(const strut_string& program,const std::vector<strut_string>& arguments,const strut_pty_options& options,const strut_process_cancellation_token& token){
#ifndef STRUT_PTY_POSIX_SPAWN_SETSID
    (void)program;(void)arguments;(void)options;(void)token;throw strut_checked_error("PtyError","PTY unsupported: P8 requires macOS or glibc 2.34 or newer");
#else
    strut_posix_spawn_data data;std::string executable;try{data=strut_posix_spawn_arguments(program,arguments,options.launch);if(!options.launch.cwd.v.empty())strut_reject_nul(options.launch.cwd.v,"cwd");}catch(const strut_checked_error& error){throw strut_checked_error("PtyError",error.message,error.code);}
    std::unique_lock<std::mutex> spawn_lock(strut_process_spawn_mutex);try{executable=strut_pty_resolve_executable(program,data,options.launch);}catch(const strut_checked_error& error){throw strut_checked_error("PtyError",error.message,error.code);}int master=-1,slave=-1;pid_t pid=-1;posix_spawn_file_actions_t actions;posix_spawnattr_t attributes;bool actions_ready=false,attributes_ready=false;
#ifdef __APPLE__
    int launch_status[2]={-1,-1};
#endif
    try{
        master=strut_pty_parent_fd(posix_openpt(O_RDWR|O_NOCTTY|O_CLOEXEC));if(master<0)throw strut_checked_error("PtyError",std::string("posix_openpt failed: ")+std::strerror(errno));if(grantpt(master)!=0||unlockpt(master)!=0)throw strut_checked_error("PtyError",std::string("PTY slave setup failed: ")+std::strerror(errno));std::string slave_name;
#ifdef __APPLE__
        char* name=ptsname(master);if(!name)throw strut_checked_error("PtyError",std::string("ptsname failed: ")+std::strerror(errno));slave_name=name;
#else
        std::vector<char> name(256);int name_error=ptsname_r(master,name.data(),name.size());if(name_error)throw strut_checked_error("PtyError",std::string("ptsname_r failed: ")+std::strerror(name_error));slave_name=name.data();
#endif
        slave=strut_pty_parent_fd(::open(slave_name.c_str(),O_RDWR|O_NOCTTY|O_CLOEXEC));if(slave<0)throw strut_checked_error("PtyError",std::string("PTY slave open failed: ")+std::strerror(errno));winsize size{};size.ws_row=options.rows;size.ws_col=options.columns;if(ioctl(slave,TIOCSWINSZ,&size)!=0)throw strut_checked_error("PtyError",std::string("PTY window setup failed: ")+std::strerror(errno));
#ifndef __APPLE__
        strut_close_fd(slave);
#endif
        int error=posix_spawn_file_actions_init(&actions);if(error)throw strut_checked_error("PtyError",std::string("PTY spawn actions failed: ")+std::strerror(error));actions_ready=true;error=posix_spawnattr_init(&attributes);if(error)throw strut_checked_error("PtyError",std::string("PTY spawn attributes failed: ")+std::strerror(error));attributes_ready=true;
#ifdef __APPLE__
        if(!strut_make_process_pipe(launch_status))throw strut_checked_error("PtyError",std::string("PTY launcher status pipe failed: ")+std::strerror(errno));error=posix_spawn_file_actions_addinherit_np(&actions,slave);if(!error)error=posix_spawn_file_actions_addinherit_np(&actions,launch_status[1]);
#else
        error=posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,slave_name.c_str(),O_RDWR,0);if(!error)error=posix_spawn_file_actions_adddup2(&actions,STDIN_FILENO,STDOUT_FILENO);if(!error)error=posix_spawn_file_actions_adddup2(&actions,STDIN_FILENO,STDERR_FILENO);
#endif
#if defined(STRUT_GLIBC_SPAWN_CLOSEFROM)
        if(!error)error=posix_spawn_file_actions_addclosefrom_np(&actions,STDERR_FILENO+1);
#endif
        if(!error&&!options.launch.cwd.v.empty()){
            error=strut_posix_spawn_addchdir(&actions,options.launch.cwd.v.c_str());
        }
        short flags=static_cast<short>(STRUT_PTY_POSIX_SPAWN_SETSID);
#ifdef __APPLE__
        flags=static_cast<short>(flags|POSIX_SPAWN_CLOEXEC_DEFAULT);
#endif
        if(!error)error=posix_spawnattr_setflags(&attributes,flags);
#ifdef __APPLE__
        const auto launcher=strut_pty_self_executable();std::vector<std::string> launcher_args={launcher,"--strut-internal-pty-launcher-v1",std::to_string(slave),std::to_string(launch_status[1]),executable};launcher_args.insert(launcher_args.end(),data.args.begin(),data.args.end());std::vector<char*> launcher_argv;launcher_argv.reserve(launcher_args.size()+1);for(auto& argument:launcher_args)launcher_argv.push_back(argument.data());launcher_argv.push_back(nullptr);if(!error)error=posix_spawn(&pid,launcher.c_str(),&actions,&attributes,launcher_argv.data(),data.envp.data());
#else
        if(!error)error=posix_spawn(&pid,executable.c_str(),&actions,&attributes,data.argv.data(),data.envp.data());
#endif
        if(error)throw strut_checked_error("PtyError",std::string("PTY posix_spawn failed: ")+std::strerror(error));
        posix_spawnattr_destroy(&attributes);attributes_ready=false;posix_spawn_file_actions_destroy(&actions);actions_ready=false;strut_close_fd(slave);
#ifdef __APPLE__
        strut_close_fd(launch_status[1]);int launch_error=0;ssize_t launch_count;do{launch_count=::read(launch_status[0],&launch_error,sizeof(launch_error));}while(launch_count<0&&errno==EINTR);const int launch_read_error=launch_count<0?errno:0;strut_close_fd(launch_status[0]);if(launch_count!=0){if(launch_count<0)launch_error=launch_read_error;throw strut_checked_error("PtyError",std::string("PTY target launch failed: ")+std::strerror(launch_error));}
#endif
    }catch(...){if(attributes_ready)posix_spawnattr_destroy(&attributes);if(actions_ready)posix_spawn_file_actions_destroy(&actions);strut_close_fd(master);strut_close_fd(slave);
#ifdef __APPLE__
        strut_close_pipe(launch_status);
#endif
        if(pid>0){(void)::kill(-pid,SIGKILL);int ignored=0;while(waitpid(pid,&ignored,0)<0&&errno==EINTR){}}throw;}
    spawn_lock.unlock();
    const int status=fcntl(master,F_GETFL,0);if(status<0||fcntl(master,F_SETFL,status|O_NONBLOCK)<0){const int saved=errno;strut_close_fd(master);(void)::kill(-pid,SIGKILL);int ignored=0;while(waitpid(pid,&ignored,0)<0&&errno==EINTR){}throw strut_checked_error("PtyError",std::string("PTY nonblocking setup failed: ")+std::strerror(saved));}
    try{auto state=std::make_shared<strut_pty_state>();state->master=master;state->pid=pid;state->closed=false;state->running_value=true;state->token=token;return strut_pty(std::move(state));}catch(...){strut_close_fd(master);(void)::kill(-pid,SIGKILL);int ignored=0;while(waitpid(pid,&ignored,0)<0&&errno==EINTR){}throw;}
#endif
}
inline strut_pty strut_pty_spawn(const strut_string& program,const std::vector<strut_string>& arguments){return strut_pty_spawn_impl(program,arguments,{},{});}
inline strut_pty strut_pty_spawn(const strut_string& program,const std::vector<strut_string>& arguments,const json::Document& options){return strut_pty_spawn_impl(program,arguments,strut_parse_pty_options(options),{});}
inline strut_pty strut_pty_spawn(const strut_string& program,const std::vector<strut_string>& arguments,const strut_process_cancellation_token& token){return strut_pty_spawn_impl(program,arguments,{},token);}
inline strut_pty strut_pty_spawn(const strut_string& program,const std::vector<strut_string>& arguments,const json::Document& options,const strut_process_cancellation_token& token){return strut_pty_spawn_impl(program,arguments,strut_parse_pty_options(options),token);}
#endif
)STRUT_PTY";
}

void emit_executor(std::ostream& out) {
    out << R"STRUT_ASYNC(
#ifndef STRUT_EXECUTION_CONTEXT_DEFINED
#define STRUT_EXECUTION_CONTEXT_DEFINED
inline thread_local const void* strut_execution_context=nullptr;
#endif
class strut_executor;
inline thread_local strut_executor* strut_current_executor=nullptr;
class strut_executor {
public:
    strut_executor(){auto n=std::thread::hardware_concurrency();if(n<2)n=2;if(n>32)n=32;max_queue_=static_cast<std::size_t>(n)*8;try{for(unsigned i=0;i<n;++i)workers_.emplace_back([this]{worker();});}catch(...){{std::lock_guard<std::mutex> g(m_);stopping_=true;}cv_.notify_all();for(auto& t:workers_)if(t.joinable())t.join();throw;}}
    ~strut_executor(){{std::lock_guard<std::mutex> g(m_);stopping_=true;}cv_.notify_all();for(auto& t:workers_)if(t.joinable())t.join();}
    template<class F> auto submit(F&& f)->std::future<std::invoke_result_t<F>>{using R=std::invoke_result_t<F>;auto task=std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));auto fut=task->get_future();const void* context=strut_execution_context;std::function<void()> job=[task,context]{const void* previous=strut_execution_context;strut_execution_context=context;(*task)();strut_execution_context=previous;};bool run_here=strut_current_executor==this;{std::lock_guard<std::mutex> g(m_);if(stopping_)throw std::runtime_error("executor is stopping");if(!run_here&&q_.size()>=max_queue_)run_here=true;else if(!run_here)q_.emplace(std::move(job));}if(run_here)job();else cv_.notify_one();return fut;}
private:
    void worker(){strut_current_executor=this;for(;;){std::function<void()> job;{std::unique_lock<std::mutex> l(m_);cv_.wait(l,[this]{return stopping_||!q_.empty();});if(stopping_&&q_.empty()){strut_current_executor=nullptr;return;}job=std::move(q_.front());q_.pop();}job();}}
    std::vector<std::thread> workers_;std::queue<std::function<void()>> q_;std::size_t max_queue_=16;std::mutex m_;std::condition_variable cv_;bool stopping_=false;
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
inline int strut_socket_poll(strut_socket_handle h,short events,int timeout_ms){const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);for(;;){WSAPOLLFD descriptor{h,events,0};const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()+std::chrono::milliseconds(1)).count();const int result=WSAPoll(&descriptor,1,static_cast<INT>(std::max<std::int64_t>(0,remaining)));if(result!=SOCKET_ERROR||WSAGetLastError()!=WSAEINTR)return result;if(std::chrono::steady_clock::now()>=deadline)return 0;}}
inline int strut_socket_poll_read(strut_socket_handle h,int timeout_ms){return strut_socket_poll(h,POLLIN,timeout_ms);}inline int strut_socket_poll_write(strut_socket_handle h,int timeout_ms){return strut_socket_poll(h,POLLOUT,timeout_ms);}
struct strut_winsock_runtime{strut_winsock_runtime(){WSADATA d{};if(WSAStartup(MAKEWORD(2,2),&d)!=0)throw strut_checked_error("NetworkError","WSAStartup failed");}~strut_winsock_runtime(){WSACleanup();}};
inline void strut_socket_init(){static strut_winsock_runtime runtime;(void)runtime;}
#else
#include <fcntl.h>
#include <poll.h>
using strut_socket_handle=int; constexpr strut_socket_handle strut_invalid_socket=-1;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket){::shutdown(h,SHUT_RDWR);::close(h);}}
inline bool strut_socket_set_blocking(strut_socket_handle h,bool blocking){const int flags=fcntl(h,F_GETFL,0);return flags>=0&&fcntl(h,F_SETFL,blocking?(flags&~O_NONBLOCK):(flags|O_NONBLOCK))==0;}
inline bool strut_socket_would_block(){return errno==EAGAIN||errno==EWOULDBLOCK;}
inline int strut_socket_poll(strut_socket_handle h,short events,int timeout_ms){const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);for(;;){pollfd descriptor{h,events,0};const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()+std::chrono::milliseconds(1)).count();const int result=poll(&descriptor,1,static_cast<int>(std::max<std::int64_t>(0,remaining)));if(result>=0||(errno!=EINTR&&errno!=EAGAIN))return result;if(std::chrono::steady_clock::now()>=deadline)return 0;}}
inline int strut_socket_poll_read(strut_socket_handle h,int timeout_ms){return strut_socket_poll(h,POLLIN,timeout_ms);}inline int strut_socket_poll_write(strut_socket_handle h,int timeout_ms){return strut_socket_poll(h,POLLOUT,timeout_ms);}
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
struct strut_socket_state:std::enable_shared_from_this<strut_socket_state>{mutable std::mutex mutex;std::condition_variable cv;strut_socket_handle handle=strut_invalid_socket;std::size_t operations=0;bool closing=false;std::atomic<bool> interrupted{false};std::atomic<std::int32_t> read_timeout_ms{0};~strut_socket_state(){strut_socket_close(handle);}strut_socket_operation acquire(){std::lock_guard<std::mutex> lock(mutex);if(closing||handle==strut_invalid_socket)throw strut_checked_error("NetworkError","operation on closed socket");++operations;return {shared_from_this(),handle};}void release(){std::lock_guard<std::mutex> lock(mutex);if(operations>0)--operations;if(operations==0)cv.notify_all();}void interrupt(){std::lock_guard<std::mutex> lock(mutex);if(handle==strut_invalid_socket)return;interrupted.store(true);
#ifdef _WIN32
    ::shutdown(handle,SD_BOTH);
#else
    ::shutdown(handle,SHUT_RDWR);
#endif
}void close(){std::unique_lock<std::mutex> lock(mutex);if(closing){cv.wait(lock,[&]{return !closing;});return;}if(handle==strut_invalid_socket)return;closing=true;interrupted.store(true);const auto native=handle;
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
    void close_after_write(bool drain_input=true){if(!s_)return;try{auto operation=s_->acquire();
#ifdef _WIN32
        ::shutdown(operation.handle,SD_SEND);
#else
        ::shutdown(operation.handle,SHUT_WR);
#endif
        (void)drain_input;if(strut_socket_set_blocking(operation.handle,false)){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);std::size_t drained=0;char buffer[4096];while(drained<65536){const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()+std::chrono::milliseconds(1)).count();if(remaining<=0||strut_socket_poll_read(operation.handle,static_cast<int>(remaining))<=0)break;
#ifdef _WIN32
            const int count=::recv(operation.handle,buffer,sizeof(buffer),0);
#else
            const ssize_t count=::recv(operation.handle,buffer,sizeof(buffer),0);
#endif
            if(count<0&&strut_socket_would_block())continue;if(count<=0)break;drained+=static_cast<std::size_t>(count);}}
        }catch(...){ }close();}
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
        const auto timeout=s_->read_timeout_ms.load();if(timeout>0){const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);for(;;){if(s_->interrupted.load())throw strut_checked_error("NetworkError","socket read interrupted");const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()+std::chrono::milliseconds(1)).count();if(remaining<=0)throw strut_checked_error("NetworkError","socket read failed");const int ready=strut_socket_poll_read(h,static_cast<int>(std::min<std::int64_t>(remaining,100)));if(ready<0)throw strut_checked_error("NetworkError","socket read failed");if(ready>0)break;}if(s_->interrupted.load())throw strut_checked_error("NetworkError","socket read interrupted");}
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

void emit_http_client(std::ostream& out, bool async, bool streaming, bool curl_global) {
    if (curl_global) out << R"HTTP_CLIENT(
inline void strut_curl_init(const char* error_type="HttpError"){static const CURLcode initialized=curl_global_init(CURL_GLOBAL_DEFAULT);if(initialized!=CURLE_OK)throw strut_checked_error(error_type,"libcurl global initialization failed",static_cast<std::int32_t>(initialized));static const bool supported=[](){const auto* info=curl_version_info(CURLVERSION_NOW);return info&&info->version_num>=0x074000;}();if(!supported)throw strut_checked_error(error_type,"libcurl 7.64.0 or newer is required",-101);}
)HTTP_CLIENT";
    if (streaming) out << "#define STRUT_HTTP_CLIENT_STREAMING 1\n";
    out << R"HTTP_CLIENT(
#if LIBCURL_VERSION_NUM < 0x074000
#error "Strut HTTP requires libcurl 7.64.0 or newer"
#endif
#include <array>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <unordered_set>
struct strut_http_response {
    std::int32_t status=0; strut_string body; std::unordered_map<strut_string,strut_string> headers;
    json::Document json() const { json::Document d;json::ParseDiagnostic diag;if(!json::Document::parse(body.v,d,diag))throw strut_checked_error("HttpError","response body is not valid JSON");return d; }
};
#ifdef STRUT_HTTP_CLIENT_STREAMING
struct strut_http_response_head {std::int32_t status=0;std::unordered_map<strut_string,strut_string> headers;};
#endif
inline bool strut_http_token_text(const std::string& value){if(value.empty())return false;for(unsigned char c:value)if(!(std::isalnum(c)||c=='!'||c=='#'||c=='$'||c=='%'||c=='&'||c=='\''||c=='*'||c=='+'||c=='-'||c=='.'||c=='^'||c=='_'||c=='`'||c=='|'||c=='~'))return false;return true;}
inline bool strut_http_metadata_text(const std::string& value){for(unsigned char c:value)if(c==0||c==0x7f||(c<0x20&&c!='\t'))return false;return true;}
inline bool strut_http_plain_text(const std::string& value){for(unsigned char c:value)if(c<=0x20||c==0x7f)return false;return true;}
inline bool strut_http_path_text(const std::string& value){if(value.empty())return false;for(unsigned char c:value)if(c<0x20||c==0x7f)return false;return true;}
inline std::string strut_http_ascii_lower(std::string value){for(char& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
inline int strut_http_status_code(const std::string& line){std::size_t code=std::string::npos;if(line.rfind("HTTP/1.0 ",0)==0||line.rfind("HTTP/1.1 ",0)==0)code=9;else if(line.rfind("HTTP/2 ",0)==0||line.rfind("HTTP/3 ",0)==0)code=7;if(code==std::string::npos||line.size()<code+3||!std::isdigit(static_cast<unsigned char>(line[code]))||!std::isdigit(static_cast<unsigned char>(line[code+1]))||!std::isdigit(static_cast<unsigned char>(line[code+2]))||(line.size()>code+3&&line[code+3]!=' ')||!strut_http_metadata_text(line))return 0;const int status=(line[code]-'0')*100+(line[code+1]-'0')*10+(line[code+2]-'0');return status>=100&&status<=599?status:0;}
inline bool strut_http_url_text(const std::string& value,bool require_https,bool& https){if(value.empty()||!strut_http_plain_text(value))return false;std::string lower=value.substr(0,std::min<std::size_t>(value.size(),8));lower=strut_http_ascii_lower(std::move(lower));std::size_t authority=0;if(lower.rfind("https://",0)==0){https=true;authority=8;}else if(!require_https&&lower.rfind("http://",0)==0){https=false;authority=7;}else return false;const auto end=value.find_first_of("/?#",authority);const auto host=value.substr(authority,end==std::string::npos?std::string::npos:end-authority);return !host.empty()&&host.find('@')==std::string::npos;}
inline std::size_t strut_http_size_option(const json::Document& options,const char* name,std::size_t fallback){if(!options.has(name))return fallback;const auto& value=options[name];constexpr double exact_max=9007199254740991.0;const double native_max=sizeof(std::size_t)<8?static_cast<double>(std::numeric_limits<std::size_t>::max()):exact_max;if(value.type!=json::Type::Number||!std::isfinite(value.num)||value.num<0||std::floor(value.num)!=value.num||value.num>native_max)throw strut_checked_error("HttpError",std::string(name)+" must be a non-negative integral byte count",-101);return static_cast<std::size_t>(value.num);}
inline long strut_http_long_option(const json::Document& options,const char* name,long fallback,bool positive,long maximum){if(!options.has(name))return fallback;const auto& value=options[name];constexpr double exact_max=9007199254740991.0;const double native_max=sizeof(long)<8?static_cast<double>(maximum):std::min(exact_max,static_cast<double>(maximum));if(value.type!=json::Type::Number||!std::isfinite(value.num)||std::floor(value.num)!=value.num||value.num>native_max||(positive?value.num<=0:value.num<0))throw strut_checked_error("HttpError",std::string(name)+" is out of range",-101);return static_cast<long>(value.num);}
struct strut_http_transfer_state{strut_http_response response;std::unordered_map<strut_string,strut_string> current_headers;std::size_t max_body=16*1024*1024,max_request=16*1024*1024,max_header_bytes=64*1024,max_header_count=100,header_bytes=0,header_count=0,response_bytes=0,upload_bytes=0;int current_status=0;bool in_headers=false,follow=true,suppress_body=false,custom_method=false;const char* failure=nullptr;std::int32_t failure_code=-100;std::array<char,CURL_ERROR_SIZE> error{};
#ifdef STRUT_HTTP_CLIENT_STREAMING
bool streaming=false,stopped=false,post_upload=false,discard_upload=false;std::optional<std::function<strut_bytes(std::int64_t)>> upload;std::optional<std::function<bool(strut_bytes)>> download;const strut_cancellation_token* cancellation=nullptr;std::optional<std::size_t> request_length;std::exception_ptr callback_error;
#endif
void fail(const char* message,std::int32_t code=-100) noexcept{if(!failure){failure=message;failure_code=code;}}};)HTTP_CLIENT"; out << R"HTTP_CLIENT(
inline size_t strut_http_write_cb(char* data,size_t size,size_t count,void* user) noexcept{auto* state=static_cast<strut_http_transfer_state*>(user);if(count&&size>std::numeric_limits<std::size_t>::max()/count){state->fail("HTTP response body size overflow");return 0;}const std::size_t amount=size*count;if(amount>state->max_body-state->response_bytes){state->fail("HTTP response body exceeds max_response_body_bytes");return 0;}state->response_bytes+=amount;if(state->suppress_body)return amount;try{
#ifdef STRUT_HTTP_CLIENT_STREAMING
if(state->streaming){if(!state->download)return amount;strut_bytes chunk(static_cast<std::int64_t>(amount));if(amount)std::memcpy(chunk.data(),data,amount);bool keep=true;try{keep=(*state->download)(std::move(chunk));}catch(...){state->callback_error=std::current_exception();state->fail("HTTP download callback failed",-104);return 0;}if(state->cancellation&&state->cancellation->cancelled()){state->fail("HTTP transfer cancelled",-103);return 0;}if(!keep){state->stopped=true;return 0;}return amount;}
#endif
state->response.body.v.append(data,amount);return amount;}catch(...){state->fail("HTTP response body allocation failed",-102);return 0;}}
inline size_t strut_http_header_cb(char* data,size_t size,size_t count,void* user) noexcept{auto* state=static_cast<strut_http_transfer_state*>(user);if(count&&size>std::numeric_limits<std::size_t>::max()/count){state->fail("HTTP response header size overflow");return 0;}const std::size_t amount=size*count;if(amount>state->max_header_bytes-state->header_bytes){state->fail("HTTP response headers exceed max_response_header_bytes");return 0;}state->header_bytes+=amount;try{std::string line(data,amount);if(line=="\r\n"||line=="\n"){if(state->in_headers){state->response.headers=state->current_headers;state->response.status=state->current_status;state->in_headers=false;const auto location=state->current_headers.find(strut_string("location"));const bool redirect=state->follow&&(state->current_status==301||state->current_status==302||state->current_status==303||state->current_status==307||state->current_status==308)&&location!=state->current_headers.end()&&!location->second.v.empty();state->suppress_body=state->current_status/100==1||redirect;if(redirect&&state->custom_method&&(state->current_status==301||state->current_status==302||state->current_status==303)){state->fail("automatic redirect for custom HTTP method is not supported",-105);return 0;}
#ifdef STRUT_HTTP_CLIENT_STREAMING
if(redirect&&state->upload&&(state->current_status==307||state->current_status==308)){state->fail("streamed request body cannot be replayed across redirect",-105);return 0;}
#endif
}return amount;}while(!line.empty()&&(line.back()=='\r'||line.back()=='\n'))line.pop_back();if(line.rfind("HTTP/",0)==0){const int status=strut_http_status_code(line);if(state->in_headers||status==0){state->fail("invalid HTTP response status line",-101);return 0;}state->response.body.v.clear();state->response_bytes=0;state->current_headers.clear();state->current_status=status;state->suppress_body=false;state->in_headers=true;return amount;}if(!state->in_headers){state->fail("HTTP response trailers are not supported",-101);return 0;}if(++state->header_count>state->max_header_count){state->fail("HTTP response headers exceed max_response_header_count");return 0;}const auto colon=line.find(':');if(colon==std::string::npos){state->fail("malformed HTTP response header",-101);return 0;}std::string name=strut_http_ascii_lower(line.substr(0,colon)),value=line.substr(colon+1);while(!value.empty()&&(value.front()==' '||value.front()=='\t'))value.erase(value.begin());while(!value.empty()&&(value.back()==' '||value.back()=='\t'))value.pop_back();if(!strut_http_token_text(name)||!strut_http_metadata_text(value)){state->fail("invalid HTTP response header",-101);return 0;}strut_string key(name);if(state->current_headers.find(key)!=state->current_headers.end()){state->fail("duplicate HTTP response header",-101);return 0;}state->current_headers.emplace(std::move(key),strut_string(std::move(value)));return amount;}catch(...){state->fail("HTTP response header allocation failed",-102);return 0;}}
#ifdef STRUT_HTTP_CLIENT_STREAMING
inline size_t strut_http_read_cb(char* data,size_t size,size_t count,void* user) noexcept{auto* state=static_cast<strut_http_transfer_state*>(user);if(state->discard_upload)return 0;if(count&&size>std::numeric_limits<std::size_t>::max()/count){state->fail("HTTP upload chunk size overflow");return CURL_READFUNC_ABORT;}const std::size_t capacity=size*count;if(!capacity)return 0;std::size_t requested=capacity;if(state->request_length)requested=std::min(requested,*state->request_length-state->upload_bytes);if(!requested)return 0;try{strut_bytes chunk;try{chunk=(*state->upload)(static_cast<std::int64_t>(requested));}catch(...){state->callback_error=std::current_exception();state->fail("HTTP upload callback failed",-104);return CURL_READFUNC_ABORT;}if(state->cancellation&&state->cancellation->cancelled()){state->fail("HTTP transfer cancelled",-103);return CURL_READFUNC_ABORT;}const std::size_t amount=chunk.native_size();if(amount>requested){state->fail("HTTP upload callback exceeded requested chunk size",-104);return CURL_READFUNC_ABORT;}if(amount==0){if(state->request_length&&state->upload_bytes!=*state->request_length){state->fail("HTTP upload ended before request_body_length",-104);return CURL_READFUNC_ABORT;}return 0;}if(amount>state->max_request-state->upload_bytes){state->fail("HTTP request body exceeds max_request_body_bytes");return CURL_READFUNC_ABORT;}std::memcpy(data,chunk.data(),amount);state->upload_bytes+=amount;return amount;}catch(...){state->fail("HTTP upload callback allocation failed",-102);return CURL_READFUNC_ABORT;}}
inline int strut_http_seek_cb(void* user,curl_off_t offset,int origin) noexcept{auto* state=static_cast<strut_http_transfer_state*>(user);if(state->post_upload&&offset==0&&origin==SEEK_SET&&(state->current_status==301||state->current_status==302||state->current_status==303)){state->discard_upload=true;return CURL_SEEKFUNC_OK;}state->fail("streamed request body cannot be replayed",-105);return CURL_SEEKFUNC_FAIL;}
inline int strut_http_progress_cb(void* user,curl_off_t,curl_off_t,curl_off_t,curl_off_t) noexcept{auto* state=static_cast<strut_http_transfer_state*>(user);if(state->stopped)return 0;if(state->cancellation&&state->cancellation->cancelled()){state->fail("HTTP transfer cancelled",-103);return 1;}return 0;}
#endif
struct strut_curl_easy_delete{void operator()(CURL* value) const noexcept{if(value)curl_easy_cleanup(value);}};using strut_curl_easy=std::unique_ptr<CURL,strut_curl_easy_delete>;
struct strut_curl_headers_delete{void operator()(curl_slist* value) const noexcept{if(value)curl_slist_free_all(value);}};using strut_curl_headers=std::unique_ptr<curl_slist,strut_curl_headers_delete>;
template<class T>inline void strut_http_setopt(CURL* curl,CURLoption option,T value){const auto code=curl_easy_setopt(curl,option,value);if(code!=CURLE_OK)throw strut_checked_error("HttpError","unable to configure HTTP transfer",static_cast<std::int32_t>(code));})HTTP_CLIENT"; out << R"HTTP_CLIENT(
inline strut_http_response strut_http_request_impl(const strut_string& method,const strut_string& url,const json::Document& options,const strut_string* ca_file,strut_http_transfer_state* supplied=nullptr){strut_curl_init("HttpError");strut_http_transfer_state local;auto& state=supplied?*supplied:local;if(!strut_http_token_text(method.v))throw strut_checked_error("HttpError","HTTP method must be a non-empty token",-101);if(options.type!=json::Type::Null&&options.type!=json::Type::Object)throw strut_checked_error("HttpError","HTTP options must be JSON object or null",-101);static const std::unordered_set<std::string> allowed={"timeout_ms","follow_redirects","max_redirects","max_response_body_bytes","max_response_header_bytes","max_response_header_count","max_request_body_bytes","request_body_length","ca_file","body","headers"};if(options.type==json::Type::Object)for(const auto& item:options.object)if(allowed.find(item.first)==allowed.end())throw strut_checked_error("HttpError","unknown HTTP option: "+item.first,-101);strut_string option_ca;const strut_string* effective_ca=ca_file;if(options.type==json::Type::Object&&options.has("ca_file")){if(ca_file||options["ca_file"].type!=json::Type::String)throw strut_checked_error("HttpError","ca_file must be a string",-101);option_ca=strut_string(options["ca_file"].string);effective_ca=&option_ca;}bool https=false;if(!strut_http_url_text(url.v,effective_ca!=nullptr,https))throw strut_checked_error("HttpError",effective_ca?"explicit-CA request requires an absolute HTTPS URL":"HTTP URL must be absolute and use http or https",-101);if(effective_ca&&!strut_http_path_text(effective_ca->v))throw strut_checked_error("HttpError","CA file path is invalid",-101);
    long timeout=30000,max_redirects=10;bool follow=true,body_present=false;std::string body;if(options.type==json::Type::Object){timeout=strut_http_long_option(options,"timeout_ms",timeout,true,std::numeric_limits<long>::max());max_redirects=strut_http_long_option(options,"max_redirects",max_redirects,false,50);state.max_body=strut_http_size_option(options,"max_response_body_bytes",state.max_body);state.max_header_bytes=strut_http_size_option(options,"max_response_header_bytes",state.max_header_bytes);state.max_header_count=strut_http_size_option(options,"max_response_header_count",state.max_header_count);state.max_request=strut_http_size_option(options,"max_request_body_bytes",state.max_request);if(options.has("follow_redirects")){if(options["follow_redirects"].type!=json::Type::Boolean)throw strut_checked_error("HttpError","follow_redirects must be boolean",-101);follow=options["follow_redirects"].boolean;}if(options.has("body")){body_present=true;const auto& value=options["body"];body=value.type==json::Type::String?value.string:value.dump();if(body.size()>state.max_request)throw strut_checked_error("HttpError","HTTP request body exceeds max_request_body_bytes",-100);}}
    state.follow=follow;
#ifdef STRUT_HTTP_CLIENT_STREAMING
    const bool stream_upload=state.streaming&&state.upload.has_value();if(options.type==json::Type::Object&&options.has("request_body_length")){if(!stream_upload)throw strut_checked_error("HttpError","request_body_length requires a streaming upload",-101);state.request_length=strut_http_size_option(options,"request_body_length",0);if(*state.request_length>state.max_request)throw strut_checked_error("HttpError","request_body_length exceeds max_request_body_bytes",-100);}if(stream_upload&&body_present)throw strut_checked_error("HttpError","streaming upload and buffered body are mutually exclusive",-101);if(state.cancellation&&state.cancellation->cancelled())throw strut_checked_error("HttpError","HTTP transfer cancelled",-103);
#else
    const bool stream_upload=false;if(options.type==json::Type::Object&&options.has("request_body_length"))throw strut_checked_error("HttpError","request_body_length requires a streaming upload",-101);
#endif
    strut_curl_headers headers(nullptr);std::unordered_set<std::string> names;if(options.type==json::Type::Object&&options.has("headers")){const auto& value=options["headers"];if(value.type!=json::Type::Object)throw strut_checked_error("HttpError","headers must be a JSON object",-101);std::size_t bytes=0;if(value.object.size()>100)throw strut_checked_error("HttpError","HTTP request headers exceed 100 fields",-100);for(const auto& item:value.object){if(item.second.type!=json::Type::String)throw strut_checked_error("HttpError","HTTP request header values must be strings",-101);const std::string lower=strut_http_ascii_lower(item.first);if(!strut_http_token_text(item.first)||!strut_http_metadata_text(item.second.string)||!names.insert(lower).second)throw strut_checked_error("HttpError","invalid or duplicate HTTP request header",-101);if(lower=="content-length"||lower=="transfer-encoding"||lower=="connection"||lower=="host")throw strut_checked_error("HttpError","HTTP request framing header is transport-owned",-101);if(item.first.size()>std::numeric_limits<std::size_t>::max()-item.second.string.size()-4)throw strut_checked_error("HttpError","HTTP request header size overflow",-100);const std::size_t amount=item.first.size()+item.second.string.size()+4;if(amount>64*1024||bytes>64*1024-amount)throw strut_checked_error("HttpError","HTTP request headers exceed 65536 bytes",-100);bytes+=amount;const std::string line=item.first+": "+item.second.string;curl_slist* appended=curl_slist_append(headers.get(),line.c_str());if(!appended)throw strut_checked_error("HttpError","HTTP request header allocation failed",-102);headers.release();headers.reset(appended);}}
    state.custom_method=method.v!="GET"&&method.v!="HEAD"&&method.v!="POST";if(method.v=="HEAD"&&(body_present||stream_upload))throw strut_checked_error("HttpError","HEAD requests cannot contain a body",-101);strut_curl_easy curl(curl_easy_init());if(!curl)throw strut_checked_error("HttpError","curl_easy_init failed",static_cast<std::int32_t>(CURLE_FAILED_INIT));strut_http_setopt(curl.get(),CURLOPT_ERRORBUFFER,state.error.data());strut_http_setopt(curl.get(),CURLOPT_URL,url.v.c_str());strut_http_setopt(curl.get(),CURLOPT_NOSIGNAL,1L);strut_http_setopt(curl.get(),CURLOPT_FOLLOWLOCATION,follow?1L:0L);strut_http_setopt(curl.get(),CURLOPT_MAXREDIRS,max_redirects);strut_http_setopt(curl.get(),CURLOPT_TIMEOUT_MS,timeout);strut_http_setopt(curl.get(),CURLOPT_SSL_VERIFYPEER,1L);strut_http_setopt(curl.get(),CURLOPT_SSL_VERIFYHOST,2L);strut_http_setopt(curl.get(),CURLOPT_UNRESTRICTED_AUTH,0L);strut_http_setopt(curl.get(),CURLOPT_HEADEROPT,CURLHEADER_SEPARATE);
#if LIBCURL_VERSION_NUM >= 0x075500
    strut_http_setopt(curl.get(),CURLOPT_PROTOCOLS_STR,"http,https");strut_http_setopt(curl.get(),CURLOPT_REDIR_PROTOCOLS_STR,https?"https":"http,https");
#else
    strut_http_setopt(curl.get(),CURLOPT_PROTOCOLS,CURLPROTO_HTTP|CURLPROTO_HTTPS);strut_http_setopt(curl.get(),CURLOPT_REDIR_PROTOCOLS,https?CURLPROTO_HTTPS:(CURLPROTO_HTTP|CURLPROTO_HTTPS));
#endif
    strut_http_setopt(curl.get(),CURLOPT_WRITEFUNCTION,strut_http_write_cb);strut_http_setopt(curl.get(),CURLOPT_WRITEDATA,&state);strut_http_setopt(curl.get(),CURLOPT_HEADERFUNCTION,strut_http_header_cb);strut_http_setopt(curl.get(),CURLOPT_HEADERDATA,&state);if(headers)strut_http_setopt(curl.get(),CURLOPT_HTTPHEADER,headers.get());if(effective_ca)strut_http_setopt(curl.get(),CURLOPT_CAINFO,effective_ca->v.c_str());)HTTP_CLIENT"; out << R"HTTP_CLIENT(
#ifdef STRUT_HTTP_CLIENT_STREAMING
    if(state.streaming){strut_http_setopt(curl.get(),CURLOPT_NOPROGRESS,0L);strut_http_setopt(curl.get(),CURLOPT_XFERINFOFUNCTION,strut_http_progress_cb);strut_http_setopt(curl.get(),CURLOPT_XFERINFODATA,&state);}if(stream_upload){strut_http_setopt(curl.get(),CURLOPT_READFUNCTION,strut_http_read_cb);strut_http_setopt(curl.get(),CURLOPT_READDATA,&state);strut_http_setopt(curl.get(),CURLOPT_SEEKFUNCTION,strut_http_seek_cb);strut_http_setopt(curl.get(),CURLOPT_SEEKDATA,&state);if(state.request_length&&*state.request_length>static_cast<std::uint64_t>(std::numeric_limits<curl_off_t>::max()))throw strut_checked_error("HttpError","request_body_length is too large for libcurl",-100);const curl_off_t length=state.request_length?static_cast<curl_off_t>(*state.request_length):static_cast<curl_off_t>(-1);if(method.v=="POST"){state.post_upload=true;strut_http_setopt(curl.get(),CURLOPT_POST,1L);strut_http_setopt(curl.get(),CURLOPT_POSTFIELDSIZE_LARGE,length);}else{strut_http_setopt(curl.get(),CURLOPT_UPLOAD,1L);strut_http_setopt(curl.get(),CURLOPT_INFILESIZE_LARGE,length);strut_http_setopt(curl.get(),CURLOPT_CUSTOMREQUEST,method.v.c_str());}}
#endif
    if(body_present){if(body.size()>static_cast<std::uint64_t>(std::numeric_limits<curl_off_t>::max()))throw strut_checked_error("HttpError","HTTP request body is too large for libcurl",-100);strut_http_setopt(curl.get(),CURLOPT_POSTFIELDS,body.data());strut_http_setopt(curl.get(),CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(body.size()));}if(!stream_upload){if(method.v=="GET"&&!body_present)strut_http_setopt(curl.get(),CURLOPT_HTTPGET,1L);else if(method.v=="HEAD")strut_http_setopt(curl.get(),CURLOPT_NOBODY,1L);else if(method.v=="POST")strut_http_setopt(curl.get(),CURLOPT_POST,1L);else strut_http_setopt(curl.get(),CURLOPT_CUSTOMREQUEST,method.v.c_str());}const CURLcode code=curl_easy_perform(curl.get());
#ifdef STRUT_HTTP_CLIENT_STREAMING
    if(state.callback_error){try{std::rethrow_exception(state.callback_error);}catch(const std::exception& error){throw strut_checked_error("HttpError",std::string("HTTP streaming callback failed: ")+error.what(),-104);}catch(...){throw strut_checked_error("HttpError","HTTP streaming callback failed",-104);}}if(state.stopped&&!state.failure){long status=0;const auto info=curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&status);if(info!=CURLE_OK)throw strut_checked_error("HttpError","unable to read HTTP response status",static_cast<std::int32_t>(info));state.response.status=static_cast<std::int32_t>(status);return std::move(state.response);}
#endif
    if(state.failure)throw strut_checked_error("HttpError",state.failure,state.failure_code);if(code!=CURLE_OK){std::string detail=state.error[0]?state.error.data():curl_easy_strerror(code);throw strut_checked_error("HttpError","HTTP transfer failed: "+detail,static_cast<std::int32_t>(code));}long status=0;const auto info=curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&status);if(info!=CURLE_OK)throw strut_checked_error("HttpError","unable to read HTTP response status",static_cast<std::int32_t>(info));state.response.status=static_cast<std::int32_t>(status);return std::move(state.response);}
inline strut_http_response http_request(const strut_string& method,const strut_string& url,const json::Document& options){return strut_http_request_impl(method,url,options,nullptr);}
inline strut_http_response http_request(const strut_string& method,const strut_string& url){return http_request(method,url,json::Document(nullptr));}
inline strut_http_response http_get(const strut_string& url){return http_request(strut_string("GET"),url);}
inline strut_http_response http_get_ca(const strut_string& url,const strut_string& ca_file){return strut_http_request_impl(strut_string("GET"),url,json::Document(nullptr),&ca_file);}
inline json::Document http_get_json(const strut_string& url){return http_get(url).json();}
#ifdef STRUT_HTTP_CLIENT_STREAMING
inline strut_http_response_head http_request_stream_impl(const strut_string& method,const strut_string& url,const json::Document& options,std::optional<std::function<strut_bytes(std::int64_t)>> upload,std::optional<std::function<bool(strut_bytes)>> download,const strut_cancellation_token* cancellation){strut_http_transfer_state state;state.streaming=true;state.upload=std::move(upload);state.download=std::move(download);state.cancellation=cancellation;auto response=strut_http_request_impl(method,url,options,nullptr,&state);return {response.status,std::move(response.headers)};}
inline strut_http_response_head http_request_stream(const strut_string& method,const strut_string& url,const json::Document& options,std::optional<std::function<strut_bytes(std::int64_t)>> upload,std::optional<std::function<bool(strut_bytes)>> download){return http_request_stream_impl(method,url,options,std::move(upload),std::move(download),nullptr);}
inline strut_http_response_head http_request_stream(const strut_string& method,const strut_string& url,const json::Document& options,std::optional<std::function<strut_bytes(std::int64_t)>> upload,std::optional<std::function<bool(strut_bytes)>> download,strut_cancellation_token cancellation){return http_request_stream_impl(method,url,options,std::move(upload),std::move(download),&cancellation);}
#endif
)HTTP_CLIENT";
    if (async) out << R"HTTP_CLIENT(
inline strut_future<strut_http_response> http_get_async(const strut_string& url){return strut_async([url]{return http_get(url);});}
inline strut_future<strut_http_response> http_request_async(const strut_string& method,const strut_string& url,const json::Document& options){return strut_async([method,url,options]{return http_request(method,url,options);});}
#ifdef STRUT_HTTP_CLIENT_STREAMING
inline strut_future<strut_http_response_head> http_request_stream_async(const strut_string& method,const strut_string& url,const json::Document& options,std::optional<std::function<strut_bytes(std::int64_t)>> upload,std::optional<std::function<bool(strut_bytes)>> download){return strut_async([method,url,options,upload=std::move(upload),download=std::move(download)]() mutable{return http_request_stream(method,url,options,std::move(upload),std::move(download));});}
inline strut_future<strut_http_response_head> http_request_stream_async(const strut_string& method,const strut_string& url,const json::Document& options,std::optional<std::function<strut_bytes(std::int64_t)>> upload,std::optional<std::function<bool(strut_bytes)>> download,strut_cancellation_token cancellation){return strut_async([method,url,options,upload=std::move(upload),download=std::move(download),cancellation]() mutable{return http_request_stream(method,url,options,std::move(upload),std::move(download),cancellation);});}
#endif
)HTTP_CLIENT";
}

void emit_http_server_types(std::ostream& out, bool json, bool file_responses, bool ndjson, bool websocket) {
    if (websocket) out << "#include <array>\n";
    out << R"STRUT_HTTP_TYPES(
struct strut_http_values{std::unordered_map<strut_string,std::vector<strut_string>> entries;std::optional<strut_string> get(const strut_string& name) const{auto found=entries.find(name);if(found==entries.end()||found->second.empty())return std::nullopt;return found->second.front();}std::vector<strut_string> values(const strut_string& name) const{auto found=entries.find(name);return found==entries.end()?std::vector<strut_string>{}:found->second;}bool has(const strut_string& name) const{auto found=entries.find(name);return found!=entries.end()&&!found->second.empty();}};
inline bool strut_http_decode_component(const std::string& input,bool plus_as_space,std::string& output){output.clear();output.reserve(input.size());auto hex=[](unsigned char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;};for(std::size_t i=0;i<input.size();++i){unsigned char c=static_cast<unsigned char>(input[i]);if(c=='%'&&i+2<input.size()){const int high=hex(static_cast<unsigned char>(input[i+1])),low=hex(static_cast<unsigned char>(input[i+2]));if(high<0||low<0)return false;c=static_cast<unsigned char>((high<<4)|low);i+=2;}else if(c=='%')return false;else if(c=='+'&&plus_as_space)c=' ';if(c==0||c==0x7f||c<0x20)return false;output.push_back(static_cast<char>(c));}return true;}
inline bool strut_parse_http_values(const std::string& raw,strut_http_values& out,std::unordered_map<strut_string,strut_string>* compatibility=nullptr,std::size_t max_fields=1024){if(raw.empty())return true;std::size_t position=0,count=0;while(position<=raw.size()){if(count++>=max_fields)return false;const auto amp=raw.find('&',position);const auto part=raw.substr(position,amp==std::string::npos?std::string::npos:amp-position);const auto equal=part.find('=');const std::string raw_name=part.substr(0,equal),raw_value=equal==std::string::npos?std::string():part.substr(equal+1);std::string name,value;if(!strut_http_decode_component(raw_name,true,name)||!strut_http_decode_component(raw_value,true,value))return false;out.entries[strut_string(name)].push_back(strut_string(value));if(compatibility)(*compatibility)[strut_string(raw_name)]=strut_string(raw_value);if(amp==std::string::npos)break;position=amp+1;}return true;}
struct strut_server_request {strut_string method,path,body;std::unordered_map<strut_string,strut_string> headers,query,params;strut_http_values query_values,cookies;strut_cancellation_token cancellation;bool buffered_body_available=false;
    strut_string text() const{return text(std::numeric_limits<std::int64_t>::max());}strut_string text(std::int64_t limit) const{if(!buffered_body_available)throw strut_checked_error("HttpError","buffered request body is unavailable for streaming handlers");if(limit<0||static_cast<std::uint64_t>(limit)<body.v.size())throw strut_checked_error("HttpError","request body exceeds helper limit");return body;}
    strut_http_values form() const{return form(std::numeric_limits<std::int64_t>::max());}strut_http_values form(std::int64_t limit) const{const auto value=text(limit);auto found=headers.find(strut_string("content-type"));if(found==headers.end())throw strut_checked_error("HttpError","form request requires application/x-www-form-urlencoded");std::string type=found->second.v;while(!type.empty()&&(type.back()==' '||type.back()=='\t'))type.pop_back();std::size_t begin=0;while(begin<type.size()&&(type[begin]==' '||type[begin]=='\t'))++begin;for(char& c:type)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');if(type.substr(begin)!="application/x-www-form-urlencoded")throw strut_checked_error("HttpError","form request requires application/x-www-form-urlencoded");strut_http_values parsed;if(!strut_parse_http_values(value.v,parsed))throw strut_checked_error("HttpError","malformed or oversized URL-encoded form body");return parsed;}
)STRUT_HTTP_TYPES";
    if (json) out << "    json::Document json() const{return json(std::numeric_limits<std::int64_t>::max());} json::Document json(std::int64_t limit) const{auto value=text(limit);try{return strut_json_parse(value);}catch(const std::exception& error){throw strut_checked_error(\"HttpError\",std::string(\"invalid JSON request body: \")+error.what());}}\n";
    out << R"STRUT_HTTP_TYPES(};
struct strut_http_cookie{strut_string name,value;std::optional<strut_string> path,domain;std::optional<std::int64_t> max_age;std::optional<strut_string> expires;bool secure=false,http_only=false;std::optional<strut_string> same_site;};
inline strut_http_cookie strut_make_http_cookie(const strut_string& name,const strut_string& value){strut_http_cookie cookie;cookie.name=name;cookie.value=value;return cookie;}
struct strut_server_response {std::int32_t status=200;strut_string body;strut_string content_type="text/plain; charset=utf-8";std::unordered_map<strut_string,strut_string> headers;std::vector<strut_http_cookie> cookies;};
inline strut_server_response strut_http_text(const strut_string& s){return {200,s,"text/plain; charset=utf-8",{}, {}};}
inline strut_server_response strut_http_html(const strut_string& s){return {200,s,"text/html; charset=utf-8",{}, {}};}
inline strut_server_response strut_http_redirect(const strut_string& location,std::int32_t status=302){if(status!=301&&status!=302&&status!=303&&status!=307&&status!=308)throw strut_checked_error("HttpError","redirect status must be 301, 302, 303, 307, or 308");if(location.v.empty())throw strut_checked_error("HttpError","redirect location cannot be empty");const std::string allowed="-._~:/?#[]@!$&'()*+,;=";int brackets=0;for(std::size_t i=0;i<location.v.size();++i){const unsigned char c=static_cast<unsigned char>(location.v[i]);if(c=='%'){if(i+2>=location.v.size()||!std::isxdigit(static_cast<unsigned char>(location.v[i+1]))||!std::isxdigit(static_cast<unsigned char>(location.v[i+2])))throw strut_checked_error("HttpError","invalid redirect location");i+=2;continue;}if(!std::isalnum(c)&&allowed.find(static_cast<char>(c))==std::string::npos)throw strut_checked_error("HttpError","invalid redirect location");if(c=='['){if(brackets++)throw strut_checked_error("HttpError","invalid redirect location");}else if(c==']'){if(brackets!=1)throw strut_checked_error("HttpError","invalid redirect location");brackets=0;}}if(brackets)throw strut_checked_error("HttpError","invalid redirect location");strut_server_response response;response.status=status;response.body="";response.headers[strut_string("Location")]=location;return response;}
)STRUT_HTTP_TYPES";
    if (json) out << "inline strut_server_response strut_http_json_response(const json::Document& j){return {200,strut_string(j.dump()),\"application/json\",{}, {}};}\n";
    out << R"STRUT_HTTP_TYPES(
inline std::string strut_trim_ascii(std::string s){while(!s.empty()&&(s.back()=='\r'||s.back()==' '||s.back()=='\t'))s.pop_back();std::size_t i=0;while(i<s.size()&&(s[i]==' '||s[i]=='\t'))++i;return s.substr(i);}
enum class strut_http_version{http_1_0,http_1_1};
struct strut_http_header_field{std::string name,lower_name,value;};
enum class strut_http_request_framing{content_length,chunked};
struct strut_http_request_head{strut_server_request request;strut_http_version version=strut_http_version::http_1_1;strut_http_request_framing framing=strut_http_request_framing::content_length;std::size_t content_length=0;std::vector<strut_http_header_field> fields;bool persistent=false,upgrade_requested=false;};
struct strut_http_head_result{strut_http_request_head head;std::int32_t status=0;const char* message=nullptr;explicit operator bool() const{return status==0;}};
inline bool strut_http_token_char(unsigned char c){return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='!'||c=='#'||c=='$'||c=='%'||c=='&'||c=='\''||c=='*'||c=='+'||c=='-'||c=='.'||c=='^'||c=='_'||c=='`'||c=='|'||c=='~';}
inline bool strut_http_token(const std::string& value){return !value.empty()&&std::all_of(value.begin(),value.end(),[](unsigned char c){return strut_http_token_char(c);});}
inline std::string strut_http_lower(std::string value){for(char& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
inline bool strut_http_field_value(const std::string& value){return std::all_of(value.begin(),value.end(),[](unsigned char c){return c!='\0'&&c!='\r'&&c!='\n'&&c!=0x7f&&(c>=0x20||c=='\t');});}
inline bool strut_http_reserved_response_field(const std::string& lower){return lower=="content-type"||lower=="content-length"||lower=="transfer-encoding"||lower=="connection"||lower=="set-cookie";}
inline const char* strut_http_reason(std::int32_t status){if(status==200)return "OK";if(status==201)return "Created";if(status==204)return "No Content";if(status==206)return "Partial Content";if(status==301)return "Moved Permanently";if(status==302)return "Found";if(status==303)return "See Other";if(status==304)return "Not Modified";if(status==307)return "Temporary Redirect";if(status==308)return "Permanent Redirect";if(status==400)return "Bad Request";if(status==403)return "Forbidden";if(status==404)return "Not Found";if(status==405)return "Method Not Allowed";if(status==413)return "Payload Too Large";if(status==414)return "URI Too Long";if(status==416)return "Range Not Satisfiable";if(status==417)return "Expectation Failed";if(status==426)return "Upgrade Required";if(status==431)return "Request Header Fields Too Large";if(status==500)return "Internal Server Error";if(status==501)return "Not Implemented";if(status==503)return "Service Unavailable";if(status==505)return "HTTP Version Not Supported";return "Response";}
inline bool strut_http_media_type(const std::string& value){if(!strut_http_field_value(value))return false;std::size_t position=0;auto token=[&](){const std::size_t start=position;while(position<value.size()&&strut_http_token_char(static_cast<unsigned char>(value[position])))++position;return position>start;};auto whitespace=[&](){while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;};if(!token()||position>=value.size()||value[position++]!='/'||!token())return false;for(;;){whitespace();if(position==value.size())return true;if(value[position++]!=';')return false;whitespace();if(!token()){return false;}whitespace();if(position>=value.size()||value[position++]!='=')return false;whitespace();if(position<value.size()&&value[position]=='"'){++position;bool closed=false;while(position<value.size()){const unsigned char c=static_cast<unsigned char>(value[position++]);if(c=='"'){closed=true;break;}if(c=='\\'){if(position>=value.size())return false;++position;}else if(c<0x20&&c!='\t')return false;}if(!closed)return false;}else if(!token())return false;}})STRUT_HTTP_TYPES"; out << R"STRUT_HTTP_TYPES(
enum class strut_http_response_framing{known_length,chunked,close_delimited,no_body,head_only};
enum class strut_http_connection_header{none,close,keep_alive};
inline bool strut_http_cookie_octets(const std::string& value){return std::all_of(value.begin(),value.end(),[](unsigned char c){return c==0x21||(c>=0x23&&c<=0x2b)||(c>=0x2d&&c<=0x3a)||(c>=0x3c&&c<=0x5b)||(c>=0x5d&&c<=0x7e);});}
inline bool strut_http_cookie_domain(const std::string& value){if(value.empty())return false;std::size_t start=value.front()=='.'?1:0;if(start==value.size())return false;bool label_start=true;for(std::size_t i=start;i<value.size();++i){const unsigned char c=static_cast<unsigned char>(value[i]);if(c=='.'){if(label_start||value[i-1]=='-')return false;label_start=true;continue;}if(!((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='-')||(label_start&&c=='-'))return false;label_start=false;}return !label_start&&value.back()!='-';}
inline bool strut_http_cookie_path(const std::string& value){return !value.empty()&&value.front()=='/'&&std::all_of(value.begin(),value.end(),[](unsigned char c){return c>=0x20&&c<0x7f&&c!=';';});}
inline bool strut_http_cookie_date(const std::string& v){if(v.size()!=29||v[3]!=','||v[4]!=' '||v[7]!=' '||v[11]!=' '||v[16]!=' '||v[19]!=':'||v[22]!=':'||v.substr(25)!=" GMT")return false;const std::string weekdays="MonTueWedThuFriSatSun",months="JanFebMarAprMayJunJulAugSepOctNovDec";const auto weekday=weekdays.find(v.substr(0,3)),month=months.find(v.substr(8,3));if(weekday==std::string::npos||weekday%3||month==std::string::npos||month%3)return false;auto number=[&](std::size_t p,std::size_t n,int& out){out=0;for(std::size_t i=0;i<n;++i){const unsigned char c=static_cast<unsigned char>(v[p+i]);if(c<'0'||c>'9')return false;out=out*10+(c-'0');}return true;};int day=0,year=0,hour=0,minute=0,second=0;if(!number(5,2,day)||!number(12,4,year)||year<1601||!number(17,2,hour)||hour>23||!number(20,2,minute)||minute>59||!number(23,2,second)||second>59)return false;const int m=static_cast<int>(month/3),days=m==1?((year%4==0&&(year%100!=0||year%400==0))?29:28):(m==3||m==5||m==8||m==10?30:31);return day>=1&&day<=days;}
inline bool strut_http_serialize_cookie(const strut_http_cookie& cookie,std::string& serialized){if(!strut_http_token(cookie.name.v)||!strut_http_cookie_octets(cookie.value.v))return false;std::ostringstream out;out<<cookie.name.v<<'='<<cookie.value.v;if(cookie.path){if(!strut_http_cookie_path(cookie.path->v))return false;out<<"; Path="<<cookie.path->v;}if(cookie.domain){if(!strut_http_cookie_domain(cookie.domain->v))return false;out<<"; Domain="<<cookie.domain->v;}if(cookie.max_age)out<<"; Max-Age="<<*cookie.max_age;if(cookie.expires){if(!strut_http_cookie_date(cookie.expires->v))return false;out<<"; Expires="<<cookie.expires->v;}if(cookie.secure)out<<"; Secure";if(cookie.http_only)out<<"; HttpOnly";if(cookie.same_site){const std::string mode=strut_http_lower(cookie.same_site->v);if(mode!="strict"&&mode!="lax"&&mode!="none")return false;if(mode=="none"&&!cookie.secure)return false;out<<"; SameSite="<<(mode=="strict"?"Strict":mode=="lax"?"Lax":"None");}serialized=out.str();return serialized.size()<=4096;}
inline bool strut_serialize_http_response_head(std::int32_t status,const std::string& content_type,const std::unordered_map<strut_string,strut_string>& headers,const std::vector<strut_http_cookie>& cookies,strut_http_response_framing framing,std::size_t body_size,strut_http_version version,strut_http_connection_header connection,std::string& serialized){const bool body_forbidden=status==204||status==205||status==304;if(status<200||status>599||cookies.size()>64||!strut_http_media_type(content_type)||(body_forbidden!=(framing==strut_http_response_framing::no_body))||(version==strut_http_version::http_1_0&&framing==strut_http_response_framing::chunked))return false;std::vector<std::string> names,cookie_values;names.reserve(headers.size());cookie_values.reserve(cookies.size());for(const auto& header:headers){if(!strut_http_token(header.first.v)||!strut_http_field_value(header.second.v))return false;const std::string lower=strut_http_lower(header.first.v);if(strut_http_reserved_response_field(lower)||std::find(names.begin(),names.end(),lower)!=names.end())return false;names.push_back(lower);}for(const auto& cookie:cookies){std::string value;if(!strut_http_serialize_cookie(cookie,value))return false;cookie_values.push_back(std::move(value));}std::ostringstream out;out<<(version==strut_http_version::http_1_1?"HTTP/1.1 ":"HTTP/1.0 ")<<status<<' '<<strut_http_reason(status)<<"\r\nContent-Type: "<<content_type<<"\r\n";if(framing==strut_http_response_framing::known_length)out<<"Content-Length: "<<body_size<<"\r\n";else if(framing==strut_http_response_framing::chunked)out<<"Transfer-Encoding: chunked\r\n";else if(framing==strut_http_response_framing::no_body&&status==205)out<<"Content-Length: 0\r\n";if(connection==strut_http_connection_header::close)out<<"Connection: close\r\n";else if(connection==strut_http_connection_header::keep_alive)out<<"Connection: keep-alive\r\n";for(const auto& header:headers)out<<header.first.v<<": "<<header.second.v<<"\r\n";for(const auto& cookie:cookie_values)out<<"Set-Cookie: "<<cookie<<"\r\n";out<<"\r\n";serialized=out.str();return true;}
enum class strut_http_response_phase{uncommitted,committing,committed,finished,aborted};)STRUT_HTTP_TYPES"; out << R"STRUT_HTTP_TYPES(
struct strut_http_response_writer_state{std::mutex mutex;std::function<void(const char*,std::size_t)> send;std::function<void()> cancel;std::function<bool()> before_commit;std::function<bool()> shutdown_started;strut_http_version version=strut_http_version::http_1_1;strut_http_response_phase phase=strut_http_response_phase::uncommitted;strut_http_response_framing framing=strut_http_response_framing::close_delimited;std::int32_t status=200;std::string content_type="text/plain; charset=utf-8";std::unordered_map<strut_string,strut_string> headers;std::vector<strut_http_cookie> cookies;std::size_t declared_length=0,written=0;bool has_declared_length=false,head_started=false,active=true,request_persistent=false,close_connection=true,head_request=false;};
class strut_http_response_writer{
public:
    strut_http_response_writer()=default;explicit strut_http_response_writer(std::shared_ptr<strut_http_response_writer_state> state):state_(std::move(state)){}
    void status(std::int32_t value) const{mutate([&](auto& s){s.status=value;});}
    void header(const strut_string& name,const strut_string& value) const{mutate([&](auto& s){if(s.headers.find(name)!=s.headers.end())fail("duplicate HTTP response header");s.headers.emplace(name,value);});}
    void cookie(const strut_http_cookie& value) const{mutate([&](auto& s){std::string serialized;if(s.cookies.size()>=64||!strut_http_serialize_cookie(value,serialized))fail("invalid HTTP response cookie");s.cookies.push_back(value);});}
    void content_type(const strut_string& value) const{mutate([&](auto& s){s.content_type=value.v;});}
    void content_length(std::int64_t value) const{if(value<0||static_cast<std::uint64_t>(value)>std::numeric_limits<std::size_t>::max())fail("invalid HTTP response content length");mutate([&](auto& s){s.has_declared_length=true;s.declared_length=static_cast<std::size_t>(value);});}
    void write(const strut_string& data) const{write_native(data.v.data(),data.v.size());}
    void write_bytes(const strut_bytes& data) const{write_native(reinterpret_cast<const char*>(data.data()),data.native_size());}
    void flush() const{auto s=require();std::unique_lock<std::mutex> lock(s->mutex);require_active(*s);if(s->phase==strut_http_response_phase::finished)return;if(s->phase==strut_http_response_phase::aborted)fail("HTTP response is aborted");if(s->phase==strut_http_response_phase::committing)fail("HTTP response commitment is in progress");commit_locked(*s,lock);}
    void finish() const{auto s=require();std::unique_lock<std::mutex> lock(s->mutex);require_active(*s);if(s->phase==strut_http_response_phase::finished)return;if(s->phase==strut_http_response_phase::aborted)fail("HTTP response is aborted");if(s->phase==strut_http_response_phase::committing)fail("HTTP response commitment is in progress");if(s->has_declared_length&&s->written!=s->declared_length)fail("HTTP response body does not match declared content length");commit_locked(*s,lock);s->phase=strut_http_response_phase::finished;if(s->framing==strut_http_response_framing::chunked&&!s->head_request)send_locked(*s,"0\r\n\r\n",5);}
    bool committed() const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);return s->head_started;}
    bool reusable() const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);return s->phase==strut_http_response_phase::finished&&!s->close_connection;}
    bool account_head_body(std::size_t size) const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);require_active(*s);if(!s->head_request)return false;if(!s->has_declared_length||s->written>s->declared_length||size>s->declared_length-s->written)fail("HTTP HEAD representation length mismatch");s->written+=size;return true;}
    void prepare_stream_content_type(const char* value) const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);require_active(*s);if(s->phase==strut_http_response_phase::finished)fail("HTTP response is finished");if(s->phase==strut_http_response_phase::aborted)fail("HTTP response is aborted");if(s->phase==strut_http_response_phase::committing)fail("HTTP response commitment is in progress");if(s->phase==strut_http_response_phase::uncommitted)s->content_type=value;else if(s->content_type!=value)fail("HTTP response stream content type changed after commitment");}
    bool abort() const{if(!state_)return false;std::lock_guard<std::mutex> lock(state_->mutex);const bool committed=state_->head_started;state_->phase=strut_http_response_phase::aborted;state_->active=false;state_->send={};return committed;}
    void invalidate() const{if(!state_)return;std::lock_guard<std::mutex> lock(state_->mutex);state_->active=false;state_->send={};}
private:
    [[noreturn]] static void fail(const char* message){throw strut_checked_error("NetworkError",message);}
    std::shared_ptr<strut_http_response_writer_state> require() const{if(!state_)fail("HTTP response writer is not initialized");return state_;}
    static void require_active(const strut_http_response_writer_state& s){if(!s.active)fail("HTTP response writer is no longer active");}
    template<class F>void mutate(F change) const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);require_active(*s);if(s->phase!=strut_http_response_phase::uncommitted)fail("HTTP response metadata is already committed");change(*s);}
    static void send_locked(strut_http_response_writer_state& s,const char* data,std::size_t size){try{s.send(data,size);}catch(const strut_checked_error& error){s.phase=strut_http_response_phase::aborted;if(s.cancel)s.cancel();if(error.type=="TlsError")throw strut_checked_error("NetworkError",error.message,error.code);throw;}catch(...){s.phase=strut_http_response_phase::aborted;if(s.cancel)s.cancel();throw;}}
    static void commit_locked(strut_http_response_writer_state& s,std::unique_lock<std::mutex>& lock){if(s.phase!=strut_http_response_phase::uncommitted)return;const bool no_body=s.status==204||s.status==205||s.status==304;s.framing=no_body?strut_http_response_framing::no_body:s.head_request?(s.has_declared_length?strut_http_response_framing::known_length:strut_http_response_framing::head_only):s.has_declared_length?strut_http_response_framing::known_length:s.version==strut_http_version::http_1_1?strut_http_response_framing::chunked:strut_http_response_framing::close_delimited;bool body_terminal=true;if(s.before_commit){s.phase=strut_http_response_phase::committing;auto before=std::move(s.before_commit);lock.unlock();try{body_terminal=before();}catch(...){lock.lock();s.phase=strut_http_response_phase::aborted;throw;}lock.lock();require_active(s);}s.close_connection=!s.request_persistent||!body_terminal||s.framing==strut_http_response_framing::close_delimited||(s.shutdown_started&&s.shutdown_started());const auto connection=s.close_connection?strut_http_connection_header::close:s.version==strut_http_version::http_1_0?strut_http_connection_header::keep_alive:strut_http_connection_header::none;std::string head;if(!strut_serialize_http_response_head(s.status,s.content_type,s.headers,s.cookies,s.framing,s.declared_length,s.version,connection,head))fail("invalid HTTP response metadata");s.phase=strut_http_response_phase::committed;s.head_started=true;send_locked(s,head.data(),head.size());}
    void write_native(const char* data,std::size_t size) const{auto s=require();std::unique_lock<std::mutex> lock(s->mutex);require_active(*s);if(s->phase==strut_http_response_phase::finished)fail("HTTP response is finished");if(s->phase==strut_http_response_phase::aborted)fail("HTTP response is aborted");if(s->phase==strut_http_response_phase::committing)fail("HTTP response commitment is in progress");const bool no_body=s->status==204||s->status==205||s->status==304;if(no_body&&size!=0)fail("HTTP status forbids a response body");if(s->has_declared_length&&(s->written>s->declared_length||size>s->declared_length-s->written))fail("HTTP response exceeds declared content length");commit_locked(*s,lock);if(size==0)return;if(!s->head_request){if(s->framing==strut_http_response_framing::chunked){std::ostringstream chunk;chunk<<std::hex<<size<<"\r\n";const std::string prefix=chunk.str();send_locked(*s,prefix.data(),prefix.size());send_locked(*s,data,size);send_locked(*s,"\r\n",2);}else send_locked(*s,data,size);}s->written+=size;}
    std::shared_ptr<strut_http_response_writer_state> state_;
};
)STRUT_HTTP_TYPES";
    if (file_responses) out << R"STRUT_HTTP_FILE(
inline std::string strut_http_file_type(const std::string& path){const auto dot=path.rfind('.');std::string extension=dot==std::string::npos?std::string():strut_http_lower(path.substr(dot));if(extension==".html"||extension==".htm")return "text/html; charset=utf-8";if(extension==".css")return "text/css; charset=utf-8";if(extension==".js")return "application/javascript";if(extension==".json")return "application/json";if(extension==".txt")return "text/plain; charset=utf-8";if(extension==".svg")return "image/svg+xml";if(extension==".png")return "image/png";if(extension==".jpg"||extension==".jpeg")return "image/jpeg";if(extension==".gif")return "image/gif";if(extension==".pdf")return "application/pdf";if(extension==".wasm")return "application/wasm";return "application/octet-stream";}
inline bool strut_http_file_path(const std::string& value,std::u8string& out){out.clear();out.reserve(value.size());for(std::size_t i=0;i<value.size();){const unsigned char c=static_cast<unsigned char>(value[i]);if(c==0)return false;std::size_t count=0;if(c<0x80)count=1;else if(c>=0xc2&&c<=0xdf)count=2;else if(c>=0xe0&&c<=0xef)count=3;else if(c>=0xf0&&c<=0xf4)count=4;else return false;if(i+count>value.size())return false;for(std::size_t j=1;j<count;++j)if((static_cast<unsigned char>(value[i+j])&0xc0)!=0x80)return false;if((c==0xe0&&static_cast<unsigned char>(value[i+1])<0xa0)||(c==0xed&&static_cast<unsigned char>(value[i+1])>=0xa0)||(c==0xf0&&static_cast<unsigned char>(value[i+1])<0x90)||(c==0xf4&&static_cast<unsigned char>(value[i+1])>=0x90))return false;for(std::size_t j=0;j<count;++j)out.push_back(static_cast<char8_t>(static_cast<unsigned char>(value[i+j])));i+=count;}return true;}
inline int strut_http_file_range(const std::string& value,std::uint64_t size,std::uint64_t& first,std::uint64_t& last){const auto equal=value.find('=');if(equal==std::string::npos||strut_http_lower(value.substr(0,equal))!="bytes"||value.find(',',equal+1)!=std::string::npos)return -1;const std::string spec=value.substr(equal+1);const auto dash=spec.find('-');if(dash==std::string::npos||spec.find('-',dash+1)!=std::string::npos)return -1;auto number=[](const std::string& text,std::uint64_t& out){if(text.empty())return false;out=0;bool saturated=false;for(unsigned char c:text){if(c<'0'||c>'9')return false;if(!saturated){const auto digit=static_cast<unsigned>(c-'0');if(out>(UINT64_MAX-digit)/10){out=UINT64_MAX;saturated=true;}else out=out*10+digit;}}return true;};const std::string left=spec.substr(0,dash),right=spec.substr(dash+1);if(left.empty()){std::uint64_t suffix=0;if(!number(right,suffix))return -1;if(size==0||suffix==0)return 0;first=suffix>=size?0:size-suffix;last=size-1;return 1;}std::uint64_t start=0,end=0;if(!number(left,start))return -1;if(start>=size)return 0;if(right.empty()){first=start;last=size-1;return 1;}if(!number(right,end))return -1;if(start>end)return 0;first=start;last=end>=size?size-1:end;return 1;}
inline void strut_http_serve_file(const strut_server_request& request,const strut_http_response_writer& writer,const strut_string& path,const strut_string& content_type=strut_string()){std::u8string utf8_path;if(!strut_http_file_path(path.v,utf8_path))throw strut_checked_error("FilesystemError","HTTP response path must be valid UTF-8 without NUL");std::filesystem::path native_path;try{native_path=std::filesystem::path(utf8_path);}catch(const std::filesystem::filesystem_error&){throw strut_checked_error("FilesystemError","unable to convert HTTP response path");}std::error_code path_error;if(!std::filesystem::is_regular_file(native_path,path_error)||path_error)throw strut_checked_error("FilesystemError","HTTP response path is not a regular file");std::ifstream file(native_path,std::ios::binary|std::ios::ate);if(!file)throw strut_checked_error("FilesystemError","unable to open HTTP response file");const std::streamoff position=static_cast<std::streamoff>(file.tellg());if(position<0||static_cast<std::uint64_t>(position)>static_cast<std::uint64_t>(INT64_MAX)||static_cast<std::uint64_t>(position)>SIZE_MAX)throw strut_checked_error("FilesystemError","HTTP response file is too large");const std::uint64_t size=static_cast<std::uint64_t>(position);std::uint64_t first=0,last=size?size-1:0;writer.status(200);writer.content_type(content_type.v.empty()?strut_string(strut_http_file_type(path.v)):content_type);writer.header("Accept-Ranges","bytes");auto range=request.headers.find(strut_string("range"));if(request.method.v=="GET"&&range!=request.headers.end()){const int result=strut_http_file_range(range->second.v,size,first,last);if(result==0){writer.status(416);writer.header("Content-Range",strut_string("bytes */"+std::to_string(size)));writer.content_length(0);writer.finish();return;}if(result>0){writer.status(206);writer.header("Content-Range",strut_string("bytes "+std::to_string(first)+"-"+std::to_string(last)+"/"+std::to_string(size)));}}const std::uint64_t length=size?last-first+1:0;writer.content_length(static_cast<std::int64_t>(length));if(writer.account_head_body(static_cast<std::size_t>(length))){writer.finish();return;}if(length){file.seekg(static_cast<std::streamoff>(first),std::ios::beg);if(!file)throw strut_checked_error("FilesystemError","unable to seek HTTP response file");std::uint64_t remaining=length;while(remaining){if(request.cancellation.cancelled())throw strut_checked_error("NetworkError","HTTP file response cancelled");const auto amount=static_cast<std::size_t>(std::min<std::uint64_t>(remaining,65536));strut_bytes chunk(static_cast<std::int64_t>(amount));file.read(reinterpret_cast<char*>(chunk.data()),static_cast<std::streamsize>(amount));if(file.gcount()!=static_cast<std::streamsize>(amount))throw strut_checked_error("FilesystemError","unable to read HTTP response file");writer.write_bytes(chunk);remaining-=amount;}}writer.finish();}
inline void strut_http_serve_file_cancellable(const strut_server_request& request,const strut_http_response_writer& writer,const strut_string& path,const strut_string& content_type=strut_string()){if(request.cancellation.cancelled())throw strut_checked_error("NetworkError","HTTP file response cancelled");strut_http_serve_file(request,writer,path,content_type);}
)STRUT_HTTP_FILE";
    if (ndjson) out << R"STRUT_NDJSON(
inline void strut_http_write_ndjson(const strut_server_request& request,const strut_http_response_writer& writer,const json::Document& value){if(request.cancellation.cancelled())throw strut_checked_error("NetworkError","NDJSON response cancelled");if(!value.is_valid())throw strut_checked_error("HttpError","NDJSON value is not valid JSON");std::string record=value.dump_compact();record.push_back('\n');if(request.cancellation.cancelled())throw strut_checked_error("NetworkError","NDJSON response cancelled");writer.prepare_stream_content_type("application/x-ndjson");writer.write(strut_string(std::move(record)));writer.flush();}
)STRUT_NDJSON";
    out << R"STRUT_HTTP_TYPES(
inline bool strut_http_uri_char(unsigned char c,bool query){if((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z'))return true;const std::string allowed=query?"-._~!$&'()*+,;=:@/?":"-._~!$&'()*+,;=:@/";return allowed.find(static_cast<char>(c))!=std::string::npos;}
inline bool strut_http_target(const std::string& target){if(target.empty()||target.front()!='/')return false;auto hex=[](unsigned char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;};bool query=false;for(std::size_t i=0;i<target.size();++i){const unsigned char c=static_cast<unsigned char>(target[i]);if(c=='?'){query=true;continue;}if(c=='%'){if(i+2>=target.size())return false;const int high=hex(static_cast<unsigned char>(target[i+1])),low=hex(static_cast<unsigned char>(target[i+2]));if(high<0||low<0)return false;const unsigned decoded=static_cast<unsigned>((high<<4)|low);if(decoded<0x20||decoded==0x7f)return false;i+=2;continue;}if(!strut_http_uri_char(c,query))return false;}return true;}
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
inline bool strut_http_connection_tokens(const std::string& value,bool& close,bool& keep_alive,bool& upgrade){std::size_t position=0;for(;;){while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;const std::size_t start=position;while(position<value.size()&&strut_http_token_char(static_cast<unsigned char>(value[position])))++position;if(start==position)return false;const std::string token=strut_http_lower(value.substr(start,position-start));if(token=="close")close=true;else if(token=="keep-alive")keep_alive=true;else if(token=="upgrade")upgrade=true;while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;if(position==value.size())return true;if(value[position++]!=',')return false;}}
inline bool strut_parse_cookie_header(const std::string& header,strut_http_values& out,std::size_t max_pairs){if(header.empty())return false;std::size_t position=0,count=0;while(position<header.size()){if(count++>=max_pairs)return false;if(position!=0){if(header[position]!=';')return false;++position;while(position<header.size()&&(header[position]==' '||header[position]=='\t'))++position;if(position==header.size())return false;}const auto semicolon=header.find(';',position);const std::string pair=header.substr(position,semicolon==std::string::npos?std::string::npos:semicolon-position);const auto equal=pair.find('=');if(equal==std::string::npos)return false;const std::string name=pair.substr(0,equal);std::string value=pair.substr(equal+1);if(!strut_http_token(name))return false;if(value.size()>=2&&value.front()=='"'&&value.back()=='"')value=value.substr(1,value.size()-2);else if(value.find('"')!=std::string::npos)return false;if(!strut_http_cookie_octets(value))return false;out.entries[strut_string(name)].push_back(strut_string(value));if(semicolon==std::string::npos)break;position=semicolon;}return true;})STRUT_HTTP_TYPES"; out << R"STRUT_HTTP_TYPES(
inline strut_http_head_result strut_parse_http_request_head(const std::string& bytes,std::size_t max_body_bytes,std::int32_t max_header_count){
    strut_http_head_result result;auto fail=[&](std::int32_t status,const char* message){result.status=status;result.message=message;return result;};
    const auto line_end=bytes.find("\r\n");if(line_end==std::string::npos)return fail(400,"Bad Request");const std::string line=bytes.substr(0,line_end);const auto first=line.find(' '),second=first==std::string::npos?std::string::npos:line.find(' ',first+1);if(first==std::string::npos||second==std::string::npos||line.find(' ',second+1)!=std::string::npos||first==0||second==first+1||second+1==line.size())return fail(400,"Bad Request");
    const std::string method=line.substr(0,first),target=line.substr(first+1,second-first-1),version=line.substr(second+1);if(!strut_http_token(method))return fail(400,"Bad Request");result.head.request.method=strut_string(method);if(version=="HTTP/1.1")result.head.version=strut_http_version::http_1_1;else if(version=="HTTP/1.0")result.head.version=strut_http_version::http_1_0;else if(version.size()==8&&version.rfind("HTTP/",0)==0&&std::isdigit(static_cast<unsigned char>(version[5]))&&version[6]=='.'&&std::isdigit(static_cast<unsigned char>(version[7])))return fail(505,"HTTP Version Not Supported");else return fail(400,"Bad Request");if(line_end>8192)return fail(414,"URI Too Long");if(!strut_http_target(target))return fail(400,"Bad Request");
    const auto query=target.find('?');result.head.request.path=strut_string(target.substr(0,query));if(query!=std::string::npos&&!strut_parse_http_values(target.substr(query+1),result.head.request.query_values,&result.head.request.query,static_cast<std::size_t>(max_header_count)))return fail(400,"Bad Request");
    std::size_t position=line_end+2;std::int32_t count=0,host_count=0,content_length_count=0,transfer_encoding_count=0;bool transfer_encoding_valid=true,body_too_large=false,connection_close=false,connection_keep_alive=false,connection_upgrade=false,has_upgrade=false;
    while(position<bytes.size()){
        const auto end=bytes.find("\r\n",position);if(end==std::string::npos)return fail(400,"Bad Request");if(end-position>8192)return fail(431,"Request Header Fields Too Large");const std::string header=bytes.substr(position,end-position);if(header.empty()||header.front()==' '||header.front()=='\t')return fail(400,"Bad Request");if(count>=max_header_count)return fail(431,"Request Header Fields Too Large");++count;const auto colon=header.find(':');if(colon==std::string::npos||colon==0||!strut_http_token(header.substr(0,colon)))return fail(400,"Bad Request");const std::string name=header.substr(0,colon),lower=strut_http_lower(name),value=strut_trim_ascii(header.substr(colon+1));for(unsigned char c:value)if(c==0x7f||(c<0x20&&c!='\t'))return fail(400,"Bad Request");if(std::any_of(result.head.fields.begin(),result.head.fields.end(),[&](const auto& field){return field.lower_name==lower;}))return fail(400,"Bad Request");result.head.fields.push_back({name,lower,value});result.head.request.headers[strut_string(lower)]=strut_string(value);
        if(lower=="host"){++host_count;if(!strut_http_host(value))return fail(400,"Bad Request");}
        else if(lower=="content-length"){
            if(++content_length_count>1||value.empty())return fail(400,"Bad Request");std::size_t length=0;for(unsigned char c:value){if(c<'0'||c>'9')return fail(400,"Bad Request");const std::size_t digit=static_cast<std::size_t>(c-'0');if(body_too_large||length>max_body_bytes/10||(length==max_body_bytes/10&&digit>max_body_bytes%10)){body_too_large=true;continue;}length=length*10+digit;}result.head.content_length=length;
        }else if(lower=="transfer-encoding"){
            if(++transfer_encoding_count>1||value.empty())return fail(400,"Bad Request");transfer_encoding_valid=strut_http_transfer_encoding(value);
        }else if(lower=="connection"){if(!strut_http_connection_tokens(value,connection_close,connection_keep_alive,connection_upgrade))return fail(400,"Bad Request");}
        else if(lower=="upgrade"){if(value.empty()||!strut_http_field_value(value))return fail(400,"Bad Request");has_upgrade=true;}
        else if(lower=="expect")return fail(417,"Expectation Failed");
        position=end+2;
    }
    if((result.head.version==strut_http_version::http_1_1&&host_count!=1)||(result.head.version==strut_http_version::http_1_0&&host_count>1))return fail(400,"Bad Request");if(transfer_encoding_count&&content_length_count)return fail(400,"Bad Request");if(transfer_encoding_count){if(!transfer_encoding_valid||result.head.version==strut_http_version::http_1_0)return fail(400,"Bad Request");if(strut_http_lower(strut_trim_ascii(result.head.request.headers[strut_string("transfer-encoding")].v))!="chunked")return fail(501,"Not Implemented");result.head.framing=strut_http_request_framing::chunked;}auto cookie=result.head.request.headers.find(strut_string("cookie"));if(cookie!=result.head.request.headers.end()&&!strut_parse_cookie_header(cookie->second.v,result.head.request.cookies,static_cast<std::size_t>(max_header_count)))return fail(400,"Bad Request");if(body_too_large)return fail(413,"Payload Too Large");result.head.persistent=!connection_close&&(result.head.version==strut_http_version::http_1_1||connection_keep_alive);result.head.upgrade_requested=connection_upgrade&&has_upgrade;return result;
})STRUT_HTTP_TYPES";
    if (websocket) out << R"STRUT_WEBSOCKET(
struct strut_websocket_message{strut_string kind;std::optional<strut_string> text;std::optional<strut_bytes> data;};
enum class strut_websocket_commitment{pending,committing,committed,failed};
struct strut_websocket_state:std::enable_shared_from_this<strut_websocket_state>{std::mutex mutex,read_mutex,write_mutex,io_mutex;std::condition_variable idle_cv;std::string buffered,selected_protocol,message;std::vector<std::string> offered_protocols;std::function<std::string(std::size_t)> read;std::function<void(const char*,std::size_t)> send;std::function<void()> interrupt;std::function<void(const std::string&)> commit;std::optional<strut_websocket_message> pending_message;strut_websocket_commitment commitment=strut_websocket_commitment::pending;std::size_t max_frame=1024*1024,max_message=4*1024*1024,operations=0;std::uint32_t utf8_codepoint=0,utf8_minimum=0;unsigned utf8_remaining=0;unsigned char fragmented_opcode=0;bool active=true,close_sent=false,peer_closed=false,protocol_failed=false;};
class strut_websocket{
public:
    strut_websocket()=default;explicit strut_websocket(std::shared_ptr<strut_websocket_state> state):state_(std::move(state)){}
    void accept(const strut_string& protocol=strut_string()) const{auto state=require();operation active(state);std::function<void(const std::string&)> commit;{std::lock_guard<std::mutex> lock(state->mutex);if(state->commitment==strut_websocket_commitment::failed)network_fail("WebSocket acceptance previously failed");if(state->commitment==strut_websocket_commitment::committing)network_fail("WebSocket acceptance is already in progress");if(state->commitment==strut_websocket_commitment::committed){if(state->selected_protocol!=protocol.v)network_fail("WebSocket was already accepted with a different subprotocol");return;}if(!protocol.v.empty()&&std::find(state->offered_protocols.begin(),state->offered_protocols.end(),protocol.v)==state->offered_protocols.end())network_fail("WebSocket subprotocol was not offered");state->selected_protocol=protocol.v;state->commitment=strut_websocket_commitment::committing;commit=state->commit;}try{std::lock_guard<std::mutex> io(state->io_mutex);{std::lock_guard<std::mutex> lock(state->mutex);if(!state->active)network_fail("WebSocket handle is no longer active");}commit(protocol.v);std::lock_guard<std::mutex> lock(state->mutex);state->commitment=strut_websocket_commitment::committed;}catch(const strut_checked_error& error){{std::lock_guard<std::mutex> lock(state->mutex);state->commitment=strut_websocket_commitment::failed;}throw_network(error);}catch(...){{std::lock_guard<std::mutex> lock(state->mutex);state->commitment=strut_websocket_commitment::failed;}throw;}}
    std::optional<strut_websocket_message> read() const{return read_kind(0);}
    std::optional<strut_string> read_text() const{auto message=read_kind(1);if(!message)return std::nullopt;return std::move(message->text);}
    std::optional<strut_bytes> read_bytes() const{auto message=read_kind(2);if(!message)return std::nullopt;return std::move(message->data);}
    void write_text(const strut_string& text) const{auto s=require();if(!valid_utf8(text.v))websocket_fail("WebSocket text is not valid UTF-8");write_data(*s,1,text.v);}
    void write_bytes(const strut_bytes& data) const{auto s=require();std::string payload;if(!data.empty())payload.assign(reinterpret_cast<const char*>(data.data()),data.native_size());write_data(*s,2,payload);}
    void ping() const{ping(strut_bytes());}
    void ping(const strut_bytes& payload) const{if(payload.native_size()>125)websocket_fail("WebSocket ping payload exceeds 125 bytes");std::string data;if(!payload.empty())data.assign(reinterpret_cast<const char*>(payload.data()),payload.native_size());auto s=require();write_frame(*s,9,data,false);}
    void close() const{close(1000,strut_string());}
    void close(std::int32_t code) const{close(code,strut_string());}
    void close(std::int32_t code,const strut_string& reason) const{if(code==1010||!valid_close_code(code))websocket_fail("invalid server WebSocket close code");if(!valid_utf8(reason.v))websocket_fail("WebSocket close reason is not valid UTF-8");if(reason.v.size()>123)websocket_fail("WebSocket close reason exceeds 123 bytes");std::string payload;payload.push_back(static_cast<char>((code>>8)&255));payload.push_back(static_cast<char>(code&255));payload+=reason.v;auto s=require();write_frame(*s,8,payload,true);}
    void finish(std::int32_t code=1000) const noexcept{try{auto s=require();operation finishing(s);std::unique_lock<std::mutex> parser(s->read_mutex,std::try_to_lock);if(!parser.owns_lock())return;bool send_close=false;{std::lock_guard<std::mutex> lock(s->mutex);if(!s->active||s->commitment!=strut_websocket_commitment::committed||s->peer_closed)return;send_close=!s->close_sent;}if(send_close)close(code);std::function<void()> interrupt;{std::lock_guard<std::mutex> lock(s->mutex);interrupt=s->interrupt;}close_timer deadline(std::move(interrupt));for(;;)if(!next_message(*s))break;}catch(...){}}
    bool invalidate(bool* failed=nullptr) const{if(!state_){if(failed)*failed=false;return false;}std::function<void()> interrupt;bool committed=false;{std::lock_guard<std::mutex> lock(state_->mutex);committed=state_->commitment==strut_websocket_commitment::committed;if(failed)*failed=state_->commitment==strut_websocket_commitment::failed||state_->commitment==strut_websocket_commitment::committing;state_->active=false;if(state_->operations!=0||state_->commitment!=strut_websocket_commitment::pending)interrupt=state_->interrupt;}if(interrupt)interrupt();std::unique_lock<std::mutex> lock(state_->mutex);state_->idle_cv.wait(lock,[&]{return state_->operations==0;});state_->buffered.clear();state_->message.clear();state_->pending_message.reset();state_->offered_protocols.clear();state_->read={};state_->send={};state_->interrupt={};state_->commit={};return committed;}
private:std::shared_ptr<strut_websocket_state> state_;
    class close_timer{public:explicit close_timer(std::function<void()> interrupt):thread_([this,interrupt=std::move(interrupt)]{std::unique_lock<std::mutex> lock(mutex_);if(!cv_.wait_for(lock,std::chrono::seconds(1),[&]{return done_;})){lock.unlock();if(interrupt)interrupt();}}){}~close_timer(){{std::lock_guard<std::mutex> lock(mutex_);done_=true;}cv_.notify_one();if(thread_.joinable())thread_.join();}close_timer(const close_timer&)=delete;close_timer& operator=(const close_timer&)=delete;private:std::mutex mutex_;std::condition_variable cv_;bool done_=false;std::thread thread_;};
    class operation{public:explicit operation(const std::shared_ptr<strut_websocket_state>& state):state_(state){std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->active)network_fail("WebSocket handle is no longer active");++state_->operations;}~operation(){std::lock_guard<std::mutex> lock(state_->mutex);if(state_->operations>0)--state_->operations;if(state_->operations==0)state_->idle_cv.notify_all();}operation(const operation&)=delete;operation& operator=(const operation&)=delete;private:std::shared_ptr<strut_websocket_state> state_;};
    [[noreturn]] static void network_fail(const char* message){throw strut_checked_error("NetworkError",message);}
    [[noreturn]] static void websocket_fail(const char* message){throw strut_checked_error("WebSocketError",message);}
    [[noreturn]] static void throw_network(const strut_checked_error& error){if(error.type=="NetworkError")throw;throw strut_checked_error("NetworkError",error.message,error.code);}
    std::shared_ptr<strut_websocket_state> require() const{if(!state_)network_fail("WebSocket handle is not initialized");return state_;}
    static bool valid_close_code(std::int32_t code){return (code>=1000&&code<=1014&&code!=1004&&code!=1005&&code!=1006)||(code>=3000&&code<=4999);}
    static bool feed_utf8(std::uint32_t& point,std::uint32_t& minimum,unsigned& remaining,unsigned char byte){if(!remaining){if(byte<=0x7f)return true;if(byte>=0xc2&&byte<=0xdf){point=byte&0x1f;minimum=0x80;remaining=1;return true;}if(byte>=0xe0&&byte<=0xef){point=byte&0x0f;minimum=0x800;remaining=2;return true;}if(byte>=0xf0&&byte<=0xf4){point=byte&7;minimum=0x10000;remaining=3;return true;}return false;}if((byte&0xc0)!=0x80)return false;point=(point<<6)|(byte&0x3f);if(--remaining==0&&(point<minimum||point>0x10ffff||(point>=0xd800&&point<=0xdfff)))return false;return true;}
    static bool valid_utf8(const std::string& value){std::uint32_t point=0,minimum=0;unsigned remaining=0;for(unsigned char byte:value)if(!feed_utf8(point,minimum,remaining,byte))return false;return remaining==0;}
    static std::string transport_read(strut_websocket_state& s,std::size_t size){try{std::lock_guard<std::mutex> io(s.io_mutex);std::function<std::string(std::size_t)> read;{std::lock_guard<std::mutex> lock(s.mutex);if(!s.active)network_fail("WebSocket handle is no longer active");read=s.read;}auto value=read(std::min<std::size_t>(size,8192));if(value.empty())network_fail("WebSocket transport closed without a close frame");return value;}catch(const strut_checked_error& error){throw_network(error);}}
    static std::string read_exact(strut_websocket_state& s,std::size_t size){std::string out;out.reserve(size);while(out.size()<size){std::string buffered;{std::lock_guard<std::mutex> lock(s.mutex);const auto count=std::min(size-out.size(),s.buffered.size());buffered.assign(s.buffered,0,count);s.buffered.erase(0,count);}out+=buffered;if(out.size()<size){auto chunk=transport_read(s,size-out.size());std::lock_guard<std::mutex> lock(s.mutex);s.buffered+=chunk;}}return out;}
    static void transport_send(strut_websocket_state& s,const std::string& frame){try{std::lock_guard<std::mutex> io(s.io_mutex);std::function<void(const char*,std::size_t)> send;{std::lock_guard<std::mutex> lock(s.mutex);if(!s.active)network_fail("WebSocket handle is no longer active");send=s.send;}send(frame.data(),frame.size());}catch(const strut_checked_error& error){throw_network(error);}}
    static std::string frame(unsigned char opcode,const std::string& payload){if(opcode==8&&payload.size()>=2&&static_cast<unsigned char>(payload[0])==3&&static_cast<unsigned char>(payload[1])==242)return frame(8,std::string());std::string out;out.reserve(payload.size()+10);out.push_back(static_cast<char>(0x80|opcode));if(payload.size()<=125)out.push_back(static_cast<char>(payload.size()));else if(payload.size()<=65535){out.push_back(126);out.push_back(static_cast<char>((payload.size()>>8)&255));out.push_back(static_cast<char>(payload.size()&255));}else{out.push_back(127);const auto size=static_cast<std::uint64_t>(payload.size());for(int shift=56;shift>=0;shift-=8)out.push_back(static_cast<char>((size>>shift)&255));}out+=payload;return out;}
    static void write_frame(strut_websocket_state& s,unsigned char opcode,const std::string& payload,bool closing){operation active(s.shared_from_this());std::lock_guard<std::mutex> write(s.write_mutex);{std::lock_guard<std::mutex> lock(s.mutex);if(!s.active)network_fail("WebSocket handle is no longer active");if(s.commitment!=strut_websocket_commitment::committed)websocket_fail("WebSocket has not been accepted");if(s.protocol_failed)websocket_fail("WebSocket failed after a protocol violation");if(closing&&(s.close_sent||s.peer_closed))return;if(s.peer_closed)websocket_fail("WebSocket peer has closed");if(s.close_sent)websocket_fail("WebSocket close has started");if(closing)s.close_sent=true;}transport_send(s,frame(opcode,payload));}
    static void write_data(strut_websocket_state& s,unsigned char opcode,const std::string& payload){{std::lock_guard<std::mutex> lock(s.mutex);if(payload.size()>s.max_message)websocket_fail("WebSocket message exceeds configured limit");if(payload.size()>s.max_frame)websocket_fail("WebSocket data frame exceeds configured limit");}write_frame(s,opcode,payload,false);}
    [[noreturn]] static void protocol_fail(strut_websocket_state& s,std::int32_t code,const char* message){std::lock_guard<std::mutex> write(s.write_mutex);bool send_close=false;{std::lock_guard<std::mutex> lock(s.mutex);s.protocol_failed=true;if(s.active&&s.commitment==strut_websocket_commitment::committed&&!s.close_sent&&!s.peer_closed){s.close_sent=true;send_close=true;}}if(send_close){std::string payload{static_cast<char>((code>>8)&255),static_cast<char>(code&255)};try{transport_send(s,frame(8,payload));}catch(...){}}websocket_fail(message);}
    static void pong(strut_websocket_state& s,const std::string& payload){std::lock_guard<std::mutex> write(s.write_mutex);{std::lock_guard<std::mutex> lock(s.mutex);if(!s.active||s.peer_closed||s.protocol_failed)return;}transport_send(s,frame(10,payload));}
)STRUT_WEBSOCKET";
    if (websocket) out << R"STRUT_WEBSOCKET(
    static std::optional<strut_websocket_message> next_message(strut_websocket_state& s){for(;;){const auto head=read_exact(s,2);const unsigned char first=static_cast<unsigned char>(head[0]),second=static_cast<unsigned char>(head[1]);const bool fin=(first&0x80)!=0,masked=(second&0x80)!=0;const unsigned char opcode=first&15,length_code=second&127;if((first&0x70)!=0)protocol_fail(s,1002,"WebSocket RSV bits require an unsupported extension");if(!masked)protocol_fail(s,1002,"client WebSocket frames must be masked");if(opcode!=0&&opcode!=1&&opcode!=2&&opcode!=8&&opcode!=9&&opcode!=10)protocol_fail(s,1002,"invalid WebSocket opcode");const bool control=(opcode&8)!=0;if(control&&!fin)protocol_fail(s,1002,"WebSocket control frames cannot be fragmented");std::uint64_t length=length_code;if(length_code==126){const auto extended=read_exact(s,2);length=(static_cast<std::uint64_t>(static_cast<unsigned char>(extended[0]))<<8)|static_cast<unsigned char>(extended[1]);if(length<126)protocol_fail(s,1002,"non-canonical WebSocket frame length");}else if(length_code==127){const auto extended=read_exact(s,8);if((static_cast<unsigned char>(extended[0])&0x80)!=0)protocol_fail(s,1002,"WebSocket frame length has its high bit set");length=0;for(unsigned char byte:extended)length=(length<<8)|byte;if(length<=65535)protocol_fail(s,1002,"non-canonical WebSocket frame length");}if(control&&length>125)protocol_fail(s,1002,"WebSocket control payload exceeds 125 bytes");std::size_t frame_limit=0;{std::lock_guard<std::mutex> lock(s.mutex);frame_limit=s.max_frame;}if(length>frame_limit||length>std::numeric_limits<std::size_t>::max())protocol_fail(s,1009,"WebSocket frame exceeds configured limit");const auto mask=read_exact(s,4);auto payload=read_exact(s,static_cast<std::size_t>(length));for(std::size_t i=0;i<payload.size();++i)payload[i]=static_cast<char>(static_cast<unsigned char>(payload[i])^static_cast<unsigned char>(mask[i%4]));if(control){if(opcode==9){pong(s,payload);continue;}if(opcode==10)continue;if(payload.size()==1)protocol_fail(s,1002,"WebSocket close payload cannot contain one byte");if(payload.size()>=2){const auto code=(static_cast<unsigned char>(payload[0])<<8)|static_cast<unsigned char>(payload[1]);if(!valid_close_code(code))protocol_fail(s,1002,"invalid WebSocket close code");if(!valid_utf8(payload.substr(2)))protocol_fail(s,1007,"WebSocket close reason is not valid UTF-8");}bool respond=false;{std::lock_guard<std::mutex> lock(s.mutex);s.peer_closed=true;s.message.clear();s.fragmented_opcode=0;s.utf8_remaining=0;if(!s.close_sent){s.close_sent=true;respond=true;}}if(respond){std::lock_guard<std::mutex> write(s.write_mutex);try{transport_send(s,frame(8,payload));}catch(...){throw;}}return std::nullopt;}if(opcode==0){if(s.fragmented_opcode==0)protocol_fail(s,1002,"WebSocket continuation has no fragmented message");}else{if(s.fragmented_opcode!=0)protocol_fail(s,1002,"WebSocket data frame started during a fragmented message");s.fragmented_opcode=opcode;s.message.clear();s.utf8_codepoint=0;s.utf8_minimum=0;s.utf8_remaining=0;}if(payload.size()>s.max_message-s.message.size())protocol_fail(s,1009,"WebSocket message exceeds configured limit");if(s.fragmented_opcode==1)for(unsigned char byte:payload)if(!feed_utf8(s.utf8_codepoint,s.utf8_minimum,s.utf8_remaining,byte))protocol_fail(s,1007,"WebSocket text is not valid UTF-8");s.message+=payload;if(!fin)continue;if(s.fragmented_opcode==1&&s.utf8_remaining!=0)protocol_fail(s,1007,"WebSocket text ends with incomplete UTF-8");strut_websocket_message message;const auto kind=s.fragmented_opcode;s.fragmented_opcode=0;if(kind==1){message.kind="text";message.text=strut_string(std::move(s.message));}else{message.kind="binary";strut_bytes data;data.append_native(s.message.data(),s.message.size());message.data=std::move(data);s.message.clear();}return message;}}
    std::optional<strut_websocket_message> read_kind(unsigned expected) const{auto s=require();operation active(s);std::unique_lock<std::mutex> read_lock(s->read_mutex,std::try_to_lock);if(!read_lock.owns_lock())websocket_fail("concurrent WebSocket reads are not supported");{std::lock_guard<std::mutex> lock(s->mutex);if(!s->active)network_fail("WebSocket handle is no longer active");if(s->commitment!=strut_websocket_commitment::committed)websocket_fail("WebSocket has not been accepted");if(s->protocol_failed)websocket_fail("WebSocket failed after a protocol violation");if(s->peer_closed)return std::nullopt;if(s->pending_message){auto message=std::move(s->pending_message);s->pending_message.reset();if(expected&&((expected==1)!=(message->kind.v=="text"))){s->pending_message=std::move(message);websocket_fail(expected==1?"next WebSocket message is binary":"next WebSocket message is text");}return message;}}auto message=next_message(*s);if(message&&expected&&((expected==1)!=(message->kind.v=="text"))){std::lock_guard<std::mutex> lock(s->mutex);s->pending_message=std::move(message);websocket_fail(expected==1?"next WebSocket message is binary":"next WebSocket message is text");}return message;}
};
inline std::uint32_t strut_websocket_rotate_left(std::uint32_t value,unsigned count){return (value<<count)|(value>>(32-count));}
inline std::array<unsigned char,20> strut_websocket_sha1(const std::string& input){std::string message=input;const std::uint64_t bits=static_cast<std::uint64_t>(message.size())*8;message.push_back(static_cast<char>(0x80));while(message.size()%64!=56)message.push_back('\0');for(int shift=56;shift>=0;shift-=8)message.push_back(static_cast<char>((bits>>shift)&0xff));std::uint32_t h0=0x67452301u,h1=0xefcdab89u,h2=0x98badcfeu,h3=0x10325476u,h4=0xc3d2e1f0u;for(std::size_t offset=0;offset<message.size();offset+=64){std::uint32_t words[80]{};for(unsigned i=0;i<16;++i){const auto p=offset+i*4;words[i]=(static_cast<std::uint32_t>(static_cast<unsigned char>(message[p]))<<24)|(static_cast<std::uint32_t>(static_cast<unsigned char>(message[p+1]))<<16)|(static_cast<std::uint32_t>(static_cast<unsigned char>(message[p+2]))<<8)|static_cast<std::uint32_t>(static_cast<unsigned char>(message[p+3]));}for(unsigned i=16;i<80;++i)words[i]=strut_websocket_rotate_left(words[i-3]^words[i-8]^words[i-14]^words[i-16],1);std::uint32_t a=h0,b=h1,c=h2,d=h3,e=h4;for(unsigned i=0;i<80;++i){std::uint32_t f=0,k=0;if(i<20){f=(b&c)|((~b)&d);k=0x5a827999u;}else if(i<40){f=b^c^d;k=0x6ed9eba1u;}else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdcu;}else{f=b^c^d;k=0xca62c1d6u;}const auto next=strut_websocket_rotate_left(a,5)+f+e+k+words[i];e=d;d=c;c=strut_websocket_rotate_left(b,30);b=a;a=next;}h0+=a;h1+=b;h2+=c;h3+=d;h4+=e;}std::array<unsigned char,20> digest{};const std::uint32_t hashes[5]={h0,h1,h2,h3,h4};for(unsigned i=0;i<5;++i)for(unsigned j=0;j<4;++j)digest[i*4+j]=static_cast<unsigned char>(hashes[i]>>(24-j*8));return digest;}
inline std::string strut_websocket_base64(const unsigned char* data,std::size_t size){static constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;out.reserve(((size+2)/3)*4);for(std::size_t i=0;i<size;i+=3){const std::uint32_t value=(static_cast<std::uint32_t>(data[i])<<16)|(i+1<size?static_cast<std::uint32_t>(data[i+1])<<8:0)|(i+2<size?data[i+2]:0);out.push_back(alphabet[(value>>18)&63]);out.push_back(alphabet[(value>>12)&63]);out.push_back(i+1<size?alphabet[(value>>6)&63]:'=');out.push_back(i+2<size?alphabet[value&63]:'=');}return out;}
inline bool strut_websocket_key(const std::string& key){if(key.size()!=24||key[22]!='='||key[23]!='=')return false;auto decode=[](unsigned char c)->int{if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;};std::array<unsigned char,16> bytes{};std::size_t output=0;for(std::size_t i=0;i<20;i+=4){const int a=decode(static_cast<unsigned char>(key[i])),b=decode(static_cast<unsigned char>(key[i+1])),c=decode(static_cast<unsigned char>(key[i+2])),d=decode(static_cast<unsigned char>(key[i+3]));if(a<0||b<0||c<0||d<0)return false;bytes[output++]=static_cast<unsigned char>((a<<2)|(b>>4));bytes[output++]=static_cast<unsigned char>((b<<4)|(c>>2));bytes[output++]=static_cast<unsigned char>((c<<6)|d);}const int a=decode(static_cast<unsigned char>(key[20])),b=decode(static_cast<unsigned char>(key[21]));if(a<0||b<0||(b&15)!=0)return false;bytes[output++]=static_cast<unsigned char>((a<<2)|(b>>4));return output==16&&strut_websocket_base64(bytes.data(),bytes.size())==key;}
inline bool strut_websocket_sha1_self_test(){static const std::array<unsigned char,20> expected={0xa9,0x99,0x3e,0x36,0x47,0x06,0x81,0x6a,0xba,0x3e,0x25,0x71,0x78,0x50,0xc2,0x6c,0x9c,0xd0,0xd8,0x9d};return strut_websocket_sha1("abc")==expected;}
inline std::string strut_websocket_accept(const std::string& key){const auto digest=strut_websocket_sha1(key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11");return strut_websocket_base64(digest.data(),digest.size());}
inline bool strut_websocket_token_list(const std::string& value,std::vector<std::string>& tokens){std::size_t position=0;for(;;){while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;const std::size_t start=position;while(position<value.size()&&strut_http_token_char(static_cast<unsigned char>(value[position])))++position;if(start==position)return false;tokens.push_back(value.substr(start,position-start));while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;if(position==value.size())return true;if(value[position++]!=',')return false;}}
inline bool strut_websocket_upgrade_products(const std::string& value,bool& has_websocket){std::size_t position=0;for(;;){while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;const std::size_t name=position;while(position<value.size()&&strut_http_token_char(static_cast<unsigned char>(value[position])))++position;if(name==position)return false;const std::string protocol=value.substr(name,position-name);bool versioned=false;if(position<value.size()&&value[position]=='/'){versioned=true;++position;const std::size_t version=position;while(position<value.size()&&strut_http_token_char(static_cast<unsigned char>(value[position])))++position;if(version==position)return false;}if(!versioned&&strut_http_lower(protocol)=="websocket")has_websocket=true;while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;if(position==value.size())return true;if(value[position++]!=',')return false;}}
inline bool strut_websocket_extensions(const std::string& value){std::size_t position=0;auto whitespace=[&]{while(position<value.size()&&(value[position]==' '||value[position]=='\t'))++position;};auto token=[&]{const std::size_t start=position;while(position<value.size()&&strut_http_token_char(static_cast<unsigned char>(value[position])))++position;return start!=position;};for(;;){whitespace();if(!token())return false;for(;;){whitespace();if(position==value.size())return true;if(value[position]==','){++position;break;}if(value[position++]!=';')return false;whitespace();if(!token())return false;whitespace();if(position<value.size()&&value[position]=='='){++position;whitespace();if(position<value.size()&&value[position]=='\"'){++position;bool closed=false;std::string decoded;while(position<value.size()){const unsigned char c=static_cast<unsigned char>(value[position++]);if(c=='\"'){closed=true;break;}if(c=='\\'){if(position>=value.size())return false;const unsigned char escaped=static_cast<unsigned char>(value[position++]);if((escaped<0x20&&escaped!='\t')||escaped==0x7f)return false;decoded.push_back(static_cast<char>(escaped));}else{if((c<0x20&&c!='\t')||c==0x7f)return false;decoded.push_back(static_cast<char>(c));}}if(!closed||!strut_http_token(decoded))return false;}else if(!token())return false;}}}}
enum class strut_websocket_opening_result{valid,invalid,unsupported_version};
inline strut_websocket_opening_result strut_validate_websocket_opening(const strut_http_request_head& head,std::string& accept,std::vector<std::string>& protocols){if(head.request.method.v!="GET"||head.version!=strut_http_version::http_1_1)return strut_websocket_opening_result::invalid;const auto connection=head.request.headers.find(strut_string("connection")),upgrade=head.request.headers.find(strut_string("upgrade")),version=head.request.headers.find(strut_string("sec-websocket-version")),key=head.request.headers.find(strut_string("sec-websocket-key"));if(connection==head.request.headers.end()||upgrade==head.request.headers.end()||version==head.request.headers.end()||key==head.request.headers.end())return strut_websocket_opening_result::invalid;bool close=false,keep_alive=false,has_upgrade=false;if(!strut_http_connection_tokens(connection->second.v,close,keep_alive,has_upgrade)||!has_upgrade)return strut_websocket_opening_result::invalid;bool has_websocket=false;if(!strut_websocket_upgrade_products(upgrade->second.v,has_websocket)||!has_websocket)return strut_websocket_opening_result::invalid;if(strut_trim_ascii(version->second.v)!="13")return strut_websocket_opening_result::unsupported_version;if(!strut_websocket_key(strut_trim_ascii(key->second.v))||head.request.headers.find(strut_string("transfer-encoding"))!=head.request.headers.end()||head.content_length!=0)return strut_websocket_opening_result::invalid;const auto subprotocol=head.request.headers.find(strut_string("sec-websocket-protocol"));if(subprotocol!=head.request.headers.end()){if(!strut_websocket_token_list(subprotocol->second.v,protocols))return strut_websocket_opening_result::invalid;std::vector<std::string> unique;for(const auto& protocol:protocols){if(std::find(unique.begin(),unique.end(),protocol)!=unique.end())return strut_websocket_opening_result::invalid;unique.push_back(protocol);}}const auto extensions=head.request.headers.find(strut_string("sec-websocket-extensions"));if(extensions!=head.request.headers.end()&&!strut_websocket_extensions(extensions->second.v))return strut_websocket_opening_result::invalid;if(!strut_websocket_sha1_self_test())return strut_websocket_opening_result::invalid;accept=strut_websocket_accept(strut_trim_ascii(key->second.v));return strut_websocket_opening_result::valid;}
inline std::string strut_serialize_websocket_switching_protocols(const std::string& accept,const std::string& protocol){std::string response="HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "+accept+"\r\n";if(!protocol.empty())response+="Sec-WebSocket-Protocol: "+protocol+"\r\n";return response+"\r\n";}
)STRUT_WEBSOCKET";
    out << R"STRUT_HTTP_TYPES(
enum class strut_http_request_body_phase{open,eof,closed,failed};
struct strut_http_request_body_state{std::mutex mutex;std::condition_variable idle_cv;std::function<std::string(std::size_t)> read;std::function<void()> interrupt,cancel;strut_http_request_framing framing=strut_http_request_framing::content_length;strut_http_request_body_phase phase=strut_http_request_body_phase::open;std::string buffer;std::size_t remaining=0,max_body=0,max_framing=0,declared_decoded=0,framing_bytes=0,chunk_remaining=0;bool need_chunk_crlf=false,active=true,reading=false,response_started=false;};
class strut_http_request_body{
public:
    strut_http_request_body()=default;explicit strut_http_request_body(std::shared_ptr<strut_http_request_body_state> state):state_(std::move(state)){}
    strut_bytes read_bytes(std::int64_t max_bytes) const{if(max_bytes<0)fail("request body read size cannot be negative");auto s=require();std::unique_lock<std::mutex> lock(s->mutex);require_active(*s);if(s->response_started)fail("request body cannot be read after response commitment");if(s->phase==strut_http_request_body_phase::closed)fail("request body is closed");if(s->phase==strut_http_request_body_phase::failed)fail("request body framing failed");if(max_bytes==0||s->phase==strut_http_request_body_phase::eof)return {};if(s->reading)fail("concurrent request body reads are not supported");s->reading=true;const std::size_t requested=static_cast<std::uint64_t>(max_bytes)>std::numeric_limits<std::size_t>::max()?std::numeric_limits<std::size_t>::max():static_cast<std::size_t>(max_bytes);try{auto out=s->framing==strut_http_request_framing::content_length?read_fixed_locked(*s,lock,requested):read_chunked_locked(*s,lock,requested);s->reading=false;s->idle_cv.notify_all();return out;}catch(...){if(s->active&&s->phase!=strut_http_request_body_phase::closed)s->phase=strut_http_request_body_phase::failed;s->reading=false;s->idle_cv.notify_all();auto cancel=s->cancel;lock.unlock();if(cancel)cancel();throw;}}
    strut_bytes read_all_bytes() const{return read_all_bytes(std::numeric_limits<std::int64_t>::max());}
    strut_bytes read_all_bytes(std::int64_t limit) const{if(limit<0)fail("request body read limit cannot be negative");strut_bytes out;for(;;){const std::size_t size=out.native_size();if(size>static_cast<std::uint64_t>(limit))fail("request body exceeds read limit");const std::int64_t allowance=limit-static_cast<std::int64_t>(size);auto chunk=read_bytes(std::min<std::int64_t>(8192,allowance==std::numeric_limits<std::int64_t>::max()?8192:allowance+1));if(chunk.empty()){if(eof())return out;continue;}if(chunk.native_size()>static_cast<std::uint64_t>(allowance))fail("request body exceeds read limit");out.append_native(reinterpret_cast<const char*>(chunk.data()),chunk.native_size());}}
    bool eof() const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);return s->phase==strut_http_request_body_phase::eof;}
    void close() const{auto s=require();std::unique_lock<std::mutex> lock(s->mutex);if(s->phase==strut_http_request_body_phase::failed)return;s->phase=strut_http_request_body_phase::closed;if(s->reading){auto interrupt=s->interrupt;lock.unlock();if(interrupt)interrupt();lock.lock();s->idle_cv.wait(lock,[&]{return !s->reading;});}}
    bool failed() const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);return s->phase==strut_http_request_body_phase::failed;}
    bool begin_response() const{auto s=require();std::lock_guard<std::mutex> lock(s->mutex);require_active(*s);if(s->phase==strut_http_request_body_phase::failed)fail("response cannot commit after request body failure");if(s->reading)fail("response cannot commit during a request body read");s->response_started=true;return s->phase==strut_http_request_body_phase::eof;}
    bool release(std::string& leftover) const{if(!state_)return false;std::unique_lock<std::mutex> lock(state_->mutex);if(state_->reading){auto interrupt=state_->interrupt;lock.unlock();if(interrupt)interrupt();lock.lock();state_->idle_cv.wait(lock,[&]{return !state_->reading;});}const bool reusable=state_->active&&state_->phase==strut_http_request_body_phase::eof;state_->active=false;if(reusable)leftover=std::move(state_->buffer);else state_->buffer.clear();state_->read={};state_->interrupt={};state_->cancel={};return reusable;}
    void invalidate() const{if(!state_)return;std::unique_lock<std::mutex> lock(state_->mutex);state_->active=false;if(state_->phase!=strut_http_request_body_phase::failed&&state_->phase!=strut_http_request_body_phase::eof)state_->phase=strut_http_request_body_phase::closed;if(state_->reading){auto interrupt=state_->interrupt;lock.unlock();if(interrupt)interrupt();lock.lock();state_->idle_cv.wait(lock,[&]{return !state_->reading;});}state_->read={};state_->interrupt={};state_->cancel={};})STRUT_HTTP_TYPES"; out << R"STRUT_HTTP_TYPES(
private:
    [[noreturn]] static void fail(const char* message){throw strut_checked_error("NetworkError",message);}
    std::shared_ptr<strut_http_request_body_state> require() const{if(!state_)fail("request body reader is not initialized");return state_;}
    static void require_active(const strut_http_request_body_state& s){if(!s.active)fail("request body reader is no longer active");}
    static void pull_locked(strut_http_request_body_state& s,std::unique_lock<std::mutex>& lock,std::size_t requested=8192){auto read=s.read;lock.unlock();std::string data;try{data=read(std::min<std::size_t>(requested,8192));}catch(...){lock.lock();fail("request body transport read failed");}lock.lock();require_active(s);if(s.phase==strut_http_request_body_phase::closed)fail("request body is closed");if(data.empty())fail("request body ended before its framing completed");s.buffer+=data;}
    static void ensure_locked(strut_http_request_body_state& s,std::unique_lock<std::mutex>& lock,std::size_t size){while(s.buffer.size()<size)pull_locked(s,lock,size-s.buffer.size());}
    static strut_bytes take_locked(strut_http_request_body_state& s,std::size_t size){strut_bytes out;out.append_native(s.buffer.data(),size);s.buffer.erase(0,size);return out;}
    static strut_bytes read_fixed_locked(strut_http_request_body_state& s,std::unique_lock<std::mutex>& lock,std::size_t requested){if(s.remaining==0){s.phase=strut_http_request_body_phase::eof;return {};}const std::size_t wanted=std::min(requested,s.remaining);if(wanted==0)return {};if(s.buffer.empty())pull_locked(s,lock,wanted);const std::size_t count=std::min(wanted,s.buffer.size());auto out=take_locked(s,count);s.remaining-=count;if(s.remaining==0)s.phase=strut_http_request_body_phase::eof;return out;}
    static bool parse_chunk_size(const std::string& line,std::size_t& size){std::size_t position=0,value=0,digits=0;while(position<line.size()&&std::isxdigit(static_cast<unsigned char>(line[position]))){const unsigned char c=static_cast<unsigned char>(line[position++]);const std::size_t digit=c>='0'&&c<='9'?c-'0':static_cast<unsigned char>(std::tolower(c))-'a'+10;if(value>(std::numeric_limits<std::size_t>::max()-digit)/16)return false;value=value*16+digit;++digits;}if(digits==0)return false;while(position<line.size()){if(line[position++]!=';')return false;const std::size_t name=position;while(position<line.size()&&strut_http_token_char(static_cast<unsigned char>(line[position])))++position;if(name==position)return false;if(position==line.size())continue;if(line[position]=='='){++position;if(position<line.size()&&line[position]=='"'){++position;bool closed=false;while(position<line.size()){const unsigned char c=static_cast<unsigned char>(line[position++]);if(c=='"'){closed=true;break;}if(c=='\\'){if(position>=line.size())return false;const unsigned char escaped=static_cast<unsigned char>(line[position++]);if((escaped<0x20&&escaped!='\t')||escaped==0x7f)return false;}else if((c<0x20&&c!='\t')||c==0x7f)return false;}if(!closed)return false;}else{const std::size_t token=position;while(position<line.size()&&strut_http_token_char(static_cast<unsigned char>(line[position])))++position;if(token==position)return false;}}}size=value;return true;}
    static void add_framing_locked(strut_http_request_body_state& s,std::size_t size){if(s.framing_bytes>s.max_framing||size>s.max_framing-s.framing_bytes)fail("chunk framing exceeds configured header limit");s.framing_bytes+=size;}
    static void next_chunk_locked(strut_http_request_body_state& s,std::unique_lock<std::mutex>& lock){if(s.need_chunk_crlf){ensure_locked(s,lock,2);if(s.buffer.compare(0,2,"\r\n")!=0)fail("invalid chunk payload terminator");s.buffer.erase(0,2);add_framing_locked(s,2);s.need_chunk_crlf=false;}for(;;){auto end=s.buffer.find("\r\n");while(end==std::string::npos){if(s.buffer.size()>8192)fail("chunk size line is too large");pull_locked(s,lock);end=s.buffer.find("\r\n");}if(end>8192)fail("chunk size line is too large");add_framing_locked(s,end+2);std::size_t size=0;if(!parse_chunk_size(s.buffer.substr(0,end),size))fail("invalid chunk size");s.buffer.erase(0,end+2);if(s.declared_decoded>s.max_body||size>s.max_body-s.declared_decoded)fail("decoded request body exceeds configured limit");s.declared_decoded+=size;if(size!=0){s.chunk_remaining=size;return;}for(;;){auto trailer_end=s.buffer.find("\r\n");while(trailer_end==std::string::npos){if(s.buffer.size()>8192)fail("chunk trailer line is too large");pull_locked(s,lock);trailer_end=s.buffer.find("\r\n");}add_framing_locked(s,trailer_end+2);if(trailer_end!=0)fail("request trailers are not supported");s.buffer.erase(0,2);s.phase=strut_http_request_body_phase::eof;return;}}}
    static strut_bytes read_chunked_locked(strut_http_request_body_state& s,std::unique_lock<std::mutex>& lock,std::size_t requested){if(requested==0)return {};while(s.chunk_remaining==0&&s.phase==strut_http_request_body_phase::open)next_chunk_locked(s,lock);if(s.phase==strut_http_request_body_phase::eof)return {};if(s.buffer.empty())pull_locked(s,lock,std::min(requested,s.chunk_remaining));const std::size_t count=std::min({requested,s.chunk_remaining,s.buffer.size()});auto out=take_locked(s,count);s.chunk_remaining-=count;if(s.chunk_remaining==0)s.need_chunk_crlf=true;return out;}
    std::shared_ptr<strut_http_request_body_state> state_;
};
inline bool strut_route_match(const std::string& pattern,const std::string& path,std::unordered_map<strut_string,strut_string>& params){std::stringstream a(pattern),b(path);std::string x,y;while(true){bool ax=static_cast<bool>(std::getline(a,x,'/')),by=static_cast<bool>(std::getline(b,y,'/'));if(!ax||!by)return ax==by;if(x.empty()&&y.empty())continue;if(!x.empty()&&x[0]==':')params[strut_string(x.substr(1))]=strut_string(y);else if(x!=y)return false;}}
)STRUT_HTTP_TYPES";
}

void emit_http_server(std::ostream& out, bool async_handlers, bool tls, bool websocket) {
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
    using request_stream_handler=std::function<void(strut_server_request,strut_http_request_body,strut_http_response_writer)>;
)STRUT_SERVER";
    if (websocket) out << R"STRUT_SERVER(
    using websocket_handler=std::function<void(strut_server_request,strut_websocket)>;
)STRUT_SERVER";
    out << R"STRUT_SERVER(
    strut_http_server():s_(std::make_shared<state>()){}
    void get(const strut_string& path,handler h) const{add_route("GET",path,std::move(h));}
    void post(const strut_string& path,handler h) const{add_route("POST",path,std::move(h));}
    void get_stream(const strut_string& path,stream_handler h) const{add_stream_route("GET",path,std::move(h));}
    void post_stream(const strut_string& path,stream_handler h) const{add_stream_route("POST",path,std::move(h));}
    void get_request_stream(const strut_string& path,request_stream_handler h) const{add_request_stream_route("GET",path,std::move(h));}
    void post_request_stream(const strut_string& path,request_stream_handler h) const{add_request_stream_route("POST",path,std::move(h));}
)STRUT_SERVER";
    if (websocket) out << R"STRUT_SERVER(
    void websocket(const strut_string& path,websocket_handler h) const{add_websocket_route(path,std::move(h));}
)STRUT_SERVER";
    if (async_handlers) out << R"STRUT_SERVER(
    void get_async(const strut_string& path,std::function<strut_future<strut_server_response>(strut_server_request)> h) const{get(path,[h=std::move(h)](strut_server_request r){return strut_await(h(std::move(r)));});}
    void post_async(const strut_string& path,std::function<strut_future<strut_server_response>(strut_server_request)> h) const{post(path,[h=std::move(h)](strut_server_request r){return strut_await(h(std::move(r)));});}
)STRUT_SERVER";
    out << R"STRUT_SERVER(
    void serve_static(const strut_string& prefix,const std::unordered_map<strut_string,strut_string>& files,const strut_string& fallback=strut_string()) const{std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("configure static files");s_->static_prefix=prefix.v;s_->static_files=files;s_->static_fallback=fallback.v;}
    void timeouts(std::int32_t read_ms,std::int32_t write_ms,std::int32_t idle_ms,std::int32_t shutdown_ms) const{if(read_ms<=0||write_ms<=0||idle_ms<=0||shutdown_ms<0)throw strut_checked_error("NetworkError","HTTP timeouts must be positive (shutdown may be zero)");std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("configure timeouts");s_->read_timeout_ms=read_ms;s_->write_timeout_ms=write_ms;s_->idle_timeout_ms=idle_ms;s_->shutdown_timeout_ms=shutdown_ms;}
    void limits(std::int64_t body_bytes,std::int64_t header_bytes,std::int32_t header_count,std::int32_t connections) const{if(body_bytes<0||header_bytes<1024||header_count<=0||connections<=0||static_cast<std::uint64_t>(body_bytes)>std::numeric_limits<std::size_t>::max()||static_cast<std::uint64_t>(header_bytes)>std::numeric_limits<std::size_t>::max())throw strut_checked_error("NetworkError","invalid HTTP server limits");std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("configure limits");s_->max_body_bytes=static_cast<std::size_t>(body_bytes);s_->max_header_bytes=static_cast<std::size_t>(header_bytes);s_->max_header_count=header_count;s_->max_connections=connections;}
)STRUT_SERVER";
    if (websocket) out << R"STRUT_SERVER(
    void websocket_limits(std::int64_t frame_bytes,std::int64_t message_bytes) const{if(frame_bytes<125||message_bytes<=0||frame_bytes>message_bytes||static_cast<std::uint64_t>(frame_bytes)>std::numeric_limits<std::size_t>::max()||static_cast<std::uint64_t>(message_bytes)>std::numeric_limits<std::size_t>::max())throw strut_checked_error("NetworkError","invalid WebSocket limits");std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("configure WebSocket limits");s_->max_websocket_frame=static_cast<std::size_t>(frame_bytes);s_->max_websocket_message=static_cast<std::size_t>(message_bytes);}
)STRUT_SERVER";
    out << R"STRUT_SERVER(
    bool running() const{return s_->running.load();}
    void stop() const{std::shared_ptr<run_state> run;{std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);if(s_->phase==lifecycle_phase::stopped)return;run=s_->current;s_->phase=lifecycle_phase::stopping;}if(!run)return;request_stop(run);if(worker_run_!=run.get()&&strut_execution_context!=run.get()){wait_for_drain(run);publish_lifecycle(s_,run);}}
    void listen(const strut_string& host,std::int32_t port,std::int32_t max_requests=0) const{
#ifndef _WIN32
        {std::lock_guard<std::mutex> signal_lock(strut_sigpipe_mutex);std::signal(SIGPIPE,SIG_IGN);}
#endif
        auto s=s_;run_server(host,port,max_requests,[s](strut_tcp_socket& socket){serve_connection(s,worker_run_,worker_connection_,socket);},[s](strut_tcp_socket& socket){set_socket_timeouts(socket,s->write_timeout_ms,s->write_timeout_ms);send_error(socket,503,"Service Unavailable");});
    }
)STRUT_SERVER";
    if (tls) out << R"STRUT_SERVER(
private:
    struct openssl_thread_cleanup{~openssl_thread_cleanup(){OPENSSL_thread_stop();}};
    class tls_socket{
    public:
        tls_socket(strut_tcp_socket socket,SSL* ssl):socket_(std::move(socket)),ssl_(ssl){}
        tls_socket(const tls_socket&)=delete;tls_socket& operator=(const tls_socket&)=delete;
        tls_socket(tls_socket&& other) noexcept:socket_(std::move(other.socket_)),ssl_(other.ssl_){other.ssl_=nullptr;}
        ~tls_socket(){close();}
        strut_socket_handle native_handle() const{return socket_.native_handle();}
        strut_socket_operation pin() const{return socket_.pin();}
        strut_string read(std::int64_t max_bytes=4096){if(max_bytes<=0)return {};auto operation=socket_.pin();std::string out(static_cast<std::size_t>(max_bytes),'\0');
#ifdef _WIN32
            const auto timeout=operation.state->read_timeout_ms.load();if(timeout>0){if(!strut_socket_set_blocking(operation.handle,false))throw strut_checked_error("TlsError","unable to configure TLS request socket");try{const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);for(;;){if(operation.state->interrupted.load())throw strut_checked_error("TlsError","TLS request read interrupted");ERR_clear_error();const int count=SSL_read(ssl_,out.data(),static_cast<int>(out.size()));if(count>0){if(!strut_socket_set_blocking(operation.handle,true))throw strut_checked_error("TlsError","unable to restore TLS request socket");out.resize(static_cast<std::size_t>(count));return strut_string(std::move(out));}const int error=SSL_get_error(ssl_,count);if(error==SSL_ERROR_ZERO_RETURN){if(!strut_socket_set_blocking(operation.handle,true))throw strut_checked_error("TlsError","unable to restore TLS request socket");return {};}if(error!=SSL_ERROR_WANT_READ&&error!=SSL_ERROR_WANT_WRITE)throw strut_checked_error("TlsError","TLS request read failed");const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()+std::chrono::milliseconds(1)).count();if(remaining<=0)throw strut_checked_error("TlsError","TLS request read failed");const int ready=(error==SSL_ERROR_WANT_WRITE?strut_socket_poll_write(operation.handle,static_cast<int>(std::min<std::int64_t>(remaining,100))):strut_socket_poll_read(operation.handle,static_cast<int>(std::min<std::int64_t>(remaining,100))));if(ready<0)throw strut_checked_error("TlsError","TLS request read failed");}}catch(...){strut_socket_set_blocking(operation.handle,true);throw;}}
#endif
            int n=SSL_read(ssl_,out.data(),static_cast<int>(out.size()));if(n==0)return {};if(n<0)throw strut_checked_error("TlsError","TLS request read failed");out.resize(static_cast<std::size_t>(n));return strut_string(std::move(out));}
        void write(const strut_string& data){auto operation=socket_.pin();std::size_t offset=0;while(offset<data.v.size()){const int amount=static_cast<int>(std::min<std::size_t>(data.v.size()-offset,static_cast<std::size_t>(std::numeric_limits<int>::max())));int n=SSL_write(ssl_,data.v.data()+offset,amount);if(n<=0)throw strut_checked_error("TlsError","TLS response write failed");offset+=static_cast<std::size_t>(n);}}
        void close(){if(ssl_){try{auto operation=socket_.pin();SSL_shutdown(ssl_);}catch(...){ }SSL_free(ssl_);ssl_=nullptr;}socket_.close();}
        void close_after_write(bool drain_input=true){if(!ssl_){socket_.close();return;}try{auto operation=socket_.pin();if(strut_socket_set_blocking(operation.handle,false)){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);if(drain_input){std::size_t drained=0;char buffer[4096];while(drained<65536){ERR_clear_error();const int count=SSL_read(ssl_,buffer,sizeof(buffer));if(count>0){drained+=static_cast<std::size_t>(count);continue;}const int error=SSL_get_error(ssl_,count);if(error==SSL_ERROR_ZERO_RETURN)break;if(error!=SSL_ERROR_WANT_READ&&error!=SSL_ERROR_WANT_WRITE)break;if(!wait(operation.handle,error,deadline))break;}}for(;;){ERR_clear_error();const int result=SSL_shutdown(ssl_);if(result>=0)break;const int error=SSL_get_error(ssl_,result);if(error!=SSL_ERROR_WANT_READ&&error!=SSL_ERROR_WANT_WRITE)break;if(!wait(operation.handle,error,deadline))break;}}}catch(...){ }SSL_free(ssl_);ssl_=nullptr;socket_.close();}
    private:
        static bool wait(strut_socket_handle handle,int error,std::chrono::steady_clock::time_point deadline){const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()+std::chrono::milliseconds(1)).count();if(remaining<=0)return false;return (error==SSL_ERROR_WANT_WRITE?strut_socket_poll_write(handle,static_cast<int>(remaining)):strut_socket_poll_read(handle,static_cast<int>(remaining)))>0;}
        strut_tcp_socket socket_;SSL* ssl_=nullptr;
    };
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
        auto s=s_;run_server(host,port,max_requests,[s,context](strut_tcp_socket& socket){openssl_thread_cleanup cleanup;std::unique_ptr<SSL,decltype(&SSL_free)> ssl(SSL_new(context.get()),SSL_free);if(!ssl)return;if(SSL_set_fd(ssl.get(),static_cast<int>(socket.native_handle()))!=1||!tls_accept_until(socket,ssl.get(),std::min({s->read_timeout_ms,s->write_timeout_ms,s->idle_timeout_ms})))return;tls_socket secure(socket,ssl.release());serve_connection(s,worker_run_,worker_connection_,secure);},[](strut_tcp_socket& socket){socket.close();});
    }
)STRUT_SERVER";
    out << R"STRUT_SERVER(
private:
    struct route{std::string method,path;handler fn;stream_handler stream;request_stream_handler request_stream;
)STRUT_SERVER";
    if (websocket) out << "websocket_handler websocket{};";
    out << R"STRUT_SERVER(};
    enum class lifecycle_phase{stopped,starting,running,stopping};
    struct connection{explicit connection(strut_tcp_socket value):socket(std::move(value)){}strut_tcp_socket socket;std::shared_ptr<strut_cancellation_source> request_cancellation;bool running=false,idle=false,served_request=false,websocket=false;};
    struct run_state{std::mutex mutex;std::condition_variable work_cv,drain_cv;std::deque<std::shared_ptr<connection>> queue,connections;std::shared_ptr<connection> rejecting;strut_tcp_listener listener;std::size_t in_flight=0;bool accepting=true,startup_complete=false,was_running=false,workers_stopping=false,forced=false,deadline_set=false;std::chrono::steady_clock::time_point deadline;std::int32_t shutdown_timeout_ms=0;};
    struct state{std::vector<route> routes;std::string static_prefix,static_fallback;std::unordered_map<strut_string,strut_string> static_files;std::atomic<bool> running{false};std::mutex lifecycle_mutex;std::shared_ptr<run_state> current;lifecycle_phase phase=lifecycle_phase::stopped;std::int32_t read_timeout_ms=30000,write_timeout_ms=30000,idle_timeout_ms=5000,shutdown_timeout_ms=5000,max_header_count=100,max_connections=1024;std::size_t max_body_bytes=1024*1024,max_header_bytes=64*1024;
)STRUT_SERVER";
    if (websocket) out << "std::size_t max_websocket_frame=1024*1024,max_websocket_message=4*1024*1024;";
    out << R"STRUT_SERVER(};
    std::shared_ptr<state> s_;
    inline static thread_local run_state* worker_run_=nullptr;
    inline static thread_local connection* worker_connection_=nullptr;
    void require_stopped_locked(const char* action) const{if(s_->phase!=lifecycle_phase::stopped)throw strut_checked_error("NetworkError",std::string("cannot ")+action+" while HTTP server is running");if(s_->current){std::lock_guard<std::mutex> run_lock(s_->current->mutex);if(s_->current->in_flight!=0)throw strut_checked_error("NetworkError",std::string("cannot ")+action+" while HTTP server shutdown is still in progress");}}
    void add_route(const char* method,const strut_string& path,handler h) const{std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("register routes");s_->routes.push_back({method,path.v,std::move(h),{},{}});}
    void add_stream_route(const char* method,const strut_string& path,stream_handler h) const{std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("register routes");s_->routes.push_back({method,path.v,{},std::move(h),{}});}
    void add_request_stream_route(const char* method,const strut_string& path,request_stream_handler h) const{std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("register routes");s_->routes.push_back({method,path.v,{},{},std::move(h)});}
)STRUT_SERVER";
    if (websocket) out << R"STRUT_SERVER(
    void add_websocket_route(const strut_string& path,websocket_handler h) const{std::lock_guard<std::mutex> lock(s_->lifecycle_mutex);require_stopped_locked("register routes");s_->routes.push_back({{},path.v,{},{},{},std::move(h)});}
)STRUT_SERVER";
    out << R"STRUT_SERVER(
    class request_scope{public:request_scope(run_state* run,connection* connection,strut_server_request& request):run_(run),connection_(connection),source_(std::make_shared<strut_cancellation_source>()){request.cancellation=source_->token();bool stopping=false;{std::lock_guard<std::mutex> lock(run_->mutex);connection_->request_cancellation=source_;connection_->idle=false;stopping=!run_->accepting;}if(stopping)source_->cancel();}~request_scope(){source_->cancel();std::lock_guard<std::mutex> lock(run_->mutex);if(connection_->request_cancellation==source_)connection_->request_cancellation.reset();}const std::shared_ptr<strut_cancellation_source>& source() const{return source_;}private:run_state* run_;connection* connection_;std::shared_ptr<strut_cancellation_source> source_;};
    static void request_stop(const std::shared_ptr<run_state>& run){std::vector<std::shared_ptr<strut_cancellation_source>> cancellations;std::vector<std::shared_ptr<connection>> interrupted;std::shared_ptr<connection> rejecting;{std::lock_guard<std::mutex> lock(run->mutex);run->accepting=false;if(!run->deadline_set){run->deadline_set=true;run->deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(run->shutdown_timeout_ms);}rejecting=run->rejecting;for(const auto& connection:run->connections){if(connection->request_cancellation)cancellations.push_back(connection->request_cancellation);if(connection->running&&(connection->idle||connection->websocket))interrupted.push_back(connection);}}for(const auto& source:cancellations)source->cancel();for(const auto& connection:interrupted)connection->socket.shutdown_io();if(rejecting)rejecting->socket.close();run->listener.close();run->work_cv.notify_all();}
    static void force_shutdown_locked(const std::shared_ptr<run_state>& run){if(run->forced)return;run->forced=true;while(!run->queue.empty()){auto connection=run->queue.front();run->queue.pop_front();connection->socket.close();auto found=std::find(run->connections.begin(),run->connections.end(),connection);if(found!=run->connections.end())run->connections.erase(found,found+1);--run->in_flight;}for(const auto& connection:run->connections)if(connection->running){if(connection->request_cancellation)connection->request_cancellation->cancel();connection->socket.close();}run->workers_stopping=true;run->work_cv.notify_all();run->drain_cv.notify_all();}
    static bool wait_for_drain(const std::shared_ptr<run_state>& run){std::unique_lock<std::mutex> lock(run->mutex);if(!run->deadline_set){run->deadline_set=true;run->deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(run->shutdown_timeout_ms);}bool drained=run->drain_cv.wait_until(lock,run->deadline,[&]{return run->startup_complete&&run->in_flight==0;});if(!drained){force_shutdown_locked(run);drained=run->startup_complete&&run->in_flight==0;}else{run->workers_stopping=true;run->work_cv.notify_all();}return drained;}
    static void publish_lifecycle(const std::shared_ptr<state>& s,const std::shared_ptr<run_state>& run){std::lock_guard<std::mutex> lifecycle_lock(s->lifecycle_mutex);if(s->current!=run)return;std::lock_guard<std::mutex> run_lock(run->mutex);const bool drained=run->startup_complete&&run->in_flight==0;s->phase=drained?lifecycle_phase::stopped:lifecycle_phase::stopping;s->running.store(!drained&&run->was_running);}
    static void worker_loop(const std::shared_ptr<state>& s,const std::shared_ptr<run_state>& run,const std::function<void(strut_tcp_socket&)>& process){for(;;){std::shared_ptr<connection> connection;{std::unique_lock<std::mutex> lock(run->mutex);run->work_cv.wait(lock,[&]{return run->workers_stopping||!run->queue.empty();});if(run->queue.empty()){if(run->workers_stopping)return;continue;}connection=run->queue.front();run->queue.pop_front();connection->running=true;}const void* previous_context=strut_execution_context;worker_run_=run.get();worker_connection_=connection.get();strut_execution_context=run.get();try{process(connection->socket);}catch(...){connection->socket.close();}strut_execution_context=previous_context;worker_connection_=nullptr;worker_run_=nullptr;bool retired_drained=false;{std::lock_guard<std::mutex> lock(run->mutex);auto found=std::find(run->connections.begin(),run->connections.end(),connection);if(found!=run->connections.end())run->connections.erase(found);if(run->in_flight>0)--run->in_flight;if(run->in_flight==0){retired_drained=run->forced;run->drain_cv.notify_all();}}if(retired_drained)publish_lifecycle(s,run);}})STRUT_SERVER"; out << R"STRUT_SERVER(
    void run_server(const strut_string& host,std::int32_t port,std::int32_t max_requests,const std::function<void(strut_tcp_socket&)>& process,const std::function<void(strut_tcp_socket&)>& reject) const{auto s=s_;auto run=std::make_shared<run_state>();{std::lock_guard<std::mutex> lock(s->lifecycle_mutex);require_stopped_locked("start HTTP server");run->shutdown_timeout_ms=s->shutdown_timeout_ms;s->current=run;s->phase=lifecycle_phase::starting;}strut_tcp_listener listener;try{listener=tcp_listen(host,port);}catch(...){{std::lock_guard<std::mutex> lock(run->mutex);run->startup_complete=true;run->drain_cv.notify_all();}std::lock_guard<std::mutex> lock(s->lifecycle_mutex);if(s->current==run){s->current.reset();s->phase=lifecycle_phase::stopped;s->running.store(false);}throw;}bool accepting=false;{std::lock_guard<std::mutex> lock(run->mutex);accepting=run->accepting;if(accepting)run->listener=std::move(listener);else listener.close();run->startup_complete=true;run->drain_cv.notify_all();}{std::lock_guard<std::mutex> lock(s->lifecycle_mutex);if(s->current==run&&s->phase==lifecycle_phase::starting&&accepting){std::lock_guard<std::mutex> run_lock(run->mutex);run->was_running=true;s->phase=lifecycle_phase::running;s->running.store(true);}else accepting=false;}std::vector<std::thread> workers;std::uint64_t admitted=0;std::exception_ptr failure;if(accepting)try{while(max_requests<=0||admitted<static_cast<std::uint64_t>(max_requests)){strut_tcp_socket socket;try{socket=run->listener.accept();}catch(...){std::lock_guard<std::mutex> lock(run->mutex);if(!run->accepting)break;throw;}auto connection=std::make_shared<strut_http_server::connection>(std::move(socket));bool saturated=false;{std::lock_guard<std::mutex> lock(run->mutex);if(!run->accepting){connection->socket.close();break;}saturated=run->in_flight>=static_cast<std::size_t>(s->max_connections);if(saturated)run->rejecting=connection;else{if(workers.size()<=run->in_flight&&workers.size()<static_cast<std::size_t>(s->max_connections))workers.emplace_back([s,run,process]{worker_loop(s,run,process);});run->connections.push_back(connection);try{run->queue.push_back(connection);}catch(...){run->connections.pop_back();throw;}++run->in_flight;}}if(saturated){reject(connection->socket);std::lock_guard<std::mutex> lock(run->mutex);if(run->rejecting==connection)run->rejecting.reset();continue;}++admitted;run->work_cv.notify_one();}}catch(...){failure=std::current_exception();}request_stop(run);const bool drained=wait_for_drain(run);if(drained)publish_lifecycle(s,run);for(auto& worker:workers)if(worker.joinable()){if(drained)worker.join();else worker.detach();}publish_lifecycle(s,run);if(failure)std::rethrow_exception(failure);}
    template<class Socket> static void set_socket_timeouts(const Socket& socket,std::int32_t read_ms,std::int32_t write_ms){
        auto operation=socket.pin();const auto handle=operation.handle;
#ifdef _WIN32
        DWORD read=static_cast<DWORD>(read_ms),write=static_cast<DWORD>(write_ms);if(setsockopt(handle,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&read),sizeof(read))!=0||setsockopt(handle,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&write),sizeof(write))!=0)throw strut_checked_error("NetworkError","unable to configure HTTP socket timeouts");operation.state->read_timeout_ms.store(read_ms);
#else
        timeval read{read_ms/1000,(read_ms%1000)*1000},write{write_ms/1000,(write_ms%1000)*1000};if(setsockopt(handle,SOL_SOCKET,SO_RCVTIMEO,&read,sizeof(read))!=0||setsockopt(handle,SOL_SOCKET,SO_SNDTIMEO,&write,sizeof(write))!=0)throw strut_checked_error("NetworkError","unable to configure HTTP socket timeouts");
#endif
    }
    template<class Socket> static strut_http_response_writer response_writer(Socket& socket,strut_http_version version,bool persistent=false,bool head_request=false,std::function<bool()> before_commit={},std::function<bool()> shutdown_started={},std::function<void()> cancel={}){auto state=std::make_shared<strut_http_response_writer_state>();state->version=version;state->request_persistent=persistent;state->head_request=head_request;state->before_commit=std::move(before_commit);state->shutdown_started=std::move(shutdown_started);state->cancel=std::move(cancel);state->send=[&socket](const char* data,std::size_t size){socket.write(strut_string(std::string(data,size)));};return strut_http_response_writer(std::move(state));}
    template<class Socket> static void interrupt_socket(Socket& socket){try{auto operation=socket.pin();
#ifdef _WIN32
        ::shutdown(operation.handle,SD_BOTH);
#else
        ::shutdown(operation.handle,SHUT_RDWR);
#endif
        }catch(...){}}
    template<class Socket> static strut_http_request_body request_body(Socket& socket,const strut_http_request_head& head,std::string buffered,std::size_t max_body,std::size_t max_framing,std::function<void()> cancel){auto state=std::make_shared<strut_http_request_body_state>();state->framing=head.framing;state->remaining=head.content_length;state->max_body=max_body;state->max_framing=max_framing;state->buffer=std::move(buffered);state->cancel=std::move(cancel);state->read=[&socket](std::size_t size){return socket.read(static_cast<std::int64_t>(std::min<std::size_t>(size,8192))).v;};state->interrupt=[&socket]{interrupt_socket(socket);};if(state->framing==strut_http_request_framing::content_length&&state->remaining==0)state->phase=strut_http_request_body_phase::eof;return strut_http_request_body(std::move(state));}
    static strut_string read_buffered_body(const strut_http_request_body& body){return body.read_all_bytes().to_string();})STRUT_SERVER"; out << R"STRUT_SERVER(
    static void write_buffered(const strut_http_response_writer& writer,const strut_server_response& response){writer.status(response.status);writer.content_type(response.content_type);for(const auto& header:response.headers)writer.header(header.first,header.second);for(const auto& cookie:response.cookies)writer.cookie(cookie);writer.content_length(static_cast<std::int64_t>(response.body.v.size()));writer.write(response.body);writer.finish();}
    template<class Socket> static void send_error(Socket& socket,std::int32_t status,const char* message,strut_http_version version=strut_http_version::http_1_1,bool head_request=false){try{auto writer=response_writer(socket,version,false,head_request);strut_server_response response{status,strut_string(message),"text/plain; charset=utf-8",{}, {}};write_buffered(writer,response);writer.invalidate();socket.close_after_write();}catch(...){socket.close();}}
)STRUT_SERVER";
    if (websocket) out << R"STRUT_SERVER(
    template<class Socket> static void send_websocket_version_error(Socket& socket){try{auto writer=response_writer(socket,strut_http_version::http_1_1,false,false);strut_server_response response{426,"Upgrade Required","text/plain; charset=utf-8",{{strut_string("Sec-WebSocket-Version"),strut_string("13")}}, {}};write_buffered(writer,response);writer.invalidate();socket.close_after_write();}catch(...){socket.close();}}
)STRUT_SERVER";
    out << R"STRUT_SERVER(
    static strut_string mime(const std::string& p){auto dot=p.rfind('.');auto e=dot==std::string::npos?std::string():p.substr(dot);if(e==".html")return "text/html; charset=utf-8";if(e==".css")return "text/css; charset=utf-8";if(e==".js")return "application/javascript";if(e==".json")return "application/json";if(e==".svg")return "image/svg+xml";if(e==".png")return "image/png";return "application/octet-stream";}
    static std::string etag(const std::string& data){std::uint64_t h=1469598103934665603ull;for(unsigned char c:data){h^=c;h*=1099511628211ull;}std::ostringstream out;out<<'"'<<std::hex<<h<<'"';return out.str();}
    static bool stopping(run_state* run){std::lock_guard<std::mutex> lock(run->mutex);return !run->accepting;}
    static bool begin_idle(run_state* run,connection* connection){std::lock_guard<std::mutex> lock(run->mutex);if(!run->accepting&&connection->served_request)return false;connection->idle=true;return true;}
    static void set_idle(run_state* run,connection* connection,bool idle){std::lock_guard<std::mutex> lock(run->mutex);connection->idle=idle;}
    template<class Socket> static void serve_connection(const std::shared_ptr<state>& s,run_state* run,connection* connection,Socket& socket){
        std::string raw;
        auto send_head_error=[&](std::int32_t status,const char* message){strut_http_version version=strut_http_version::http_1_1;bool head_request=false;const auto line_end=raw.find("\r\n");if(line_end!=std::string::npos){auto context=strut_parse_http_request_head(raw.substr(0,line_end+2),s->max_body_bytes,s->max_header_count);version=context.head.version;head_request=context.head.request.method.v=="HEAD";}send_error(socket,status,message,version,head_request);};
        try{for(;;){
            if(!begin_idle(run,connection)){socket.close();return;}
            set_socket_timeouts(socket,s->idle_timeout_ms,s->write_timeout_ms);
            std::size_t header_end=raw.find("\r\n\r\n");
            while(header_end==std::string::npos){auto chunk=socket.read(4096).v;if(chunk.empty()){if(raw.empty())socket.close();else send_head_error(400,"Bad Request");return;}if(raw.empty())set_socket_timeouts(socket,s->read_timeout_ms,s->write_timeout_ms);raw+=chunk;header_end=raw.find("\r\n\r\n");const std::size_t inspected=header_end==std::string::npos?raw.size():header_end+4;for(std::size_t i=0;i<inspected;++i){if(raw[i]=='\n'&&(i==0||raw[i-1]!='\r')){send_head_error(400,"Bad Request");return;}if(raw[i]=='\r'&&i+1<inspected&&raw[i+1]!='\n'){send_head_error(400,"Bad Request");return;}}if(header_end==std::string::npos&&raw.size()>s->max_header_bytes){send_head_error(431,"Request Header Fields Too Large");return;}}
            if(header_end+4>s->max_header_bytes){send_head_error(431,"Request Header Fields Too Large");return;}
            set_idle(run,connection,false);connection->served_request=true;set_socket_timeouts(socket,s->read_timeout_ms,s->write_timeout_ms);)STRUT_SERVER"; out << R"STRUT_SERVER(
            auto parsed=strut_parse_http_request_head(raw.substr(0,header_end+2),s->max_body_bytes,s->max_header_count);const bool head_request=parsed.head.request.method.v=="HEAD";if(!parsed){send_error(socket,parsed.status,parsed.message,parsed.head.version,head_request);return;}const auto version=parsed.head.version;
)STRUT_SERVER";
    if (websocket) out << R"STRUT_SERVER(
            route* websocket_route=nullptr;for(auto& route:s->routes){if(!route.websocket)continue;parsed.head.request.params.clear();if(strut_route_match(route.path,parsed.head.request.path.v,parsed.head.request.params)){websocket_route=&route;break;}}
            if(websocket_route){std::string accept;std::vector<std::string> protocols;const auto opening=strut_validate_websocket_opening(parsed.head,accept,protocols);if(opening==strut_websocket_opening_result::unsupported_version){send_websocket_version_error(socket);return;}if(opening!=strut_websocket_opening_result::valid){send_error(socket,400,"Bad Request",version,head_request);return;}strut_server_request req=std::move(parsed.head.request);request_scope scope(run,connection,req);auto websocket_state=std::make_shared<strut_websocket_state>();websocket_state->buffered=raw.substr(header_end+4);websocket_state->offered_protocols=std::move(protocols);websocket_state->max_frame=s->max_websocket_frame;websocket_state->max_message=s->max_websocket_message;websocket_state->read=[&socket](std::size_t size){return socket.read(static_cast<std::int64_t>(std::min<std::size_t>(size,8192))).v;};websocket_state->send=[&socket](const char* data,std::size_t size){socket.write(strut_string(std::string(data,size)));};websocket_state->interrupt=[&socket]{interrupt_socket(socket);};websocket_state->commit=[&socket,accept,run,connection](const std::string& protocol){socket.write(strut_string(strut_serialize_websocket_switching_protocols(accept,protocol)));bool stopped=false;{std::lock_guard<std::mutex> lock(run->mutex);connection->websocket=true;stopped=!run->accepting;}if(stopped)interrupt_socket(socket);};strut_websocket upgraded(websocket_state);try{websocket_route->websocket(req,upgraded);}catch(...){upgraded.finish(1011);bool failed=false;const bool committed=upgraded.invalidate(&failed);if(committed||failed)socket.close();else send_error(socket,500,"Internal Server Error",version,head_request);return;}upgraded.finish();bool failed=false;const bool committed=upgraded.invalidate(&failed);if(committed||failed)socket.close();else send_error(socket,403,"Forbidden",version,head_request);return;}
)STRUT_SERVER";
    out << R"STRUT_SERVER(
            if(parsed.head.upgrade_requested){send_error(socket,501,"Not Implemented",version,head_request);return;}
            strut_server_request req=std::move(parsed.head.request);const bool request_persistent=parsed.head.persistent;std::string next;bool body_reusable=false,response_reusable=false,completed=false;
            {request_scope scope(run,connection,req);auto source=scope.source();auto cancel=[source]{source->cancel();};auto shutdown=[run]{return stopping(run);};auto body=request_body(socket,parsed.head,raw.substr(header_end+4),s->max_body_bytes,s->max_header_bytes,cancel);strut_server_response response;bool found=false,method_mismatch=false;const std::string route_method=head_request?"GET":req.method.v;
                for(auto& route:s->routes){
)STRUT_SERVER";
    if (websocket) out << "if(route.websocket)continue;";
    out << R"STRUT_SERVER(req.params.clear();if(!strut_route_match(route.path,req.path.v,req.params))continue;if(route.method!=route_method){method_mismatch=true;continue;}
                    if(route.request_stream){auto writer=response_writer(socket,version,request_persistent,head_request,[body]{return body.begin_response();},shutdown,cancel);try{route.request_stream(req,body,writer);if(body.failed())throw strut_checked_error("NetworkError","request body framing failed");writer.finish();response_reusable=writer.reusable();body_reusable=body.release(next);writer.invalidate();completed=true;}catch(...){const bool malformed=body.failed();const bool was_committed=writer.abort();body.release(next);if(was_committed)socket.close();else send_error(socket,malformed?400:500,malformed?"Bad Request":"Internal Server Error",version,head_request);return;}break;}
                    try{req.body=read_buffered_body(body);req.buffered_body_available=true;}catch(...){body.release(next);send_error(socket,400,"Bad Request",version,head_request);return;}body_reusable=body.release(next);
                    if(route.stream){auto writer=response_writer(socket,version,request_persistent,head_request,{},shutdown,cancel);try{route.stream(req,writer);writer.finish();response_reusable=writer.reusable();writer.invalidate();completed=true;}catch(...){const bool was_committed=writer.abort();if(was_committed)socket.close();else send_error(socket,500,"Internal Server Error",version,head_request);return;}break;}
                    try{response=route.fn(req);}catch(...){send_error(socket,500,"Internal Server Error",version,head_request);return;}found=true;break;
                }
                if(!completed){if(!body_reusable){try{req.body=read_buffered_body(body);req.buffered_body_available=true;}catch(...){body.release(next);send_error(socket,400,"Bad Request",version,head_request);return;}body_reusable=body.release(next);}if(!found&&!s->static_files.empty()&&route_method=="GET"){std::string key=req.path.v;if(!s->static_prefix.empty()&&key.rfind(s->static_prefix,0)==0)key=key.substr(s->static_prefix.size());while(!key.empty()&&key.front()=='/')key.erase(key.begin());if(key.empty())key="index.html";if(key.find("..")!=std::string::npos){response.status=400;response.body="Bad Request";found=true;}else{auto it=s->static_files.find(strut_string(key));if(it==s->static_files.end()&&!s->static_fallback.empty())it=s->static_files.find(strut_string(s->static_fallback));if(it!=s->static_files.end()){response.status=200;response.body=it->second;response.content_type=mime(key);response.headers[strut_string("ETag")]=strut_string(etag(response.body.v));response.headers[strut_string("Cache-Control")]=strut_string("public, max-age=0, must-revalidate");found=true;}}}if(!found){response.status=method_mismatch?405:404;response.body=method_mismatch?"Method Not Allowed":"Not Found";}auto writer=response_writer(socket,version,request_persistent,head_request,{},shutdown,cancel);try{write_buffered(writer,response);response_reusable=writer.reusable();writer.invalidate();completed=true;}catch(...){const bool was_committed=writer.abort();if(was_committed)socket.close();else send_error(socket,500,"Internal Server Error",version,head_request);return;}}
            }
            if(!completed||stopping(run)){socket.close();return;}if(!body_reusable){socket.close_after_write();return;}if(!response_reusable){socket.close_after_write(false);return;}raw=std::move(next);
        }}catch(...){socket.close();}
    }
};
)STRUT_SERVER";
}

} // namespace strut::generated_runtime
