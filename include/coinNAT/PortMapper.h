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

/// The PortMapper class is a wrapper around the miniupnpc and libnatpmp libraries.
/// When activate is called it start a mapping cycle of:
/// * try to setup mapping using IDG UPnP (spawns a background thread as the UPnP calls are blocking) if:
/// ** OK: wait repeat_interval and start over again.
/// ** NOT OK:...
/// * try to setup mapping using NATPMP - wait for response. If:
/// ** response not yet there: wait again
/// ** OK: wait repeat_interval and start over again.
/// ** NOT OK: do.
/// The lease time is choosen to 2 times the repeat_interval 
class PortMapper : boost::noncopyable {
public:
    /// Create a PortMapper using the io_service and mapping the port to the same public port. Repeat the mapping
    /// every 10 minutes by default.
    PortMapper(boost::asio::io_service& io_service, unsigned short port, unsigned int repeat_interval = 10*60);

    /// Start the portmapper.
    void start();
    
    /// Stop the portmapper.
    void stop();
    
private:
    /// internal state machine handler
    void handle_mapping(const boost::system::error_code& e);
    
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
    bool _idg_trying;
    
    unsigned int _pmp_timeout; // in milli seconds
    
    class Impl;
    boost::shared_ptr<Impl> _impl;
};
#endif

