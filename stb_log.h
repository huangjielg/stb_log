/* stb_log - v1.0.0

 // first define log severity
 #define LOG_SEVERITY_LEVEL 10

 // start a logger that write to std out
 start_logger();
 
 // start a logger that write to file "log/test.log"
 start_file_logger("log/test.log");
 
 // or manually setup a logger
 CLogFile *err = new CLogFile("log/error.log");
 // filter that only accept Error message
 err->set_filter([](const LogData* log) -> bool {
 	return log->level >= LOG_ERROR;
 });
 // start collecting messages
 start_handler_thread(err);
 
 // now we can write log message at any threads
 // all started handlers will do the I/O jobs
 // the log_xxx macros will be stripped according LOG_SEVERITY_LEVEL
 
 int i = 255;
 float f = 3.1415926f;
 const char *c = "const chars";
 std::tstring s = "std::tstring";
 
 log_write(LOG_INFO, "TEST", "current log sevirity level is [%d]", log_severity_level);
 log_debug("debug message: %d, %f, '%s', '%s'", i, f, c, s);
 log_info("info message: %d, %f, '%s', '%s'", i, f, c, s);
 log_warning("warning message: %d, %f, '%s', '%s'", i, f, c, s);
 log_error("error message: %d, %f, '%s', '%s'", i, f, c, s);
 log_critical("critical message: %d, %f, '%s', '%s'", i, f, c, s);

 // finnally close the logger
 close_logger();

 */

#ifndef INCLUDE_STB_LOG_H
#define INCLUDE_STB_LOG_H

#include <cassert>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <string>
#include <tchar.h>
namespace std {
#ifdef _UNICODE
#define to_tstring to_wstring
typedef wstring tstring;
#else
typedef string tstring;
#define to_tstring to_string
#endif
};

#ifdef USE_NAMESPACE
#ifndef STB_LOG_NAMESPACE
#define STB_LOG_NAMESPACE stb
#endif
namespace STB_LOG_NAMESPACE {
#endif

// --------------------------------
// library settings
// --------------------------------
enum StbLogLevel {
	LOG_CRITICAL = 50,
	LOG_ERROR = 40,
	LOG_WARNING = 30,
	LOG_INFO = 20,
	LOG_DEBUG = 10,
};

// log severity level
#ifndef LOG_SEVERITY_LEVEL
#define LOG_SEVERITY_LEVEL 0
#endif
// default log file rotate size (256 MB)
#define LOG_FILE_ROTATE_SIZE (256*1024*1024)
// define log file rotate count
// keep latest 8 log file
#define LOG_FILE_ROTATE_COUNT 8
// logger queue buffer size
// writer(producer) thread will block when the queue is full
// change the size according the maximum concurrency
#define LOG_QUEUE_SIZE 256
// logger worker thread sleep time when it's casual (in milliseconds)
#define LOG_WORKER_SLEEP_TIME 30
// logger worker batch size
#define LOG_BATCH_SIZE 64
// string logger default buffer size
#define LOG_STRING_SIZE 1024
#define LOG_STRING_SIZE_MAX 1024 * 1024
// cache line size
#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
#endif

// stb_log namespace
#ifdef USE_NAMESPACE
#define STB_LOG_LEVEL STB_LOG_NAMESPACE::StbLogLevel
#else
#define STB_LOG_LEVEL StbLogLevel
#endif
#define log_level(n) (get_log_context()->logger->set_ActiveLevel(n))
// write log
#define log_write(lvl, channel, fmt, ...) (get_log_context()->logger && get_log_context()->logger->write(lvl, channel, (fmt), ##__VA_ARGS__))
#ifdef LOG_SEVERITY_LEVEL
// write critical log
#if LOG_SEVERITY_LEVEL <= 50
#define log_critical(fmt, ...) (get_log_context()->logger && get_log_context()->logger->write(STB_LOG_LEVEL::LOG_CRITICAL, _T("CRITICAL"), (fmt), ##__VA_ARGS__))
#else
#define log_critical(fmt,...)
#endif
// write error log
#if LOG_SEVERITY_LEVEL <= 40
#define log_error(fmt, ...) (get_log_context()->logger && get_log_context()->logger->write(STB_LOG_LEVEL::LOG_ERROR, _T("ERROR"), (fmt), ##__VA_ARGS__))
#else
#define log_error(fmt,...)
#endif
// write warning log
#if LOG_SEVERITY_LEVEL <= 30
#define log_warning(fmt, ...) (get_log_context()->logger && get_log_context()->logger->write(STB_LOG_LEVEL::LOG_WARNING, _T("WARNING"), (fmt), ##__VA_ARGS__))
#else
#define log_warning(fmt,...)
#endif
// write info log
#if LOG_SEVERITY_LEVEL <= 20
#define log_info(fmt, ...) (get_log_context()->logger && get_log_context()->logger->write(STB_LOG_LEVEL::LOG_INFO, _T("INFO"), (fmt), ##__VA_ARGS__))
#else
#define log_info(fmt,...)
#endif
// write debug log
#if LOG_SEVERITY_LEVEL <= 10
#define log_debug(fmt, ...) (get_log_context()->logger && get_log_context()->logger->write(STB_LOG_LEVEL::LOG_DEBUG, _T("DEBUG"), (fmt), ##__VA_ARGS__))
#else
#define log_debug(fmt,...)
#endif
#else // skip log macro
#define log_critical(fmt,...)
#define log_error(fmt,...)
#define log_warning(fmt,...)
#define log_info(fmt,...)
#define log_debug(fmt,...)
#endif // LOG_SEVERITY_LEVEL

// --------------------------------
// public user interface
// --------------------------------
class CLogger;
class CLogHandler;

struct LogContext {
	CLogger *logger;
	std::vector<std::unique_ptr<std::thread>> thread_pool;
};
typedef std::chrono::milliseconds::rep millisecond_t;

// get global logger info
inline LogContext *get_log_context() {
	static LogContext s_logger_context;
	return &s_logger_context;
}

// get global logger
inline CLogger* get_logger() {
	return get_log_context()->logger;
}

// close logger
void close_logger();

// start a logger thread
void start_handler_thread(CLogHandler *handler, millisecond_t sleep_time = LOG_WORKER_SLEEP_TIME);

// start logging to standard output
CLogHandler* start_logger(bool async = true, millisecond_t sleep_time = LOG_WORKER_SLEEP_TIME);

// start logging to file
CLogHandler* start_file_logger(const TCHAR *log_file_path,
                       bool append_mode = false,
                       int max_rotation = LOG_FILE_ROTATE_COUNT,
                       size_t rotate_size = LOG_FILE_ROTATE_SIZE,
					   bool async = true, millisecond_t sleep_time = LOG_WORKER_SLEEP_TIME
);

// start logging to string buffer
CLogHandler* start_string_logger(size_t buffer_size = LOG_STRING_SIZE,
								 bool async = true, millisecond_t sleep_time = LOG_WORKER_SLEEP_TIME);

// convert any value to primitive types that can be recognized by printf
// add overload functions to customize type conversion
template <class T>
struct log_save
{
	typedef T origType;
	origType v;
};

template <>
struct log_save<char*>
{
	std::string s;
};
template <>
struct log_save<wchar_t*>
{
	std::wstring s;
};
template<class T>
struct to_printable
{
	
