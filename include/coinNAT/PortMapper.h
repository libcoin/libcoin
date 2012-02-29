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


#ifndef PORTMAPPER_H
#define PORTMAPPER_H

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/logic/tribool.hpp>

class PortMapper : boost::noncopyable {
public:
    PortMapper(boost::asio::io_service& io_service, unsigned short port, unsigned int repeat_interval = 10*60);
                                 
    void handle_mapping(const boost::system::error_code& e);
    
private:
    //    bool getIDGdevice();
    void reqIDGportmap(unsigned short port);

    unsigned int reqPMPportmap(unsigned short port);
    boost::tribool repPMPportmap();
    
private:
    enum PortMapState {
        NONE = 0,
        UPNPIDG,
        NATPMP
    } _portmap_state;

    enum State {
        IDGPORTMAP,
        IDGWAITPORTMAP,
        PMPPORTMAP,
        PMPWAITPORTMAP
    } _state;
    
    unsigned short _port;
    unsigned int _repeat_interval;
    boost::asio::deadline_timer _repeat_timer;
    
    boost::thread _idg_thread;
    bool _idg_mapping;
    
    unsigned int _pmp_timeout; // in milli seconds
    
    class Impl;
    boost::shared_ptr<Impl> _impl;
};
#endif

