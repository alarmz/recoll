#ifndef _ZLIBUT_H_INCLUDED_
#define _ZLIBUT_H_INCLUDED_

#include <sys/types.h>

class ZLibUtBuf {
public:
    ZLibUtBuf();
    ~ZLibUtBuf();
    ZLibUtBuf(const ZLibUtBuf&) = delete;
    ZLibUtBuf& operator=(const ZLibUtBuf&) = delete;
    
    char *getBuf() const;
    char *takeBuf();
    size_t getCnt();

    class Internal;
    Internal *m;
};

bool inflateToBuf(const void* inp, size_t inlen, ZLibUtBuf& buf);
bool deflateToBuf(const void* inp, size_t inlen, ZLibUtBuf& buf);

#endif /* _ZLIBUT_H_INCLUDED_ */
