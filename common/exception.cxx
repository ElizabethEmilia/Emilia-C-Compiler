#include "exception.h"

namespace Miyuki::Common {

    const char * IOException::what() const noexcept {
       return "no such file or directory.";
    }

    const char * InvalidToken::what() const noexcept {
       return "invalid token found.";
    }

    const char *SyntaxError::what() const noexcept {
        return msg.c_str();
    }

}