	to_printable(const T&_v){

	}
};

template<class T>
struct to_printable<log_save<T>>
{

	const log_save<T>& v;
	to_printable(const log_save<T>& _v) :v(_v)
	{

	}
	const T  get() {
		return v.v;
	}
};
template<>
struct to_printable<log_save<wchar_t*>>
{

	const log_save<wchar_t*>& v;
	to_printable(const log_save<wchar_t*>& _v) :v(_v)
	{

	}
	const wchar_t*  get() {
		return v.s.c_str();
	}
};

template<>
struct to_printable<log_save<char*>>
{

	const log_save<char*>& v;
	to_printable(const log_save<char*>& _v) :v(_v)
	{

	}
	const char* get() {
		return v.s.c_str();
	}
};

template<>
struct to_printable<log_save<std::string>>
{

	const log_save<std::string>& v;
	to_printable(const log_save<std::string>& _v) :v(_v)
	{

	}
	const char* get() {
		return v.v.c_str();
	}
};

template<>
struct to_printable<log_save<std::wstring>>
{

	const log_save<std::wstring>& v;
	to_printable(const log_save<std::wstring>& _v) :v(_v)
	{

	}
	const wchar_t* get() {
		return v.v.c_str();
	}
};
template<>
struct to_printable<log_save<CString>>
{

	const log_save<CString>& v;
	to_printable(const log_save<CString>& _v) :v(_v)
	{

	}
	const TCHAR* get() {
		return v.v.GetString();
	}
};



template<class F,typename... Args>
int remove_arg1(
	F f,
	LPCTSTR _Format1,
	to_printable<LPCTSTR> _Format2,
	Args... args)
{
	
	return f(_Format1,args.get()...);
}
template<class F,typename A1,typename... Args>
int remove_arg2(
	F f,
	A1 a1,
	LPCTSTR _Format1,
	to_printable<LPCTSTR> _Format2,
	Args... args)
{
	
	return f(a1,_Format1,args.get()...);
}

template<class F,typename A1,typename A2,typename... Args>
int remove_arg3(
	F f,
	A1 a1,
	A2 a2,
	LPCTSTR _Format1,
	to_printable<LPCTSTR> _Format2,
	Args... args)
{
	
	return f(a1,a2,_Format1,args.get()...);
}
/*
inline const uint64_t to_printable(std::thread::id tid) {
	// if-constexpr
	constexpr bool is_size64 = sizeof(std::thread::id) == sizeof(uint64_t);
	if(is_size64)
		return *(uint64_t*)&tid;
	else {
		std::hash<std::thread::id> hasher;
		return hasher(tid);
	}
}
*/	


// --------------------------------
// END of interface declaration
// --------------------------------

struct alignas(CACHELINE_SIZE) Sequence {
	std::atomic<uint64_t> value;
	char _padding[CACHELINE_SIZE - sizeof(std::atomic<uint64_t>)];

	inline void set(uint64_t v) {
		value = v;
	}

	inline uint64_t get() const {
		return value;
	}

	inline uint64_t load() const {
		return value.load(std::memory_order::memory_order_acquire);
	}

	inline void store(uint64_t v) {
		value.store(v, std::memory_order::memory_order_release);
	}

