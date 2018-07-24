/* stb_log - v0.0.1
   
*/

#ifndef INCLUDE_STB_LOG_H
#define INCLUDE_STB_LOG_H

#include <cassert>
#include <vector>
#include <atomic>
#include <chrono>

#ifndef NAMESPACE_NAME
#define NAMESPACE_NAME wyc
#endif

#ifdef USE_NAMESPACE
namespace NAMESPACE_NAME {
#endif

	enum StbLogLevel
	{
		CRITICAL = 50,
		ERROR = 40,
		WARNING = 30,
		INFO = 20,
		DEBUG = 10,
		NOTSET = 0,
		// negative values are reserved for internal ctrl code
		CLOSE = -1,  
	};

#ifndef CACHELINE_SIZE
	#define CACHELINE_SIZE 64
#endif
#define ASSERT_ALIGNMENT(ptr, align) assert((uintptr_t(ptr) % (align)) == 0)

	struct alignas(CACHELINE_SIZE) Sequence
	{
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

	void *aligned_alloc(size_t alignment, size_t size) 
	{
		// [Memory returned][ptr to start of memory][aligned memory][extra memory]
		size_t request_size = size + alignment ;
		void *raw = malloc(request_size + sizeof(void*));
		if (!raw)
			return nullptr;
		void *ptr = (void**)raw + 1;
		ptr = std::align(alignment, size, ptr, request_size);
		if (!ptr) {
			free(raw);
			return nullptr;
		}
		*((void**)ptr - 1) = raw;
		return ptr;
	}

	void aligned_free(void *ptr) 
	{
		void *raw = *((void**)ptr - 1);
		free(raw);
	}

	using LogClock = std::chrono::system_clock;
	using LogEventTime = LogClock::time_point;

	struct LogEvent
	{
		int level;
		unsigned capacity;
		char channel[16];
		LogEventTime time;
		union {
			void *buffer;
			char fixed_buffer[1];
		};
		Sequence publish;
	};
	constexpr unsigned log_event_fixed_buffer_size = offsetof(LogEvent, publish) - offsetof(LogEvent, buffer);

	typedef bool (*LogFilter)(const LogEvent*);

	class CLogger;

	class CLogHandler
	{
	public:
		static void* operator new(size_t size) {
			return aligned_alloc(alignof(CLogHandler), size);
		}
		static void operator delete(void *ptr) {
			aligned_free(ptr);
		}

		CLogHandler();
		virtual ~CLogHandler();
		void process();
		virtual void process_event(const LogEvent *log) {};

		inline void follow(CLogger *seq) {
			m_logger = seq;
		}
		inline uint64_t get_sequence() const {
			return m_seq.get();
		}
		inline uint64_t acquire_sequence() const {
			return m_seq.load();
		}
		inline void add_filter(LogFilter filter) {
			m_filters.push_back(filter);
		}
		inline bool is_closed() const {
			return m_closed;
		}

	private:
		CLogger* m_logger;
		std::vector<LogFilter> m_filters;
		bool m_closed;
		Sequence m_seq;
	};

	class CLogStdout : public CLogHandler 
	{
	public:
		virtual void process_event(const LogEvent *log) override;
	};

	class CLogFile : public CLogHandler
	{
	public:
		virtual void process_event(const LogEvent *log) override;
	};

#if defined(_WIN32) || defined(_WIN64)
#ifdef _MSC_VER 
	class CLogDebugWindow : public CLogHandler
	{
	public:
		virtual void process_event(const LogEvent *log) override;
	};
#endif
#endif

	class CLogger
	{
	public:
		CLogger(size_t buf_size);
		~CLogger();
		CLogger(const CLogger&) = delete;
		CLogger& operator = (const CLogger&) = delete;
		void write(int level, const void *data = 0, size_t size = 0);
		void write(int level, const char* channel, const char *format, ...);
		void add_handler(CLogHandler *handler);
		void remove_handler(CLogHandler *handler);
		inline void close() {
			write((int)StbLogLevel::CLOSE);
		}
		inline const LogEvent* get_event(uint64_t seq) const {
			return m_event_queue + (seq & m_size_mask);
		}
		inline LogEvent* get_event(uint64_t seq) {
			return m_event_queue + (seq & m_size_mask);
		}
		
		static void* operator new(size_t size) {
			return aligned_alloc(alignof(CLogger), size);
		}
		static void operator delete(void *ptr) {
			aligned_free(ptr);
		}
		static size_t get_next_power2(size_t size);
		static char* ensure_buffer(LogEvent *log, size_t size);

