#pragma once

# if USE_COLORS
#define escape_sequence "\x1b"
#define info_color "\x1b[47;31m"
#define fork_color "\x1b[47;30m"
#define key_color "\x1b[01;32;41m"
#define keysym_color "\x1b[33;01m"
#define warning_color "\x1b[38;5;160m"
#define key_io_color "\x1b[38;5;226m"
#define color_reset "\x1b[0m"
# else // USE_COLORS
#define info_color ""
#define key_color  ""
#define warning_color  ""
#define color_reset ""
#define escape_sequence ""
// ....
#endif // USE_COLORS
