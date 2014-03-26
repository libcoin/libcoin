/* -*-c++-*- libcoin - Copyright (C) 2012 Michael Gronager
 *
 * libcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * libcoin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libcoin.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef COIN_LOGGER_H
#define COIN_LOGGER_H

#include <coin/Export.h>

#include <boost/thread/thread.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <deque>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvariadic-macros"

class COIN_EXPORT LineBuffer : public std::streambuf {
public:
    LineBuffer(size_t lines = 10) : std::streambuf(), _lines(lines) { }
    
    ~LineBuffer() {
        newline();
    }
    
    void flush(std::ostream& os);
    
protected:
    virtual int overflow(int ch);
    
private:
    void newline();
    
private:
    LineBuffer(LineBuffer const &); // disallow copy construction
    void operator= (LineBuffer const &); // disallow copy assignment
    
private:
    std::string _line;
    size_t _lines;
    typedef std::deque<std::string> Buffer;
    Buffer _buffer;
};

class COIN_EXPORT Logger {
public: // enums
    enum Level {
        trace = 1,
        debug,
        info,
        warn,
        error,
        fatal
    };
    
public: // static
    static Logger& instance();
    
    static std::ostream& log(Level level, std::string file, int line, std::string function = "");
    
    static void instantiate(std::ostream& log_stream, Level log_level = info);
    
    static void label_thread(std::string label);
    
public:
    const std::string label(Level level) const;
    
    const std::string& label();

    const std::string timestamp() const;
    
private:
    std::ostream* _log_stream;

    LineBuffer _line_buffer;
    std::ostream _buf_stream;
    
    std::ostream _null_stream;
    
    Level _log_level;

    typedef std::map<Level, std::string> LevelLabels;
    LevelLabels _level_labels;

    typedef std::map<boost::thread::id, std::string> ThreadLabels;
    ThreadLabels _thread_labels;    

private:
    Logger();

    // not to be implemented!
    Logger(Logger const&);
    void operator=(Logger const&);
};

#define log_fatal(...) log_fatal_(__VA_ARGS__,"")
#define log_error(...) log_error_(__VA_ARGS__,"")
#define log_warn(...) log_warn_(__VA_ARGS__,"")
#define log_info(...) log_info_(__VA_ARGS__,"")
#define log_debug(...) log_debug_(__VA_ARGS__,"")
#define log_trace(...) log_trace_(__VA_ARGS__,"")

#define log_fatal_(...) Logger::log(Logger::fatal, __FILE__, __LINE__, __FUNCTION__) << cformat(__VA_ARGS__) << std::endl
#define log_error_(...) Logger::log(Logger::error, __FILE__, __LINE__, __FUNCTION__) << cformat(__VA_ARGS__) << std::endl
#define log_warn_(...) Logger::log(Logger::warn, __FILE__, __LINE__, __FUNCTION__) << cformat(__VA_ARGS__) << std::endl
#define log_info_(...) Logger::log(Logger::info, __FILE__, __LINE__, __FUNCTION__) << cformat(__VA_ARGS__) << std::endl
#define log_debug_(...) Logger::log(Logger::debug, __FILE__, __LINE__, __PRETTY_FUNCTION__) << cformat(__VA_ARGS__) << std::endl
#define log_trace_(...) Logger::log(Logger::trace, __FILE__, __LINE__, __PRETTY_FUNCTION__) << cformat(__VA_ARGS__) << std::endl

//#define log_debug(...) do {} while(0)
//#define log_trace(...) do {} while(0)

// MSVC++ has a bug in the interpretation of VA_ARGS so:
// #define F(x, ...) X = x and VA_ARGS = __VA_ARGS__
// #define G(...) F(__VA_ARGS__)
// G(1, 2, 3) -> X = 1, 2, 3
// i.e. VA_ARGS is interpretated as one argument not multiple and hence a VA_ARGS cannot be forwarded - hence we use (fmt, ...) throughout

#define cformat(fmt, ...) (Format(fmt), ##__VA_ARGS__)

class COIN_EXPORT Format {
public:
    Format(std::string format);
    
    /// The format string parser - a format specifier follows the prototype:
    ///     %[flags][width][.precision][length]specifier
    template<class T>
    inline Format& operator,(const T& rhs) {
        // find the format string and insert
        _pos = _format.find('%', _pos);
        if (_pos != std::string::npos) {
            size_t specifier_pos = _format.find_first_of("diuoxXfFeEgGaAcspn%", _pos + 1);
            
            if (specifier_pos == std::string::npos)
                return *this;

            if (_format[specifier_pos] == '%') {
                _format.erase(specifier_pos, 1);
                _pos += 1;
                return operator,(rhs);
            }
            
            std::string fmt = _format.substr(_pos + 1, specifier_pos - _pos);

            std::stringstream ss;
            manipulate(ss, fmt);
            ss << rhs;
            const std::string& text = ss.str();
            
            _format.replace(_pos, fmt.length() + 1, text);
            _pos += text.length();
        }
        return *this;
    }

    std::string text() const;
private:
    void manipulate(std::ostream& os, std::string fmt) const;

private:
    std::string _format;
    size_t _pos;
};

inline std::ostream& operator<<(std::ostream& os, const Format& fmt) { return os << fmt.text(); }

#pragma GCC diagnostic pop
            
#endif // COIN_LOGGER_H