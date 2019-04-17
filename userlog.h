#ifndef _USERLOG_H_
#define _USERLOG_H_

#define LOG_TRACE(...) printf( __VA_ARGS__ )
#define LOG_DEBUG(...) printf( __VA_ARGS__ )
#define LOG_INFO(...) printf( __VA_ARGS__ )
#define LOG_WARN(...) printf( __VA_ARGS__ )
#define LOG_ERROR(...) printf( __VA_ARGS__ )
#define LOG_CRITICAL(...) printf( __VA_ARGS__ )

#endif