	inline uint64_t fetch_add(uint64_t v) {
		return value.fetch_add(v, std::memory_order::memory_order_relaxed);
	}
};

void *aligned_alloc(size_t alignment, size_t size);
void aligned_free(void *ptr);

using LogEventTime = std::chrono::system_clock::time_point;

struct LogData;
typedef void (*LogWriter)(const LogData *log, void *context);

struct LogData
{
	int level;
	LogEventTime time;
	const TCHAR *channel;
	const LogWriter *writer;
};
	
struct LogEvent
{
	// 1 cacheline for shared publish flag
	Sequence publish;
	// 1 cacheline for data
	std::shared_ptr<void> data;
	char _padding[CACHELINE_SIZE - sizeof(std::shared_ptr<void>)];
};

template<class F, size_t... Is>
constexpr auto index_apply_impl(F f, std::index_sequence<Is...>) {
	return f(std::integral_constant<size_t, Is>{}...);
}

template<size_t N, class F>
constexpr auto index_apply(F f) {
	return index_apply_impl(f, std::make_index_sequence<N>{});
}
	
enum ELogWriterType
{
	LOG_WRITER_STDOUT,
	LOG_WRITER_FILE,
	LOG_WRITER_STRING,
	
	LOG_WRITER_COUNT
};

template<typename... Args>
struct GenericLogWriter {
//#pragma clang diagnostic push
//#pragma clang diagnostic ignored "-Wformat-security"

	static void write_stdout(const LogData *log, void *context) {
		using tuple_t = std::tuple<const TCHAR *, Args...>;
		constexpr size_t tuple_size = std::tuple_size<tuple_t>::value;
		auto t = reinterpret_cast<const tuple_t *>((const char *) log + sizeof(LogData));
		index_apply<tuple_size>([t](auto... Is) {
			remove_arg1(_tprintf,std::get<0>(*t),to_printable(std::get<Is>(*t))...);
		});
	}

	static void write_file(const LogData *log, void *context) {
		using tuple_t = std::tuple<const TCHAR *, Args...>;
		constexpr size_t tuple_size = std::tuple_size<tuple_t>::value;
		auto t = reinterpret_cast<const tuple_t *>((const char *) log + sizeof(LogData));
		auto c = (std::pair<FILE*, long>*)context;
		index_apply<tuple_size>([t, c](auto... Is) {
			c->second = remove_arg2(_ftprintf,c->first, std::get<0>(*t),to_printable(std::get<Is>(*t))...);
		});
	}

	static void write_string(const LogData *log, void *context) {
		using tuple_t = std::tuple<const TCHAR *, Args...>;
		constexpr size_t tuple_size = std::tuple_size<tuple_t>::value;
		auto t = reinterpret_cast<const tuple_t *>((const char *) log + sizeof(LogData));

		auto c = (std::tuple<TCHAR*, size_t,ULONG_PTR>*)context;
		index_apply<tuple_size>([t, c](auto... Is) {
			if (std::get<0>(*c) == nullptr)
			{
				std::get<1>(*c) = remove_arg1(_sctprintf,std::get<0>(*t),to_printable(std::get<Is>(*t))...);
			}
			else {
				std::get<1>(*c) = remove_arg3(_sntprintf,std::get<0>(*c), std::get<1>(*c), std::get<0>(*t),to_printable(std::get<Is>(*t))...);
			}
		});

	}
	
//#pragma clang diagnostic pop

	static const LogWriter* get_writer() {
		static const LogWriter s_writer_table[LOG_WRITER_COUNT] = {
			&write_stdout,
			&write_file,
			&write_string,
		};
		return s_writer_table;
	}
};

typedef bool (*LogFilter)(const LogData *);

class CLogger;

class CLogTimeFormatter {
public:
	virtual ~CLogTimeFormatter() {}

	virtual const TCHAR *format_time(LogEventTime t) = 0;
};

// time string format: "HH:MM:SS", 8 char
class CTimeFormatter : public CLogTimeFormatter {
public:
	virtual const TCHAR *format_time(LogEventTime t) override;

private:
	static constexpr unsigned MAX_LENGTH = 9;
	TCHAR m_buf[MAX_LENGTH];
};

// time string format: "HH:MM:SS.xxx", 12 char
class CMsTimeFormatter : public CLogTimeFormatter {
public:
	virtual const TCHAR *format_time(LogEventTime t) override;

private:
	static constexpr unsigned MAX_LENGTH = 13;
	TCHAR m_buf[MAX_LENGTH];
};

// time string format: "YYYY-MM-DD HH:MM:SS", 19 char
class CDateTimeFormatter : public CLogTimeFormatter {
public:
	virtual const TCHAR *format_time(LogEventTime t) override;

private:
	static constexpr unsigned MAX_LENGTH = 20;
	TCHAR m_buf[MAX_LENGTH];
};

class CLogHandler {
public:
	static void *operator new(size_t size) {
		return aligned_alloc(alignof(CLogHandler), size);
	}

	static void operator delete(void *ptr) {
		aligned_free(ptr);
	}

	CLogHandler();
	virtual ~CLogHandler();
	void process();
	virtual void flush();
	virtual void process_event(const LogData *data) {};
	virtual void on_close() {};

