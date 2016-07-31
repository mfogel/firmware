/*
 * Copyright (c) 2016 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SPARK_WIRING_LOGGING_H
#define SPARK_WIRING_LOGGING_H

#include <cstring>
#include <cstdarg>

#include "logging.h"
#include "system_control.h"

#include "spark_wiring_json.h"
#include "spark_wiring_print.h"
#include "spark_wiring_string.h"
#include "spark_wiring_thread.h"
#include "spark_wiring_array.h"

namespace spark {

class LogCategoryFilter {
public:
    LogCategoryFilter(String category, LogLevel level);
    LogCategoryFilter(const char *category, LogLevel level);
    LogCategoryFilter(const char *category, size_t length, LogLevel level);

    const char* category() const;
    LogLevel level() const;

private:
    String cat_;
    LogLevel level_;

    friend class LogFilter;
};

typedef Array<LogCategoryFilter> LogCategoryFilters;

// Internal implementation
class LogFilter {
public:
    explicit LogFilter(LogLevel level);
    LogFilter(LogLevel level, LogCategoryFilters filters);
    ~LogFilter();

    LogLevel level() const;
    LogLevel level(const char *category) const;

    // This class in non-copyable
    LogFilter(const LogFilter&) = delete;
    LogFilter& operator=(const LogFilter&) = delete;

private:
    struct Node;

    Array<String> cats_; // Category filter strings
    Array<Node> nodes_; // Lookup table
    LogLevel level_; // Default level

    static int nodeIndex(const Array<Node> &nodes, const char *name, size_t size, bool &found);
};

/*!
    \brief Abstract log handler.

    This class can be subclassed to implement custom log handlers. Log handlers act as "sinks"
    for the overall logging output generated by the system and application modules, and may have
    individual filtering settings.

    The library also provides several built-in handlers, such as \ref spark::SerialLogHandler and
    \ref spark::Serial1LogHandler.
*/
class LogHandler {
public:
    explicit LogHandler(LogLevel level = LOG_LEVEL_INFO);
    /*!
        \brief Constructor.
        \param level Default logging level.
        \param filters Category filters.
    */
    LogHandler(LogLevel level, LogCategoryFilters filters);
    /*!
        \brief Destructor.
    */
    virtual ~LogHandler() = default;
    /*!
        \brief Returns default logging level.
    */
    LogLevel level() const;
    /*!
        \brief Returns logging level enabled for specified category.
        \param category Category name.
    */
    LogLevel level(const char *category) const;
    /*!
        \brief Returns level name.
        \param level Logging level.
    */
    static const char* levelName(LogLevel level);

    // These methods are called by the LogManager
    void message(const char *msg, LogLevel level, const char *category, const LogAttributes &attr);
    void write(const char *data, size_t size, LogLevel level, const char *category);

    // This class is non-copyable
    LogHandler(const LogHandler&) = delete;
    LogHandler& operator=(const LogHandler&) = delete;

protected:
    /*!
        \brief Performs processing of a log message.
        \param msg Text message.
        \param level Logging level.
        \param category Category name (can be null).
        \param attr Message attributes.

        This method should be implemented by all subclasses.
    */
    virtual void logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) = 0;
    /*!
        \brief Writes character buffer to output stream.
        \param data Buffer.
        \param size Buffer size.

        Default implementation does nothing.
    */
    virtual void write(const char *data, size_t size);

private:
    LogFilter filter_;
};

/*!
    \brief Stream-based log handler.

    Adapter class allowing to use existent stream objects as destination for logging output.
*/
class StreamLogHandler: public LogHandler {
public:
    /*!
        \brief Constructor.
        \param stream Output stream.
        \param level Default logging level.
        \param filters Category filters.
    */
    explicit StreamLogHandler(Print &stream, LogLevel level = LOG_LEVEL_INFO, LogCategoryFilters filters = {});
    /*!
        \brief Returns output stream.
    */
    Print* stream() const;

protected:
    /*!
        \brief Formats log message and writes it to output stream.
        \param msg Text message.
        \param level Logging level.
        \param category Category name (can be null).
        \param attr Message attributes.

        Default implementation generates messages in the following format:
        `<timestamp> [category] [file]:[line], [function]: <level>: <message> [attributes]`.
    */
    virtual void logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) override;
    /*!
        \brief Writes character buffer to output stream.
        \param data Buffer.
        \param size Buffer size.

        This method is equivalent to `stream()->write((const uint8_t*)data, size)`.
    */
    virtual void write(const char *data, size_t size) override;
    /*!
        \brief Writes string to output stream.
        \param str String.

        This method is equivalent to `write(str, strlen(str))`.
    */
    void write(const char *str);

