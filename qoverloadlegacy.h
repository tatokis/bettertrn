#ifndef QOVERLOADLEGACY_H
#define QOVERLOADLEGACY_H
#define SINGLE_ARG(...) __VA_ARGS__
//#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
#if 0
#include <QtGlobal>
#define OVERLOAD_PTR(type, cl, func) (QOverload<type>::of(&cl::func))
#else
#define OVERLOAD_PTR(type, cl, func) (static_cast<void (cl::*)(type)>(&cl::func))
#endif
#endif // QOVERLOADLEGACY_H