	inline void follow(CLogger *seq) {
		m_logger = seq;
	}

	inline uint64_t get_sequence() const {
		return m_seq.get();
	}

	inline uint64_t acquire_sequence() const {
		return m_seq.load();
	}

	inline void set_filter(LogFilter filter) {
		m_filter = filter;
	}

	inline bool is_closed() const {
		return m_closed;
	}

	inline void set_time_formatter(std::unique_ptr<CLogTimeFormatter> &&ptr) {
		m_formatter = std::move(ptr);
	}

protected:
	CLogger *m_logger;
	LogFilter m_filter;
	std::unique_ptr<CLogTimeFormatter> m_formatter;
	std::vector<std::shared_ptr<void>> m_batch;
	bool m_closed;
	Sequence m_seq;
};

class CLogStdout : public CLogHandler {
public:
	virtual void process_event(const LogData *log) override;
};

// platform dependence filesystem api
// It should be replaced by std::filesystem (C++17) if possible
class CLogFileSystem {
public:
#if defined(_WIN32) || defined(_WIN64)
	static constexpr char seperator = '\\';
	static constexpr char reversed_seperator = '/';
#else
	static constexpr char seperator = '/';
	static constexpr char reversed_seperator = '\\';
#endif

	static void normpath(std::tstring &path);

	static void split(const std::tstring &path, std::tstring &dir, std::tstring &file_name);

	static void split_ext(const std::tstring &file_name, std::tstring &base_name, std::tstring &ext);

	static bool isdir(const std::tstring &path);

	static bool isfile(const std::tstring &path);

	static bool makedirs(const std::tstring &path);
};

class CLogFile : public CLogHandler {
public:
	CLogFile(const TCHAR *filepath, bool append = false, int rotate_count = LOG_FILE_ROTATE_COUNT,
	         size_t rotate_size = LOG_FILE_ROTATE_SIZE);

	virtual ~CLogFile();

	virtual void flush() override;
	virtual void process_event(const LogData *log) override;
	virtual void on_close() override;

	inline bool is_ready() const {
		return m_hfile != nullptr;
	}

	inline const std::tstring &get_directory() const {
		return m_logpath;
	}

	inline const std::tstring &get_base_name() const {
		return m_logname;
	}

	inline const std::tstring &get_file_path() const {
		return m_curfile;
	}

	void rotate();

private:
	static FILE *_share_open(const TCHAR *path, const TCHAR *mode);

	FILE *m_hfile;
	std::tstring m_logpath;
	std::tstring m_logname;
	std::tstring m_curfile;
	size_t m_cur_size;
	size_t m_rotate_size;
	int m_rotate_count;
};
	
class CCustomLog : public CLogHandler
{
public:
	CCustomLog();
	virtual ~CCustomLog() {}
	virtual void process_event(const LogData *log) override;
	// get C string
	inline const TCHAR *str() const {
		return std::get<0>(m_str);
	}
	// get string size
	inline size_t size() const {
		return std::get<1>(m_str);
	}
	inline ULONG_PTR context() const {
		return std::get<2>(m_str);
	}
protected:
	virtual std::tuple<TCHAR*, size_t,ULONG_PTR> getstr(size_t required_size) = 0;
	virtual void setstr(size_t size,ULONG_PTR context) {}
	virtual void handle_error(const LogData *log);
	
	std::tuple<TCHAR*, size_t, ULONG_PTR> m_str;
};
	
class CLogString : public CCustomLog
{
public:
	CLogString(size_t init_size = LOG_STRING_SIZE);
	virtual ~CLogString();

protected:
	virtual std::tuple<TCHAR*, size_t, ULONG_PTR> getstr(size_t required_size) override;

private:
	TCHAR *m_buf;
	size_t m_capacity;
};
template<class T>
struct CLogTranser
{
	T v;

};
class CLogger {
public:
	CLogger(size_t buf_size);
	~CLogger();
	CLogger(const CLogger &) = delete;
	CLogger &operator=(const CLogger &) = delete;
	// notify all handlers to close
	void close();
	void add_handler(CLogHandler *handler);
	void remove_handler(CLogHandler *handler);
	// release all handlers
	// assume self own the handlers, and handlers are allocated by new operator
	void release_handlers();
	// send log message to handlers
	template<class... Args>
	bool write(int level, const TCHAR *channel, const TCHAR *format, Args... args) {
		using tuple_t = std::tuple<const TCHAR *, log_save<Args>...>;
		struct entry_t  {
			LogData base;
			tuple_t data;
		};
		auto sptr = std::make_shared<entry_t>();
		sptr->data = {format, args...};
		sptr->base.writer = GenericLogWriter<log_save<Args>...>::get_writer();
		_publish(level, channel, sptr);
		return true;
	}
	// send any data to handlers
	template<class T>
	bool write(int level, const TCHAR *channel, const T &obj) {
		struct entry_t {
			LogData base;
			T data;
		};
		auto sptr = std::make_shared<entry_t>();
		sptr->data = obj;
		sptr->base.writer = nullptr;
		_publish(level, channel, sptr);
		return true;
	}

	inline const LogEvent *get_event(uint64_t seq) const {
		return m_event_queue + (seq & m_size_mask);
	}

	inline LogEvent *get_event(uint64_t seq) {
		return m_event_queue + (seq & m_size_mask);
	}

