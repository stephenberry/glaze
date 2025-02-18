#ifdef CPP_MODULES
#define EXPORT export
#define BEGIN_EXPORT export {
#define END_EXPORT }
#else
#define EXPORT
#define BEGIN_EXPORT
#define END_EXPORT
#endif