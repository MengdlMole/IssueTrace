// Qt's MinGW entry-point library references the legacy imported __argc
// variable. Newer UCRT-oriented MinGW-w64 toolchains expose it through
// __p___argc() instead. Define the compatible import pointer locally so a
// current open-source cross compiler can link against the official Qt SDK.
#if defined(_WIN32) && defined(__GNUC__)
extern "C" {
int* __p___argc();
int* __imp___argc = __p___argc();
}
#endif