	static void *operator new(size_t size) {
		return aligned_alloc(alignof(CLogger), size);
	}

	static void operator delete(void *ptr) {
		aligned_free(ptr);
	}

	static size_t get_next_power2(size_t val);
	void set_ActiveLevel(int n){
		m_activeLevel=n;
	}
private:
	uint64_t _claim(uint64_t count);
	void _publish(int level, const TCHAR *channel, std::shared_ptr<void> sptr);

	LogEvent *m_event_queue;
	size_t m_size_mask;
	int m_activeLevel;
	std::vector<CLogHandler *> m_handler_list;
	uint64_t m_min_seq;
	Sequence m_seq_claim;
};

#ifdef USE_NAMESPACE
}
#endif
#endif // NCLUDE_STB_LOG_H

#ifdef STB_LOG_IMPLEMENTATION

#include <emmintrin.h>
#include <sys/stat.h>
#include <cstdarg>
#include <ctime>
#include <string>
#include <algorithm>

#define ASSERT_ALIGNMENT(ptr, align) assert((ptr) && ((uintptr_t(ptr) % (align)) == 0))

#ifdef USE_NAMESPACE
namespace STB_LOG_NAMESPACE {
#endif

inline LogContext* add_log_handler(CLogHandler *handler)
{
	LogContext *lc = get_log_context();
	if (!lc->logger)
		lc->logger = new CLogger(LOG_QUEUE_SIZE);
	lc->logger->add_handler(handler);
	return lc;
}

void start_handler_thread(CLogHandler *handler, millisecond_t sleep_time) {
	LogContext *lc = add_log_handler(handler);
	std::chrono::milliseconds msec(sleep_time);
	lc->thread_pool.emplace_back(std::make_unique<std::thread>([handler, msec] {
		void SetThreadName( const char* threadName);
		SetThreadName("stb_log");
		while (!handler->is_closed()) {
			handler->process();
			std::this_thread::sleep_for(msec);
		}
	}));
}

void close_logger() {
	LogContext *lc = get_log_context();
	if (!lc->logger)
		return;
	lc->logger->close();
	for (auto &th : lc->thread_pool) {
		th->join();
	}
	lc->thread_pool.clear();
	lc->logger->release_handlers();
	delete lc->logger;
	lc->logger = nullptr;
}

CLogHandler* start_logger(bool async, millisecond_t sleep_time) {
	CLogStdout *handler = new CLogStdout();
	handler->set_time_formatter(std::make_unique<CDateTimeFormatter>());
	if(async)
		start_handler_thread(handler, sleep_time);
	else
		add_log_handler(handler);
	return handler;
}

CLogHandler* start_file_logger(const TCHAR *log_file_path, bool append_mode, int max_rotation,
					   size_t rotate_size, bool async, millisecond_t sleep_time) {
	CLogFile *handler = new CLogFile(log_file_path, append_mode, max_rotation, rotate_size);
	handler->set_time_formatter(std::make_unique<CDateTimeFormatter>());
	if(async)
		start_handler_thread(handler, sleep_time);
	else
		add_log_handler(handler);
	return handler;
}
	
CLogHandler* start_string_logger(size_t buffer_size, bool async, millisecond_t sleep_time)
{
	CLogString *handler = new CLogString(buffer_size);
	handler->set_time_formatter(std::make_unique<CDateTimeFormatter>());
	if(async)
		start_handler_thread(handler, sleep_time);
	else
		add_log_handler(handler);
	return handler;
}

void *aligned_alloc(size_t alignment, size_t size) {
	// [Memory returned][ptr to start of memory][aligned memory][extra memory]
	size_t request_size = size + alignment;
	void *raw = malloc(request_size + sizeof(void *));
	if (!raw)
		return nullptr;
	void *ptr = (void **) raw + 1;
	ptr = std::align(alignment, size, ptr, request_size);
	if (!ptr) {
		free(raw);
		return nullptr;
	}
	*((void **) ptr - 1) = raw;
	return ptr;
}

void aligned_free(void *ptr) {
	void *raw = *((void **) ptr - 1);
	free(raw);
}

size_t CLogger::get_next_power2(size_t val) {
	// val maybe power of 2
	--val;
	// set the bits right of MSB to 1
	val |= (val >> 1);
	val |= (val >> 2);
	val |= (val >> 4);
	val |= (val >> 8);        /* Ok, since int >= 16 bits */
#if (SIZE_MAX != 0xffff)
	val |= (val >> 16);        /* For 32 bit int systems */
#if (SIZE_MAX > 0xffffffffUL)
	val |= (val >> 32);        /* For 64 bit int systems */
#endif // SIZE_MAX != 0xffff
#endif // SIZE_MAX > 0xffffffffUL
	++val;
	assert((val & (val - 1)) == 0);
	return val;
}

// --------------------------------
// CLogger implementation
// --------------------------------
	
CLogger::CLogger(size_t size) {
	assert(size > 0);
	if (size & (size - 1))
		size = get_next_power2(size);
	static_assert(sizeof(LogEvent) % CACHELINE_SIZE == 0, "LogEvent should be fit in cacheline.");
	size_t buf_size = sizeof(LogEvent) * size;
	m_event_queue = (LogEvent *) aligned_alloc(CACHELINE_SIZE, buf_size);
	ASSERT_ALIGNMENT(m_event_queue, CACHELINE_SIZE);
	m_size_mask = size - 1;
	for (size_t i = 0; i < size; ++i) {
		LogEvent *log = &m_event_queue[i];
		ASSERT_ALIGNMENT(log, CACHELINE_SIZE);
		ASSERT_ALIGNMENT(&log->publish, CACHELINE_SIZE);
		// initialize LogEvent
		new(log) LogEvent;
		log->publish.set(0);
	}
	m_seq_claim.set(0);
	m_min_seq = 0;
	m_activeLevel=LOG_SEVERITY_LEVEL;
}

CLogger::~CLogger() {
	for (auto handler : m_handler_list) {
		handler->follow(nullptr);
	}
	for (size_t i = 0; i <= m_size_mask; ++i) {
		// clean up LogEvent
		LogEvent *log = &m_event_queue[i];
		log->~LogEvent();
	}
	aligned_free(m_event_queue);
	m_event_queue = nullptr;
}


uint64_t CLogger::_claim(uint64_t count) {
	uint64_t request_seq = m_seq_claim.fetch_add(count);
	if (request_seq <= m_min_seq + m_size_mask) {
		return request_seq;
	}
	uint64_t min_seq = ULLONG_MAX, seq = 0;
	constexpr unsigned YIELD = 1;
	constexpr unsigned SLEEP = 2;
	constexpr unsigned SPIN = 10;
	unsigned spin_count = 0;
	unsigned loop_count = 0;
	// millisecond_t ms = 1;
	// TODO: need a better spin lock
	for (CLogHandler *handler : m_handler_list) {
		seq = handler->get_sequence();
		while (request_seq > seq + m_size_mask) {
			if(loop_count < YIELD) {
				for (spin_count = SPIN; spin_count > 0; --spin_count)
					_mm_pause(); // pause, about 12ns
			}
			else {
				unsigned yield_count = loop_count - YIELD;
				if(SLEEP > 0 && (yield_count % SLEEP == SLEEP - 1)) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					loop_count = 0;
				}
				else {
					// if no waiting threads, about 113ns
					// else lead to thread switching
					std::this_thread::yield();
				}
			}
			++loop_count;
			seq = handler->get_sequence();
		}
		seq = handler->acquire_sequence();
		if (seq < min_seq)
			min_seq = seq;
	}
	m_min_seq = min_seq;
	return request_seq;
}

