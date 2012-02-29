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

#include <coin/util.h>
#include <coinNAT/PortMapper.h>

#include "miniupnpc/miniwget.h"
#include "miniupnpc/miniupnpc.h"
#include "miniupnpc/upnpcommands.h"
#include "miniupnpc/upnperrors.h"
#include "libnatpmp/natpmp.h"

#include <boost/bind.hpp>

using namespace std;
using namespace boost;
using namespace boost::asio;


class PortMapper::Impl {
public:
    struct UPNPDev* devlist;
    struct UPNPUrls urls;
    struct IGDdatas data;
    char lanaddr[64];
    natpmp_t natpmp;
    natpmpresp_t response;
};

PortMapper::PortMapper(boost::asio::io_service& io_service, unsigned short port, unsigned int repeat_interval) : _portmap_state(NONE), _state(IDGPORTMAP), _port(port), _repeat_interval(repeat_interval), _repeat_timer(io_service), _impl(new Impl) {    
}

void PortMapper::start() {
    _repeat_timer.expires_from_now(boost::posix_time::seconds(0));
    _repeat_timer.async_wait(bind(&PortMapper::handle_mapping, this, placeholders::error));
}

void PortMapper::stop() {
    _repeat_timer.cancel();
}


void PortMapper::handle_mapping(const boost::system::error_code& e) {
    if(e && e != error::operation_aborted) {
        printf("PortMapper timer error: %s\n", e.message().c_str());
        _repeat_timer.expires_from_now(boost::posix_time::seconds(_repeat_interval));
    }
    else {
        switch(_state) {
            case IDGPORTMAP:
                _idg_thread = thread(&PortMapper::reqIDGportmap, this, _port);  
                _state = IDGWAITPORTMAP;
                return; // don't start the timer!
            case IDGWAITPORTMAP:
                _idg_thread.join(); // wait for the thread to really exit
                if(_idg_mapping) { // portmapping performed
                    _repeat_timer.expires_from_now(boost::posix_time::seconds(_repeat_interval));
                    _state = IDGPORTMAP;
                }
                else {
                    _repeat_timer.expires_from_now(boost::posix_time::seconds(0));
                    _state = PMPPORTMAP;                    
                }
                break;
            case PMPPORTMAP:
                if((_pmp_timeout = reqPMPportmap(_port))) {
                    _repeat_timer.expires_from_now(boost::posix_time::milliseconds(_pmp_timeout));
                    _state = PMPWAITPORTMAP;                                        
                }
                else {
                    _repeat_timer.expires_from_now(boost::posix_time::seconds(_repeat_interval));
                    _state = IDGPORTMAP;                    
                }
                break;
            case PMPWAITPORTMAP: {
                boost::tribool status = repPMPportmap();
                if(status || !status) { // portmapping is already setup or not available
                    _repeat_timer.expires_from_now(boost::posix_time::seconds(_repeat_interval));
                    _state = IDGPORTMAP;
                }
                else // indeterminate - wait again
                    _repeat_timer.expires_from_now(boost::posix_time::seconds(_pmp_timeout));
                break;
            }
            default:
                break;
        }
    }
    _repeat_timer.async_wait(bind(&PortMapper::handle_mapping, this, placeholders::error));
}
        
void PortMapper::reqIDGportmap(unsigned short p) {
    printf("PortMapper::getIDGdevice()...\n");
    
    const char * multicastif = 0;
    const char * minissdpdpath = 0;
    int ipv6 = 0, error = 0;
    _impl->devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, ipv6, &error);
    
    int r;
    
    r = UPNP_GetValidIGD(_impl->devlist, &_impl->urls, &_impl->data, _impl->lanaddr, sizeof(_impl->lanaddr));
    if (r == 1) {
        char port[6];
        sprintf(port, "%d", p);
        char leaseDuration[8];
        sprintf(leaseDuration, "%d", 2*_repeat_interval);
        
        int r = UPNP_AddPortMapping(_impl->urls.controlURL, _impl->data.first.servicetype, port, port, _impl->lanaddr, 0, "TCP", 0, leaseDuration);

        _idg_mapping = (r == UPNPCOMMAND_SUCCESS);
        
        freeUPNPDevlist(_impl->devlist); _impl->devlist = 0;
        FreeUPNPUrls(&_impl->urls);        
    }
    else {
        printf("No valid UPnP IGDs found\n");
        _idg_mapping = false;
        freeUPNPDevlist(_impl->devlist); _impl->devlist = 0;
        if (r != 0)
            FreeUPNPUrls(&_impl->urls);
    }    
    _repeat_timer.expires_from_now(boost::posix_time::seconds(0));
    _repeat_timer.async_wait(bind(&PortMapper::handle_mapping, this, placeholders::error));
}

unsigned int PortMapper::reqPMPportmap(unsigned short port) {
    in_addr_t forcedgw = 0;
    struct timeval timeout;
    initnatpmp(&_impl->natpmp, 0, forcedgw);
    sendnewportmappingrequest(&_impl->natpmp, NATPMP_PROTOCOL_TCP, port, port, 2*_repeat_interval);
    getnatpmprequesttimeout(&_impl->natpmp, &timeout);
    return timeout.tv_sec*1000 + timeout.tv_usec/1000;    
}

boost::tribool PortMapper::repPMPportmap() {
    int r = readnatpmpresponseorretry(&_impl->natpmp, &_impl->response);
    if(r == NATPMP_TRYAGAIN)
        return indeterminate;
    
    closenatpmp(&_impl->natpmp);    

    if(r == 0)
        printf("NATPMP: mapped public port %hu to localport %hu liftime %u\n",
               _impl->response.pnu.newportmapping.mappedpublicport,
               _impl->response.pnu.newportmapping.privateport,
               _impl->response.pnu.newportmapping.lifetime);
    else
        printf("NATPMP: PORTMAP FAILED");
    
    return (r == 0);
}