private:
    Print *stream_;
};

class JSONLogHandler: public StreamLogHandler {
public:
    explicit JSONLogHandler(Print &stream, LogLevel level = LOG_LEVEL_INFO, LogCategoryFilters filters = {});

    virtual void logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) override;

private:
    JSONStreamWriter writer_;
};

class AttributedLogger;

/*!
    \brief Logger.

    Primary interface for application logging. Most of the logging operations, except configuration,
    are done through this class.

    The library also declares default logger instance accessible as global \ref spark::Log variable.
*/
class Logger {
public:
    /*!
        \brief Default logging level.
    */
    static const LogLevel DEFAULT_LEVEL = LOG_LEVEL_INFO;

    /*!
        \brief Constructor.
        \param name Category name.

        Default-constructed logger uses category name specified at module level (typically, "app").
    */
    explicit Logger(const char *name = LOG_MODULE_CATEGORY);
    /*!
        \brief Generates trace message.
        \param fmt Format string.
    */
    void trace(const char *fmt, ...) const __attribute__((format(printf, 2, 3))); // First argument is implicit 'this'
    /*!
        \brief Generates info message.
        \param fmt Format string.
    */
    void info(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
    /*!
        \brief Generates warning message.
        \param fmt Format string.
    */
    void warn(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
    /*!
        \brief Generates error message.
        \param fmt Format string.
    */
    void error(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
    /*!
        \brief Generates log message.
        \param fmt Format string.

        This method uses default logging level (\ref DEFAULT_LEVEL).
    */
    void log(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
    /*!
        \brief Generates log message.
        \param level Logging level.
        \param fmt Format string.
    */
    void log(LogLevel level, const char *fmt, ...) const __attribute__((format(printf, 3, 4)));
    /*!
        \brief Writes formatted string to log.
        \param fmt Format string.

        This method uses default logging level (\ref DEFAULT_LEVEL).
    */
    void printf(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
    /*!
        \brief Writes formatted string to log.
        \param level Logging level.
        \param fmt Format string.
    */
    void printf(LogLevel level, const char *fmt, ...) const __attribute__((format(printf, 3, 4)));
    /*!
        \brief Writes string to log.
        \param str String.

        This method uses default logging level (\ref DEFAULT_LEVEL).
    */
    void print(const char *str) const;
    /*!
        \brief Writes string to log.
        \param level Logging level.
        \param str String.
    */
    void print(LogLevel level, const char *str) const;
    /*!
        \brief Writes character buffer to log.
        \param data Buffer.
        \param size Buffer size.

        This method uses default logging level (\ref DEFAULT_LEVEL).
    */
    void write(const char *data, size_t size) const;
    /*!
        \brief Writes character buffer to log.
        \param level Logging level.
        \param data Buffer.
        \param size Buffer size.
    */
    void write(LogLevel level, const char *data, size_t size) const;
    /*!
        \brief Encodes data buffer in hex and writes resulting string to log.
        \param data Buffer.
        \param size Buffer size.

        This method uses default logging level (\ref DEFAULT_LEVEL).
    */
    void dump(const void *data, size_t size) const;
    /*!
        \brief Encodes data buffer in hex and writes resulting string to log.
        \param level Logging level.
        \param data Buffer.
        \param size Buffer size.
    */
    void dump(LogLevel level, const void *data, size_t size) const;
    /*!
        \brief Returns `true` if trace level is enabled for this logger.
    */
    bool isTraceEnabled() const;
    /*!
        \brief Returns `true` if info level is enabled for this logger.
    */
    bool isInfoEnabled() const;
    /*!
        \brief Returns `true` if warning level is enabled for this logger.
    */
    bool isWarnEnabled() const;
    /*!
        \brief Returns `true` if error level is enabled for this logger.
    */
    bool isErrorEnabled() const;
    /*!
        \brief Returns `true` if specified level is enabled for this logger.
        \param level Logging level.
    */
    bool isLevelEnabled(LogLevel level) const;
    /*!
        \brief Returns category name set for this logger.
    */
    const char* name() const;
    /*!
        \brief Sets `LogAttributes::code` attribute.
        \param code Code.
    */
    AttributedLogger code(intptr_t code) const;
    /*!
        \brief Sets `LogAttributes::details` attribute.
        \param str String.
    */
    AttributedLogger details(const char *str) const;
    /*!
        \brief This method is an alias for \ref log(const char*, ...).
    */
    void operator()(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
    /*!
        \brief This method is an alias for \ref log(LogLevel, const char*, ...).
    */
    void operator()(LogLevel level, const char *fmt, ...) const __attribute__((format(printf, 3, 4)));

    // This class is non-copyable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    const char* const name_; // Category name

    void log(LogLevel level, const char *fmt, va_list args) const;
};

/*!
    \brief Attributed logger.

    Helper class allowing to set additional attributes for log messages. This class can be
    instantiated only via \ref spark::Logger methods, e.g. \ref spark::Logger::code():

    ```
    Logger log;
    log.code(-1).details("details").info("This is info message");
    ```
*/
class AttributedLogger {
public:
    /*!
        \name Methods generating a log message.

        Refer to the \ref spark::Logger for the details.
    */
    //! @{
    void trace(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    void info(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    void warn(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    void error(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    void log(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    void log(LogLevel level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
    //! @}
    /*!
        \name Setter methods for log message attributes.

        Refer to the \ref spark::Logger for the details.
    */
    //! @{
    AttributedLogger& code(intptr_t code);
    AttributedLogger& details(const char *str);
    //! @}

private:
    const char* const name_;
    LogAttributes attr_;

    explicit AttributedLogger(const char *name);
    AttributedLogger(const AttributedLogger&) = default;

    void log(LogLevel level, const char *fmt, va_list args);

    AttributedLogger& operator=(const AttributedLogger&) = default;

    friend class Logger;
};

// NOTE: This is an experimental API and is subject to change
class LogHandlerFactory {
public:
    virtual ~LogHandlerFactory() = default;

    virtual LogHandler* createHandler(const JSONString &type, const JSONValue &params, Print *stream, LogLevel level,
            const LogCategoryFilters &filters) = 0;
    virtual void destroyHandler(LogHandler *handler);
};

// NOTE: This is an experimental API and is subject to change
class OutputStreamFactory {
public:
    virtual ~OutputStreamFactory() = default;

    virtual Print* createStream(const JSONString &type, const JSONValue &params) = 0;
    virtual void destroyStream(Print *stream);
};

/*!
    \brief Log manager.

    Singleton class maintaining global list of registered log handlers. Log manager takes care of
    the necessary system callbacks and forwards generated logging output to registered log handlers.
*/
class LogManager {
public:
    /*!
        \brief Destructor.
    */
    ~LogManager();
    /*!
        \brief Registers log handler.

        \param handler Handler instance.
        \return `false` in case of error.

        \note Log manager doesn't take ownership over the handler instance.
    */
    bool addHandler(LogHandler *handler);
    /*!
        \brief Unregisters log handler.

        \param handler Handler instance.
    */
    void removeHandler(LogHandler *handler);
    /*!
        \brief Registers log handler factory.

        \param factory Factory instance.
        \return `false` in case of error.

        \note Log manager doesn't take ownership over the factory instance.
    */
    bool addHandlerFactory(LogHandlerFactory *factory);
    /*!
        \brief Unregisters log handler factory.

        \param factory Factory instance.
    */
    void removeHandlerFactory(LogHandlerFactory *factory);
    /*!
        \brief Registers output stream factory.

        \param factory Factory instance.
        \return `false` in case of error.

        \note Log manager doesn't take ownership over the factory instance.
    */
    bool addStreamFactory(OutputStreamFactory *factory);
    /*!
        \brief Unregisters output stream factory.

        \param factory Factory instance.
    */
    void removeStreamFactory(OutputStreamFactory *factory);
    /*!
        \brief Creates and registers a named log handler.

        \param id Handler ID.
        \param handlerType Handler type name.
        \param handlerParams Type-specific handler parameters.
        \param streamType Stream type name.
        \param streamParams Type-specific stream parameters.
        \param level Default logging level.
        \param filters Category filters.

        \return `false` in case of error.
    */
    bool addNamedHandler(const JSONString &id, const JSONString &handlerType, const JSONValue &handlerParams,
            const JSONString &streamType, const JSONValue &streamParams, LogLevel level, LogCategoryFilters filters);
    /*!
        \brief Unregisters and destroys a named log handler.

        \param id Handler ID.
    */
    void removeNamedHandler(const JSONString &id);
    /*!
        \brief Enumerates named log handlers.

        \param callback Callback function invoked for each active handler.
        \param data User data.
    */
    void enumNamedHandlers(void(*callback)(const char *id, void *data), void *data);

    /*!
        \brief Returns log manager's instance.
    */
    static LogManager* instance();

    // This class is non-copyable
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

private:
    struct NamedHandler;

    Array<LogHandlerFactory*> handlerFactories_;
    Array<OutputStreamFactory*> streamFactories_;
    Array<NamedHandler> namedHandlers_;
    Array<LogHandler*> activeHandlers_;

#if PLATFORM_THREADING
    Mutex mutex_; // TODO: Use read-write lock?
#endif

    // This class can be instantiated only via instance() method
    LogManager();

    int namedHandlerIndex(const JSONString &id) const;
    void destroyNamedHandler(const NamedHandler &h);

    // System callbacks
    static void logMessage(const char *msg, int level, const char *category, const LogAttributes *attr, void *reserved);
    static void logWrite(const char *data, size_t size, int level, const char *category, void *reserved);
    static int logEnabled(int level, const char *category, void *reserved);
};

/*!
    \brief Performs processing of a configuration request.

    \param reqData Request data.
    \param reqSize Request data size.
    \param repData Buffer for reply data.
    \param repSize Reply data size.
    \param fmt Request format.

    \return `false` in case of error.

    \note `repSize` should be initialized with maximum size available for reply data.
*/
bool logProcessConfigRequest(const char *reqData, size_t reqSize, char *repData, size_t *repSize, DataFormat fmt);

/*!
    \brief Default logger instance.
*/
extern const Logger Log;

} // namespace spark

// spark::LogCategoryFilter
inline spark::LogCategoryFilter::LogCategoryFilter(String category, LogLevel level) :
        cat_(category),
        level_(level) {
}

inline spark::LogCategoryFilter::LogCategoryFilter(const char *category, LogLevel level) :
        cat_(category),
        level_(level) {
}

inline spark::LogCategoryFilter::LogCategoryFilter(const char *category, size_t length, LogLevel level) :
        cat_(category, length),
        level_(level) {
}

inline const char* spark::LogCategoryFilter::category() const {
    return cat_.c_str();
}

inline LogLevel spark::LogCategoryFilter::level() const {
    return level_;
}

// spark::LogFilter
inline LogLevel spark::LogFilter::level() const {
    return level_;
}

// spark::LogHandler
inline spark::LogHandler::LogHandler(LogLevel level) :
        filter_(level) {
}

inline spark::LogHandler::LogHandler(LogLevel level, LogCategoryFilters filters) :
        filter_(level, filters) {
}

inline LogLevel spark::LogHandler::level() const {
    return filter_.level();
}

inline LogLevel spark::LogHandler::level(const char *category) const {
    return filter_.level(category);
}

inline const char* spark::LogHandler::levelName(LogLevel level) {
    return log_level_name(level, nullptr);
}

inline void spark::LogHandler::message(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) {
    if (level >= filter_.level(category)) {
        logMessage(msg, level, category, attr);
    }
}

inline void spark::LogHandler::write(const char *data, size_t size, LogLevel level, const char *category) {
    if (level >= filter_.level(category)) {
        write(data, size);
    }
}

inline void spark::LogHandler::write(const char *data, size_t size) {
    // Default implementation does nothing
}

// spark::StreamLogHandler
inline spark::StreamLogHandler::StreamLogHandler(Print &stream, LogLevel level, LogCategoryFilters filters) :
        LogHandler(level, filters),
        stream_(&stream) {
}

inline void spark::StreamLogHandler::write(const char *data, size_t size) {
    stream_->write((const uint8_t*)data, size);
}

inline Print* spark::StreamLogHandler::stream() const {
    return stream_;
}

inline void spark::StreamLogHandler::write(const char *str) {
    write(str, strlen(str));
}

// spark::JSONLogHandler
inline spark::JSONLogHandler::JSONLogHandler(Print &stream, LogLevel level, LogCategoryFilters filters) :
        StreamLogHandler(stream, level, filters),
        writer_(stream) {
}

// spark::Logger
inline spark::Logger::Logger(const char *name) :
        name_(name) {
}

inline void spark::Logger::trace(const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    log(LOG_LEVEL_TRACE, fmt, args);
    va_end(args);
}

inline void spark::Logger::info(const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    log(LOG_LEVEL_INFO, fmt, args);
    va_end(args);
}

inline void spark::Logger::warn(const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    log(LOG_LEVEL_WARN, fmt, args);
    va_end(args);
}

inline void spark::Logger::error(const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    log(LOG_LEVEL_ERROR, fmt, args);
    va_end(args);
}

inline void spark::Logger::log(const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    log(DEFAULT_LEVEL, fmt, args);
    va_end(args);
}

inline void spark::Logger::log(LogLevel level, const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    log(level, fmt, args);
    va_end(args);
}

inline void spark::Logger::printf(const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    log_printf_v(DEFAULT_LEVEL, name_, nullptr, fmt, args);
    va_end(args);
}

inline void spark::Logger::printf(LogLevel level, const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    log_printf_v(level, name_, nullptr, fmt, args);
    va_end(args);
}

inline void spark::Logger::print(const char *str) const {
    print(DEFAULT_LEVEL, str);
}

inline void spark::Logger::print(LogLevel level, const char *str) const {
    write(level, str, strlen(str));
}

inline void spark::Logger::write(const char *data, size_t size) const {
    write(DEFAULT_LEVEL, data, size);
}

inline void spark::Logger::write(LogLevel level, const char *data, size_t size) const {
    if (data) {
        log_write(level, name_, data, size, nullptr);
    }
}

inline void spark::Logger::dump(const void *data, size_t size) const {
    dump(DEFAULT_LEVEL, data, size);
}

inline void spark::Logger::dump(LogLevel level, const void *data, size_t size) const {
    if (data) {
        log_dump(level, name_, data, size, 0, nullptr);
    }
}

inline bool spark::Logger::isTraceEnabled() const {
    return isLevelEnabled(LOG_LEVEL_TRACE);
}

inline bool spark::Logger::isInfoEnabled() const {
    return isLevelEnabled(LOG_LEVEL_INFO);
}

inline bool spark::Logger::isWarnEnabled() const {
    return isLevelEnabled(LOG_LEVEL_WARN);
}

inline bool spark::Logger::isErrorEnabled() const {
    return isLevelEnabled(LOG_LEVEL_ERROR);
}

inline bool spark::Logger::isLevelEnabled(LogLevel level) const {
    return log_enabled(level, name_, nullptr);
}

inline const char* spark::Logger::name() const {
    return name_;
}

inline spark::AttributedLogger spark::Logger::code(intptr_t code) const {
    AttributedLogger log(name_);
    log.code(code);
    return log;
}

inline spark::AttributedLogger spark::Logger::details(const char *str) const {
    AttributedLogger log(name_);
    log.details(str);
    return log;
}

inline void spark::Logger::operator()(const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    log(DEFAULT_LEVEL, fmt, args);
    va_end(args);
}

inline void spark::Logger::operator()(LogLevel level, const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    log(level, fmt, args);
    va_end(args);
}

inline void spark::Logger::log(LogLevel level, const char *fmt, va_list args) const {
    LogAttributes attr;
    attr.size = sizeof(LogAttributes);
    attr.flags = 0;
    log_message_v(level, name_, &attr, nullptr, fmt, args);
}

// spark::AttributedLogger
inline spark::AttributedLogger::AttributedLogger(const char *name) :
        name_(name) {
    attr_.size = sizeof(LogAttributes);
    attr_.flags = 0;
}

inline void spark::AttributedLogger::trace(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LOG_LEVEL_TRACE, fmt, args);
    va_end(args);
}

inline void spark::AttributedLogger::info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LOG_LEVEL_INFO, fmt, args);
    va_end(args);
}

inline void spark::AttributedLogger::warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LOG_LEVEL_WARN, fmt, args);
    va_end(args);
}

inline void spark::AttributedLogger::error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LOG_LEVEL_ERROR, fmt, args);
    va_end(args);
}

inline void spark::AttributedLogger::log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(Logger::DEFAULT_LEVEL, fmt, args);
    va_end(args);
}

inline void spark::AttributedLogger::log(LogLevel level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(level, fmt, args);
    va_end(args);
}

inline spark::AttributedLogger& spark::AttributedLogger::code(intptr_t code) {
    LOG_ATTR_SET(attr_, code, code);
    return *this;
}

inline spark::AttributedLogger& spark::AttributedLogger::details(const char *str) {
    LOG_ATTR_SET(attr_, details, str);
    return *this;
}

inline void spark::AttributedLogger::log(LogLevel level, const char *fmt, va_list args) {
    log_message_v(level, name_, &attr_, nullptr, fmt, args);
}

// spark::LogHandlerFactory
inline void spark::LogHandlerFactory::destroyHandler(LogHandler *handler) {
    delete handler;
}

// spark::OutputStreamFactory
inline void spark::OutputStreamFactory::destroyStream(Print *stream) {
    delete stream;
}

#endif // SPARK_WIRING_LOGGING_H