	private:
		uint64_t _claim(uint64_t count);
		
		LogEvent * m_event_queue;
		unsigned m_size_mask;
		std::vector<CLogHandler*> m_handler_list;
		uint64_t m_min_seq;
		Sequence m_seq_claim;
	};

#ifdef USE_NAMESPACE
}
#endif
#endif // NCLUDE_STB_LOG_H

#ifdef STB_LOG_IMPLEMENTATION

#include <stdarg.h>
#include <emmintrin.h>
#include <thread>
#include <new>
#include <ctime>

#define LOG_EVENT_BUFFER(log) (char*)(((log)->capacity <= log_event_fixed_buffer_size) ? ((log)->fixed_buffer) : ((log)->buffer))

#ifdef USE_NAMESPACE
namespace NAMESPACE_NAME {
#endif

	size_t CLogger::get_next_power2(size_t val)
	{
		// val maybe power of 2
		--val;
		// set the bits right of MSB to 1
		val |= (val >> 1);
		val |= (val >> 2);
		val |= (val >> 4);
		val |= (val >> 8);		/* Ok, since int >= 16 bits */
#if (SIZE_MAX != 0xffff)
		val |= (val >> 16);		/* For 32 bit int systems */
#if (SIZE_MAX > 0xffffffffUL)
		val |= (val >> 32);		/* For 64 bit int systems */
#endif // SIZE_MAX != 0xffff
#endif // SIZE_MAX > 0xffffffffUL
		++val;
		assert((val & (val - 1)) == 0);
		return val;
	}

	// --------------------------------
	// CLogger implementation
	// --------------------------------

	CLogger::CLogger(size_t size)
	{
		assert(size > 0);
		if (size & (size - 1)) 
			size = get_next_power2(size);
		assert(sizeof(LogEvent) % CACHELINE_SIZE == 0);
		size_t buf_size = sizeof(LogEvent) * size;
		m_event_queue = (LogEvent*)aligned_alloc(CACHELINE_SIZE, buf_size);
		assert(m_event_queue);
		m_size_mask = size - 1;
		for (size_t i = 0; i < size; ++i) {
			LogEvent *log = &m_event_queue[i];
			ASSERT_ALIGNMENT(log, CACHELINE_SIZE);
			ASSERT_ALIGNMENT(&log->publish, CACHELINE_SIZE);
			// initialize LogEvent
			log->capacity = log_event_fixed_buffer_size;
			log->publish.set(0);
		}
		m_seq_claim.set(0);
		m_min_seq = 0;
	}

	CLogger::~CLogger()
	{
		for (auto handler : m_handler_list)
		{
			handler->follow(nullptr);
		}
		for (size_t i = 0; i <= m_size_mask; ++i) {
			// clean up LogEvent
			LogEvent *log = &m_event_queue[i];
			if (log->capacity > log_event_fixed_buffer_size) {
				delete[] log->buffer;
			}
		}
		aligned_free(m_event_queue);
		m_event_queue = nullptr;
	}

	void CLogger::write(int level, const void *data, size_t size)
	{
		uint64_t seq = _claim(1);
		// write header
		LogEvent *log = get_event(seq);
		log->level = level;
		log->channel[0] = 0;
		// write data
		if (data && size > 0) {
			char *buf = ensure_buffer(log, size);
			memcpy(buf, data, size);
		}
		// publish event
		log->publish.store(seq + 1);
	}

	void CLogger::write(int level, const char * channel, const char * format, ...)
	{
		va_list args;
		va_start(args, format);
		int length = vsnprintf(0, 0, format, args);
		va_end(args);
		if (length <= 0)
			return;
		uint64_t seq = _claim(1);
		// write header
		LogEvent *log = get_event(seq);
		log->level = level;
		log->time = LogClock::now();
		constexpr unsigned channel_size = sizeof(log->channel) - 1;
		if (strlen(channel) <= channel_size)
			strcpy(log->channel, channel);
		else {
			strncpy(log->channel, channel, channel_size);
			log->channel[channel_size] = 0;
		}
		// write data
		char *buf = ensure_buffer(log, length + 1);
		va_start(args, format);
		length = vsnprintf(buf, log->capacity, format, args);
		va_end(args);
		if (length == -1) {
			log->level = StbLogLevel::ERROR;
			const char *err = "Logging fail";
			buf = ensure_buffer(log, strlen(err) + 1);
			strcpy(buf, err);
		}
		// publish event
		log->publish.store(seq + 1);
	}

