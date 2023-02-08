#include <bits/types/struct_timeval.h>
#include <sys/select.h>
#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include <Utils/log.h>
#include "Utils/utils.h"

namespace fleet {

/*******************Logger*******************/
Logger &Logger::Instance() {
  static Logger instance;
  return instance;
}

Logger::~Logger() { InfoL << "Program ends."; }

// 写到_channels中
void Logger::write_event(LogEvent::Ptr event) {
  if (_writer) {  // 异步
    _writer->push_event(event, this);
  } else {  // 同步
    write_to_channels(event);
  }
}

void Logger::add_channel(std::shared_ptr<LogChannel> ch) { _channels.push_back(ch); }

void Logger::write_to_channels(LogEvent::Ptr event) {
  for (auto &ch : _channels) {
    ch->write(event);
  }
}

void Logger::set_async() { _writer = std::make_shared<AsyncWriter>(); }

/*******************LogEvent*******************/
LogEvent::LogEvent(LogLevel level, const char *file, const char *function, int line)
    : _level(level), _file(strrchr(file, '/') + 1), _function(function), _line(line) {
  gettimeofday(&_tv, nullptr);  // 获取时间，精确到毫秒
  _thread_id = get_thread_id();
}

/*******************LogEventCapture*******************/
LogEventCapture::LogEventCapture(Logger &logger, LogLevel level, const char *file, const char *function, int line)
    : _logger(logger), _event(new LogEvent(level, file, function, line)) {}

LogEventCapture::LogEventCapture(LogEventCapture &&other) : _logger(other._logger), _event(other._event) {
  other._event.reset();
}

LogEventCapture::~LogEventCapture() {
  // LogEventCapture对象里面可能会存放空的_event
  if (_event) {
    _logger.write_event(_event);
  }
}

void LogEventCapture::clear() { _event.reset(); }

/*******************AsyncWriter*******************/
AsyncWriter::AsyncWriter() : _exit(false) {
  _thread = std::make_shared<Thread>([this]() { this->run(); }, "Logger Async Writer");
}

AsyncWriter::~AsyncWriter() {
  _exit = true;
  _sem.post();  // 让run线程的_sem.wait()通过，从而跳出循环
  _thread->join();
  flush_all();  // 处理run线程结束之后，this正式析构之前的Events
}
void AsyncWriter::push_event(LogEvent::Ptr event, Logger *logger) {
  {
    Mutex::Lock lock(_mtx);
    _pending.emplace_back(event, logger);
  }
  _sem.post();
}
void AsyncWriter::run() {
  while (!_exit) {
    _sem.wait();
    flush_all();
  }
}

void AsyncWriter::flush_all() {
  decltype(_pending) tmp;
  {
    Mutex::Lock lock(_mtx);
    tmp.swap(_pending);
  }
  tmp.for_each([](std::pair<LogEvent::Ptr, Logger *> &p) { p.second->write_to_channels(p.first); });
}

/*******************LogChannel*******************/
void LogChannel::format(LogEvent::Ptr event, std::ostream &stream, bool if_color) {
  // 时间
  stream << '[';
  char sec[64], ms[64];
  auto lct = localtime(&(event->_tv.tv_sec));
  strftime(sec, sizeof sec, "%Y-%m-%d %H:%M:%S", lct);
  snprintf(ms, sizeof ms, "%s.%03d", sec, static_cast<int>(event->_tv.tv_usec / 1000));
  stream << ms;
  stream << "] ";

  // 日志等级
  static std::array<std::string, 5> color{"34", "32", "37", "33", "31"};
#define CASE(level)                                                                     \
  case LogLevel::level:                                                                 \
    if (if_color) {                                                                     \
      stream << "\033[1;" << color[static_cast<unsigned long>(LogLevel::level)] << "m"; \
    }                                                                                   \
    stream << std::setw(9) << #level;                                                   \
    if (if_color) {                                                                     \
      stream << "\033[0m";                                                              \
    }                                                                                   \
    stream << " ";                                                                      \
    break;
  switch (event->_level) {
    CASE(Trace)
    CASE(Debug)
    CASE(Info)
    CASE(Warn)
    CASE(Error)
  }
#undef CASE

  // 文件信息
  stream << std::setw(20);
  stream << event->_file;
  stream << std::setw(20);
  stream << event->_function;
  stream << std::setw(6);
  stream << event->_line;

  // 线程号
  stream << std::setw(5);
  stream << '<';
  stream << event->_thread_id;
  stream << ">     ";

  // 日志内容
  if (if_color) {
    stream << "\033[1;" << color[static_cast<unsigned long>(event->_level)] << "m";
  }
  stream << event->str() << '\n';
  if (if_color) {
    stream << "\033[0m";
  }
}

void ConsoleChannel::write(LogEvent::Ptr event) { format(event, std::cout, true); }

FileChannel::FileChannel() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);  // 获取时间
  char time_buf[64];
  auto lct = localtime(&(tv.tv_sec));
  strftime(time_buf, sizeof time_buf, "%Y-%m-%d-%H_%M_%S", lct);
  _path.assign(time_buf);
  _path += ".log";
  reopen();
}

FileChannel::~FileChannel() { _stream.close(); }

void FileChannel::reopen() {
  if (_stream.is_open()) {
    _stream.close();
  }
  _stream.open(_path, std::fstream::out);
}

void FileChannel::write(LogEvent::Ptr event) { format(event, _stream, false); }
}  // namespace fleet