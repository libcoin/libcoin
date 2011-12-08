
#ifndef HTTP_MIME_TYPES_H
#define HTTP_MIME_TYPES_H

#include <string>

namespace MimeTypes {

/// Convert a file extension into a MIME type.
std::string extension_to_type(const std::string& extension);

} // namespace MimeTypes

#endif // HTTP_MIME_TYPES_H