void CLogger::_publish(int level, const TCHAR *channel, std::shared_ptr<void> sptr) {
	LogData *base = (LogData*)sptr.get();
	if(level>=m_activeLevel) {
		base->level = level;
		base->channel = channel;
		base->time = std::chrono::system_clock::now();
		auto seq = _claim(1);
		auto *log = get_event(seq);
		log->data = sptr;
		log->publish.store(seq + 1);
	}
}

void CLogger::add_handler(CLogHandler *handler) {
	m_handler_list.push_back(handler);
	handler->follow(this);
}

void CLogger::remove_handler(CLogHandler *handler) {
	for (auto iter = m_handler_list.begin(); iter != m_handler_list.end(); ++iter) {
		if (*iter == handler) {
			m_handler_list.erase(iter);
			handler->follow(nullptr);
			return;
		}
	}
}

void CLogger::release_handlers() {
	for (auto iter: m_handler_list) {
		iter->follow(nullptr);
		delete iter;
	}
	m_handler_list.clear();
}

void CLogger::close() {
	uint64_t seq = _claim(1);
	LogEvent *log = get_event(seq);
	log->data = nullptr;
	log->publish.store(seq + 1);
}

// --------------------------------
// CLogHandler implementation
// --------------------------------

CLogHandler::CLogHandler()
		: m_logger(nullptr), m_filter(nullptr), m_formatter(nullptr), m_closed(false)
{
	ASSERT_ALIGNMENT(this, CACHELINE_SIZE);
	ASSERT_ALIGNMENT(&m_seq, CACHELINE_SIZE);
	m_seq.set(0);
	m_batch.reserve(LOG_BATCH_SIZE);
}

CLogHandler::~CLogHandler() {
	if (m_logger) {
		m_logger->remove_handler(this);
		m_logger = nullptr;
	}
}

void CLogHandler::process() {
	assert(m_logger);
	uint64_t seq = m_seq.get(), pub;
	LogEvent *log;
	LogData *data;
	while (!m_closed) {
		log = m_logger->get_event(seq);
		pub = log->publish.load();
		if (pub <= seq)
			break;
		data = (LogData*)(log->data.get());
		if (!data) {
			m_closed = true;
		} else if (!m_filter or m_filter(data)) {
			m_batch.push_back(log->data);
		}
		m_seq.store(pub);
		seq += 1;
		assert(pub == seq);
		if (m_batch.size() >= LOG_BATCH_SIZE)
			flush();
	}
	if(!m_batch.empty())
		flush();
	if (m_closed)
		on_close();
}
	
void CLogHandler::flush()
{
	for (auto &iter: m_batch) {
		process_event((LogData*)iter.get());
	}
	m_batch.clear();
}

