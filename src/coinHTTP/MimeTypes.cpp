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

#include "coinHTTP/MimeTypes.h"

namespace MimeTypes {
    
    struct mapping {
        const char* extension;
        const char* mime_type;
    } mappings[] = {
        { "gif", "image/gif" },
        { "htm", "text/html" },
        { "html", "text/html" },
        { "jpg", "image/jpeg" },
        { "png", "image/png" },
        { "css", "text/css" },
        { "js", "text/javascript" },
        { 0, 0 } // Marks end of list.
    };
    
    std::string extension_to_type(const std::string& extension) {
        for (mapping* m = mappings; m->extension; ++m) {
            if (m->extension == extension) {
                return m->mime_type;
            }
        }
        
        return "text/plain";
    }
    
} // namespace MimeTypes
