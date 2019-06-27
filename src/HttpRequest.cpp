#include "HttpRequest.h"

// ÖØÔØHttpRequest <<
std::ostream &operator<<(std::ostream &os, const HttpRequest &request) {
    os << "method:" << request.mMethod << std::endl;
    os << "uri:" << request.mUri << std::endl;
    os << "version:" << request.mVersion << std::endl;
    //os << "content:" << request.mContent << std::endl;
    for (auto it = request.mHeaders.begin(); it != request.mHeaders.end(); it++) {
        os << it->first << ":" << it->second << std::endl;
    }
    return os;
}