// --------------------------------
// CLogTimeFormatter implementation
// --------------------------------
#if defined(WIN32) || defined(WIN64)
#define LOCALTIME(datetime, timestam) localtime_s(&(datetime), &(timestamp))
#define S_IFDIR _S_IFDIR
#define S_IFREG _S_IFREG
#else
#define LOCALTIME(datetime, timestamp) localtime_r(&(timestamp), &(datetime))
#endif

const TCHAR *CTimeFormatter::format_time(LogEventTime t) {
	time_t timestamp = std::chrono::system_clock::to_time_t(t);
	tm datetime;
	LOCALTIME(datetime, timestamp);
	if (_tcsftime(m_buf, MAX_LENGTH, _T("%T"), &datetime) == 0) {
		m_buf[MAX_LENGTH - 1] = 0;
	}
	return m_buf;
}

const TCHAR *CMsTimeFormatter::format_time(LogEventTime t) {
	time_t timestamp = std::chrono::system_clock::to_time_t(t);
	tm datetime;
	LOCALTIME(datetime, timestamp);
	auto len = _tcsftime(m_buf, MAX_LENGTH, _T("%T"), &datetime);
	if (len == 0) {
		m_buf[MAX_LENGTH - 1] = 0;
		return m_buf;
	}
	auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch());
	_stprintf(m_buf + len, _T(".%03d"), int(msec.count() % 1000));
	return m_buf;
}

const TCHAR *CDateTimeFormatter::format_time(LogEventTime t) {
	time_t timestamp = std::chrono::system_clock::to_time_t(t);
	tm datetime;
	LOCALTIME(datetime, timestamp);
	if (_tcsftime(m_buf, MAX_LENGTH, _T("%F %T"), &datetime) == 0) {
		m_buf[MAX_LENGTH - 1] = 0;
	}
	return m_buf;
}

// --------------------------------
// Standard logger implementation
// --------------------------------

void CLogStdout::process_event(const LogData *log) {
	if (m_formatter) {
		const TCHAR *stime = m_formatter->format_time(log->time);
		_tprintf(_T("[%s] "), stime);
	}
	if (log->channel[0] != 0) {
		_tprintf(_T("[%s] "), log->channel);
	}
	log->writer[LOG_WRITER_STDOUT](log, nullptr);
	_tprintf(_T("\n"));
}

// --------------------------------
// File system implementation
// --------------------------------

void CLogFileSystem::normpath(std::tstring &path) {
	std::replace(path.begin(), path.end(), char(reversed_seperator), char(seperator));
}

void CLogFileSystem::split(const std::tstring &path, std::tstring &dir, std::tstring &file_name) {
	size_t pos = path.rfind(seperator);
	if (pos != std::tstring::npos) {
		pos += 1;
		dir = path.substr(0, pos);
		file_name = path.substr(pos);
	} else {
		dir = _T("");
		file_name = path;
	}
}

void CLogFileSystem::split_ext(const std::tstring &file_name, std::tstring &base_name, std::tstring &ext) {
	size_t pos = file_name.rfind('.');
	if (pos != std::tstring::npos) {
		base_name = file_name.substr(0, pos);
		ext = file_name.substr(pos);
	} else {
		base_name = file_name;
		ext = _T("");
	}
}

bool CLogFileSystem::isdir(const std::tstring &path) {
	struct _stat path_st;
	return _tstat(path.c_str(), &path_st) == 0 && path_st.st_mode & S_IFDIR;
}

bool CLogFileSystem::isfile(const std::tstring &path) {
	struct _stat path_st;
	return _tstat(path.c_str(), &path_st) == 0 && path_st.st_mode & S_IFREG;
}

bool CLogFileSystem::makedirs(const std::tstring &path) {
	std::tstring cmd = _T("mkdir ");
	cmd += path;
	return _tsystem(cmd.c_str()) == 0;
}

// --------------------------------
// File logger implementation
// --------------------------------

FILE *CLogFile::_share_open(const TCHAR *path, const TCHAR *mode) {
#if defined(_WIN32) || defined(_WIN64)
	return _tfsopen(path, mode, _SH_DENYWR);
#else
	return fopen(path, mode);
#endif
}

CLogFile::CLogFile(const TCHAR *filepath, bool append, int rotate_count, size_t rotate_size)
	: m_hfile(nullptr)
	, m_curfile(filepath)
	, m_cur_size(0)
	, m_rotate_size(rotate_size)
	, m_rotate_count(rotate_count)
{
	CLogFileSystem::normpath(m_curfile);
	CLogFileSystem::split(m_curfile, m_logpath, m_logname);
	if (!CLogFileSystem::isdir(m_logpath) && !CLogFileSystem::makedirs(m_logpath)) {
		_tprintf(_T("Fail to create log director [%s]\n"), m_logpath.c_str());
		m_logpath = _T("");
	}
	if (append)
		m_hfile = _share_open(m_curfile.c_str(), _T("a"));
	else if (CLogFileSystem::isfile(m_curfile))
		rotate();
	else
		m_hfile = _share_open(m_curfile.c_str(), _T("w"));
}

CLogFile::~CLogFile() {
	if (m_hfile) {
		fclose(m_hfile);
		m_hfile = 0;
	}
}
	
void CLogFile::flush()
{
	CLogHandler::flush();
	fflush(m_hfile);
}

