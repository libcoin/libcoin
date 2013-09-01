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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/time_facet.hpp>
#include <boost/filesystem.hpp>

#include <coin/Logger.h>

#include <boost/lexical_cast.hpp>
#include <boost/assign/list_of.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace boost;

void LineBuffer::flush(std::ostream& os) {
    while (_buffer.size()) {
        os << "\t" << _buffer.front() << std::endl;
        _buffer.pop_front();
    }
}
    
int LineBuffer::overflow(int ch) {
    if (ch == '\n') {
        newline();
    } else {
        _line += ch;
    }
    return ch == -1 ? -1 : ' ';
}
    
void LineBuffer::newline() {
    _buffer.push_back(_line);
    while (_buffer.size() > _lines)
        _buffer.pop_front();

    _line.erase();
}

Logger& Logger::instance() {
    static Logger __instance;
    
    return __instance;
}
    
std::ostream& Logger::log(Level level, std::string file, int line, std::string function) {
    Logger& self = instance();
    filesystem::path path(file);
    file = path.filename().string();

    std::ostream* s = instance()._log_stream;
    if (level < self._log_level)
        s = &self._null_stream;
        //        s = &self._buf_stream;
    std::ostream& os = *s;
    if (level == fatal) {
        os << "Flushing earlier log:" << std::endl;
        self._line_buffer.flush(os);
    }
    os << self.timestamp() << " ";
    os << self.label(level) << "(" << self.label() << ") ";
    if (self._log_level < info) {
        os << "[" << file << ":" << line;
        if (function.size())
            os << " " << function;
        os << "] ";
    }
    return os;
}
    
void Logger::instantiate(std::ostream& log_stream, Level log_level) {
    if (!log_stream.fail()) {
        instance()._log_stream = &log_stream;
    }
    instance()._log_level = log_level;
}
    
void Logger::label_thread(std::string label) {
    boost::thread::id id = boost::this_thread::get_id();
    instance()._thread_labels[id] = label;
}
const std::string Logger::label(Level level) const {
    LevelLabels::const_iterator ll = _level_labels.find(level);
    if (ll != _level_labels.end())
        return ll->second;
    return "MESSAGE";
}

const std::string& Logger::label() { // not const as we register the thread if it is not already registered
    boost::thread::id id = boost::this_thread::get_id();
    ThreadLabels::const_iterator tl = _thread_labels.find(id);
    if (tl != _thread_labels.end())
        return tl->second;
    _thread_labels[id] = "thread#" + lexical_cast<std::string>(_thread_labels.size());
    return _thread_labels[id];
}

const std::string Logger::timestamp() const {
    boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    boost::posix_time::time_facet* facet(new boost::posix_time::time_facet("%H:%M:%S"));
    std::stringstream ss;
    ss.imbue(std::locale(ss.getloc(), facet));
    ss << now;
    return ss.str();
}

Logger::Logger() : _log_stream(&std::cerr), _line_buffer(10), _buf_stream(&_line_buffer), _null_stream(NULL) {
    _level_labels = boost::assign::map_list_of
        (trace, "TRACE")
        (debug, "DEBUG")
        (info, "INFO")
        (warn, "WARN")
        (error, "ERROR")
        (fatal, "FATAL");
}

Format::Format(std::string format) : _format(format), _pos(0) {
}

std::string Format::text() const {
    return _format;
}



void Format::manipulate(std::ostream& os, std::string fmt) const {
    char specifier = fmt[fmt.length() - 1];
    fmt.erase(fmt.length() - 1, 1);
    switch (specifier) {
        case 'x': os << std::hex; break;
        case 'X': os << std::hex << std::uppercase; break;
        case 'o': os << std::oct; break;
        case 'O': os << std::oct << std::uppercase; break;
        case 'f': os << std::fixed; break;
        case 'F': os << std::fixed << std::uppercase; break;
        case 'e': os << std::scientific; break;
        case 'E': os << std::scientific << std::uppercase; break;
    };
    if (fmt.length()) {
        // do we have flags ?
        while (fmt.find_first_of("-+ #0") == 0) {
            switch (fmt[0]) {
                case '-': os << std::left; break;
                case '+': os << std::showpos; break;
                case ' ': break; // not supported
                case '#': os << std::showbase << std::showpoint; break;
                case '0': os << std::setfill('0'); break;
            };
            fmt.erase(0, 1);
        }
        // do we have a width
        size_t width = 0;
        std::istringstream(fmt) >> width;
        if (width) os << std::setw(width);
        // do we have a precision
        size_t dot_pos = fmt.find('.');
        if (dot_pos != std::string::npos) {
            fmt.erase(0, dot_pos + 1);
            size_t precision = 0;
            std::istringstream(fmt) >> precision;
            os << std::setprecision(precision);
        }
    }
}