	uint64_t CLogger::_claim(uint64_t count) 
	{
		uint64_t request_seq = m_seq_claim.fetch_add(count);
		if (request_seq < m_min_seq)
			return request_seq;
		uint64_t min_seq = ULLONG_MAX, seq = 0;
		for (CLogHandler *handler : m_handler_list) {
			seq = handler->get_sequence();
			while (request_seq > seq + m_size_mask) {
				_mm_pause(); // pause, about 12ns
				seq = handler->get_sequence();
				if (request_seq <= seq + m_size_mask)
					break;
				// if no waiting threads, about 113ns
				// else lead to thread switching
				std::this_thread::yield();
				seq = handler->get_sequence();
			}
			seq = handler->acquire_sequence();
			if (seq < min_seq)
				min_seq = seq;
		}
		m_min_seq = seq;
		return request_seq;
	}

	char * CLogger::ensure_buffer(LogEvent * log, size_t size)
	{
		if (log->capacity < size) {
			if (log->capacity > log_event_fixed_buffer_size) {
				delete[] log->buffer;
			}
			if (size < 256 && (size & (size - 1)) != 0) {
				size = get_next_power2(size);
			}
			log->buffer = new char[size];
			log->capacity = size;
		}
		return log->capacity > log_event_fixed_buffer_size ? (char*)log->buffer : log->fixed_buffer;
	}

	void CLogger::add_handler(CLogHandler * handler)
	{
		m_handler_list.push_back(handler);
		handler->follow(this);
	}

	void CLogger::remove_handler(CLogHandler * handler)
	{
		for (auto iter = m_handler_list.begin(); iter != m_handler_list.end(); ++iter) {
			if (*iter == handler)
			{
				m_handler_list.erase(iter);
				handler->follow(nullptr);
				return;
			}
		}
	}

	// --------------------------------
	// CLogHandler implementation
	// --------------------------------

	CLogHandler::CLogHandler()
		: m_logger(nullptr)
		, m_closed(false)
	{
		ASSERT_ALIGNMENT(this, CACHELINE_SIZE);
		ASSERT_ALIGNMENT(&m_seq, CACHELINE_SIZE);
		m_seq.set(0);
	}

	CLogHandler::~CLogHandler()
	{
		if (m_logger) {
			m_logger->remove_handler(this);
		}
	}
	
	void CLogHandler::process() 
	{
		assert(m_logger);
		uint64_t seq = m_seq.get(), pub;
		const LogEvent *log;
		while (!m_closed) {
			log = m_logger->get_event(seq);
			pub = log->publish.load();
			if (pub > seq) {
				if (log->level == StbLogLevel::CLOSE) {
					// process system event
					m_closed = true;
				}
				else { // process user event
					bool b = true;
					for (auto filter : m_filters) {
						if (!filter(log)) {
							b = false;
							break;
						}
					}
					if (b)
						process_event(log);
				}
				m_seq.store(pub);
				seq += 1;
				assert(pub == seq);
				continue;
			}
			break;
		}
	}

	// --------------------------------
	// Event handlers
	// --------------------------------

	void CLogStdout::process_event(const LogEvent * log)
	{
		constexpr unsigned MAX_LENGTH = 26;
		time_t timestamp = std::chrono::system_clock::to_time_t(log->time);
		tm datetime;
		localtime_s(&datetime, &timestamp);
		char stime[MAX_LENGTH];
		int slen = 0;
		// time string in the format "[YYYY-MM-DD HH:MM:SS]"
		auto cnt = strftime(stime, MAX_LENGTH, "%F %T", &datetime);
		if (cnt > 0)
			slen += cnt;
		else
			stime[0] = 0;
		assert(slen < MAX_LENGTH);
		auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(log->time.time_since_epoch());
		cnt = sprintf(stime + slen, ".%03u", int(msec.count() % 1000));
		if (cnt > 0)
			slen += cnt;
		else
			stime[slen] = 0;
		assert(slen < MAX_LENGTH);
		const char *message = LOG_EVENT_BUFFER(log);
		printf("[%s] [%s] %s\n", stime, log->channel, message);
	}

#ifdef USE_NAMESPACE
}
#endif
#endif // STB_LOG_IMPLEMENTATION