void CLogFile::process_event(const LogData *log) {
	if (m_formatter) {
		const TCHAR *stime = m_formatter->format_time(log->time);
		m_cur_size += _ftprintf(m_hfile, _T("[%s] "), stime);
	}
	if (log->channel[0] != 0) {
		m_cur_size += _ftprintf(m_hfile, _T("[%s] "), log->channel);
	}
	std::pair<FILE*, long> context{m_hfile, 0};
	log->writer[LOG_WRITER_FILE](log, &context);
	m_cur_size += context.second + 1;
	_ftprintf(m_hfile, _T("\n"));
	if (m_cur_size >= m_rotate_size) {
		rotate();
	}
}

void CLogFile::on_close() {
	if (m_hfile) {
		fclose(m_hfile);
		m_hfile = 0;
	}
}

void CLogFile::rotate() {
	if (m_rotate_count < 1)
		return;
	if (m_hfile) {
		fclose(m_hfile);
		m_hfile = 0;
	}
	std::tstring logfile, ext;
	CLogFileSystem::split_ext(m_curfile, logfile, ext);
	std::tstring last_file = logfile, cur_file;
	last_file += std::to_tstring(m_rotate_count);
	last_file += ext;
	if (CLogFileSystem::isfile(last_file)) {
		if (_tremove(last_file.c_str()) != 0)
			return;
	}
	for (int i = m_rotate_count - 1; i >= 0; --i) {
		cur_file = logfile;
		cur_file += std::to_tstring(i);
		cur_file += ext;
		if (CLogFileSystem::isfile(cur_file))
			_trename(cur_file.c_str(), last_file.c_str());
		last_file = cur_file;
	}
	_trename(m_curfile.c_str(), last_file.c_str());
	m_hfile = _share_open(m_curfile.c_str(), _T("w"));
}

// --------------------------------
// String logger implementation
// --------------------------------

CCustomLog::CCustomLog()
	: m_str(nullptr, 0,0)
{
	
}
	
void CCustomLog::process_event(const LogData *log)
{
	size_t capacity;
	
	std::tuple<TCHAR*, size_t, ULONG_PTR> strTmp(nullptr,0,0);
	
	log->writer[LOG_WRITER_STRING](log, &strTmp);
	if(std::get<1>(strTmp) < 0) {
		goto ERROR_EXIT;
	}
	if (m_formatter) {
		const TCHAR* stime = m_formatter->format_time(log->time);
		std::get<1>(strTmp) += _sntprintf(nullptr,0,_T("[%s] "), stime);
	}
	if (log->channel[0] != 0) {
		std::get<1>(strTmp) += _sntprintf(nullptr,0,_T("[%s] "), log->channel);
	}
	//if(std::get<1>(m_str) >= capacity) 
	{
		
		m_str = getstr(std::get<1>(strTmp) + 1);
		if(!std::get<0>(m_str)) {
			goto ERROR_EXIT;
		}
		capacity = std::get<1>(m_str);
		int offset = 0;
		if (m_formatter) {
			const TCHAR* stime = m_formatter->format_time(log->time);
			offset+= _sntprintf(std::get<0>(m_str)+offset, capacity-offset, _T("[%s] "), stime);
		}
		if (log->channel[0] != 0) {
			offset += _sntprintf(std::get<0>(m_str) + offset, capacity - offset, _T("[%s] "), log->channel);
		}
		
		std::get<0>(strTmp) = std::get<0>(m_str) + offset;
		std::get<1>(strTmp) = std::get<1>(m_str) - offset;
		log->writer[LOG_WRITER_STRING](log, &strTmp);
		if(std::get<1>(strTmp)< 0) {
			goto ERROR_EXIT;
		}

		// if still not enough, strip the string
		//if(std::get<1>(m_str) >= capacity) {
		//	std::get<1>(m_str) = capacity - 1;
		//	std::get<0>(m_str)[std::get<1>(m_str)] = 0;
		//}
		//std::get<1>(m_str) += std::get<1>(strTmp);
	}
	// done with string buffer
	setstr(std::get<1>(m_str) + 1, std::get<2>(m_str));
	return;
	
ERROR_EXIT:
	// encounter error
	handle_error(log);
	return;
}

void CCustomLog::handle_error(const LogData *log)
{
	printf("Fail to write log.\n");
}


CLogString::CLogString(size_t init_size)
	: CCustomLog()
	, m_buf(nullptr)
	, m_capacity(init_size)
{
	if(init_size < 1)
		init_size = 1;
	m_buf = new TCHAR[init_size];
	m_buf[0] = 0;
}

CLogString::~CLogString()
{
	if(m_buf) {
		delete [] m_buf;
		m_buf = nullptr;
	}
}

std::tuple<TCHAR*, size_t, ULONG_PTR> CLogString::getstr(size_t required_size)
{
	if(required_size > m_capacity) {
		size_t size = m_capacity;
		while(size < required_size) {
			size *= 2;
		}
		if(size >= LOG_STRING_SIZE_MAX)
			size = LOG_STRING_SIZE_MAX;
		if(m_buf)
			delete [] m_buf;
		m_buf = new TCHAR[size];
		m_capacity = size;
	}
	return {m_buf, m_capacity,0};
}

#ifdef USE_NAMESPACE
}
#endif
#endif // STB_LOG_IMPLEMENTATION
