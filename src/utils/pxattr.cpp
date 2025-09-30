/*
  Copyright (c) 2009 Jean-Francois Dockes

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation
  files (the "Software"), to deal in the Software without
  restriction, including without limitation the rights to use,
  copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following
  conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.
*/

/** \file pxattr.cpp 
    \brief Portable External Attributes API
*/


// PXALINUX: platforms like kfreebsd which aren't Linux but use the same xattr interface
#if defined(__linux__) ||                                               \
    (defined(__FreeBSD_kernel__)&&defined(__GLIBC__)&&!defined(__FreeBSD__)) || \
    defined(__CYGWIN__)
#define PXALINUX
#endif

// FreeBSD and NetBSD use the same interface. Let's call this PXAFREEBSD
#if defined(__FreeBSD__) || defined(__NetBSD__)
#define PXAFREEBSD
#endif // __FreeBSD__ or __NetBSD__

// Not exactly true for win32, but makes my life easier by avoiding ifdefs in recoll (the calls just
// fail, which is expected)
#if defined(__DragonFly__) || defined(__OpenBSD__) || defined(_WIN32)
#define HAS_NO_XATTR
#endif

#if defined(_MSC_VER)
#define ssize_t int
#endif

// If the platform is not known yet, let this file be empty instead of breaking the compile, this
// will let the build work if the rest of the software is not actually calling us. If it does call
// us, the undefined symbols will bring attention to the necessity of a port.
//
// If the platform is known but does not support extattrs (e.g.__OpenBSD__), just let the methods
// return errors (like they would on a non-xattr fs on e.g. linux), no need to break the build in
// this case.

// All known systems/families: implement the interface, possibly by returning errors if the system
// is not supported
#if defined(PXAFREEBSD) || defined(PXALINUX) || defined(__APPLE__) || defined(HAS_NO_XATTR)

#include "pxattr.h"

#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(PXAFREEBSD)
#include <sys/extattr.h>
#include <sys/uio.h>
#elif defined(PXALINUX)
#include <sys/xattr.h>
#elif defined(__APPLE__)
#include <sys/xattr.h>
#elif defined(HAS_NO_XATTR)
#else
#error "Unknown system can't compile"
#endif


#ifndef PRETEND_USE
#define PRETEND_USE(var) ((void)var)
#endif

using std::string;
using std::vector;

namespace pxattr {

class AutoBuf {
public:
    char *buf;
    AutoBuf() : buf(nullptr) {}
    ~AutoBuf() {if (buf) free(buf); buf = nullptr;}
    bool alloc(int n) {
        if (buf) {
            free(buf);
            buf = nullptr;
        }
        buf = (char *)malloc(n); 
        return buf != nullptr;
    }
};

static bool get(int fd, const string& path, const string& _name, string *value,
                flags flags, nspace dom)
{
    string name;
    if (!sysname(dom, _name, &name)) 
        return false;

    ssize_t ret = -1;
    AutoBuf buf;
#if defined(PXAFREEBSD)
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = extattr_get_link(path.c_str(), EXTATTR_NAMESPACE_USER, name.c_str(), 0, 0);
        } else {
            ret = extattr_get_file(path.c_str(), EXTATTR_NAMESPACE_USER, name.c_str(), 0, 0);
        }
    } else {
        ret = extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, name.c_str(), 0, 0);
    }
    if (ret < 0)
        return false;
    if (!buf.alloc(ret+1)) // Don't want to deal with possible ret=0
        return false;
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = extattr_get_link(path.c_str(), EXTATTR_NAMESPACE_USER, name.c_str(), buf.buf, ret);
        } else {
            ret = extattr_get_file(path.c_str(), EXTATTR_NAMESPACE_USER, name.c_str(), buf.buf, ret);
        }
    } else {
        ret = extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, name.c_str(), buf.buf, ret);
    }
#elif defined(PXALINUX)
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = lgetxattr(path.c_str(), name.c_str(), nullptr, 0);
        } else {
            ret = getxattr(path.c_str(), name.c_str(), nullptr, 0);
        }
    } else {
        ret = fgetxattr(fd, name.c_str(), nullptr, 0);
    }
    if (ret < 0)
        return false;
    if (!buf.alloc(ret+1)) // Don't want to deal with possible ret=0
        return false;
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = lgetxattr(path.c_str(), name.c_str(), buf.buf, ret);
        } else {
            ret = getxattr(path.c_str(), name.c_str(), buf.buf, ret);
        }
    } else {
        ret = fgetxattr(fd, name.c_str(), buf.buf, ret);
    }
#elif defined(__APPLE__)
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = getxattr(path.c_str(), name.c_str(), 0, 0, 0, XATTR_NOFOLLOW);
        } else {
            ret = getxattr(path.c_str(), name.c_str(), 0, 0, 0, 0);
        }
    } else {
        ret = fgetxattr(fd, name.c_str(), 0, 0, 0, 0);
    }
    if (ret < 0)
        return false;
    if (!buf.alloc(ret+1)) // Don't want to deal with possible ret=0
        return false;
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = getxattr(path.c_str(), name.c_str(), buf.buf, ret, 0, XATTR_NOFOLLOW);
        } else {
            ret = getxattr(path.c_str(), name.c_str(), buf.buf, ret, 0, 0);
        }
    } else {
        ret = fgetxattr(fd, name.c_str(), buf.buf, ret, 0, 0);
    }
#else
    PRETEND_USE(fd);
    PRETEND_USE(flags);
    PRETEND_USE(path);
    errno = ENOTSUP;
#endif

    if (ret >= 0)
        value->assign(buf.buf, ret);
    return ret >= 0;
}

static bool set(int fd, const string& path, const string& _name, 
                const string& value, flags flags, nspace dom)
{
    string name;
    if (!sysname(dom, _name, &name)) 
        return false;

    ssize_t ret = -1;

#if defined(PXAFREEBSD)
    
    if (flags & (PXATTR_CREATE|PXATTR_REPLACE)) {
        // Need to test existence
        bool exists = false;
        ssize_t eret;
        if (fd < 0) {
            if (flags & PXATTR_NOFOLLOW) {
                eret = extattr_get_link(path.c_str(), EXTATTR_NAMESPACE_USER, name.c_str(), 0, 0);
            } else {
                eret = extattr_get_file(path.c_str(), EXTATTR_NAMESPACE_USER, name.c_str(), 0, 0);
            }
        } else {
            eret = extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, name.c_str(), 0, 0);
        }
        if (eret >= 0)
            exists = true;
        if (eret < 0 && errno != ENOATTR)
            return false;
        if ((flags & PXATTR_CREATE) && exists) {
            errno = EEXIST;
            return false;
        }
        if ((flags & PXATTR_REPLACE) && !exists) {
            errno = ENOATTR;
            return false;
        }
    }
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = extattr_set_link(path.c_str(), EXTATTR_NAMESPACE_USER,
                                   name.c_str(), value.c_str(), value.length());
        } else {
            ret = extattr_set_file(path.c_str(), EXTATTR_NAMESPACE_USER, 
                                   name.c_str(), value.c_str(), value.length());
        }
    } else {
        ret = extattr_set_fd(fd, EXTATTR_NAMESPACE_USER,
                             name.c_str(), value.c_str(), value.length());
    }
#elif defined(PXALINUX)
    int opts = 0;
    if (flags & PXATTR_CREATE)
        opts = XATTR_CREATE;
    else if (flags & PXATTR_REPLACE)
        opts = XATTR_REPLACE;
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = lsetxattr(path.c_str(), name.c_str(), value.c_str(), value.length(), opts);
        } else {
            ret = setxattr(path.c_str(), name.c_str(), value.c_str(), value.length(), opts);
        }
    } else {
        ret = fsetxattr(fd, name.c_str(), value.c_str(), value.length(), opts);
    }
#elif defined(__APPLE__)
    int opts = 0;
    if (flags & PXATTR_CREATE)
        opts = XATTR_CREATE;
    else if (flags & PXATTR_REPLACE)
        opts = XATTR_REPLACE;
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = setxattr(path.c_str(), name.c_str(), value.c_str(),
                           value.length(),  0, XATTR_NOFOLLOW|opts);
        } else {
            ret = setxattr(path.c_str(), name.c_str(), value.c_str(), value.length(),  0, opts);
        }
    } else {
        ret = fsetxattr(fd, name.c_str(), value.c_str(), value.length(), 0, opts);
    }
#else
    PRETEND_USE(fd);
    PRETEND_USE(flags);
    PRETEND_USE(value);
    PRETEND_USE(path);
    errno = ENOTSUP;
#endif
    return ret >= 0;
}

static bool del(int fd, const string& path, const string& _name, flags flags, nspace dom) 
{
    string name;
    if (!sysname(dom, _name, &name)) 
        return false;

    int ret = -1;

#if defined(PXAFREEBSD)
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = extattr_delete_link(path.c_str(), EXTATTR_NAMESPACE_USER, name.c_str());
        } else {
            ret = extattr_delete_file(path.c_str(), EXTATTR_NAMESPACE_USER, name.c_str());
        }
    } else {
        ret = extattr_delete_fd(fd, EXTATTR_NAMESPACE_USER, name.c_str());
    }
#elif defined(PXALINUX)
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = lremovexattr(path.c_str(), name.c_str());
        } else {
            ret = removexattr(path.c_str(), name.c_str());
        }
    } else {
        ret = fremovexattr(fd, name.c_str());
    }
#elif defined(__APPLE__)
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = removexattr(path.c_str(), name.c_str(), XATTR_NOFOLLOW);
        } else {
            ret = removexattr(path.c_str(), name.c_str(), 0);
        }
    } else {
        ret = fremovexattr(fd, name.c_str(), 0);
    }
#else
    PRETEND_USE(fd);
    PRETEND_USE(flags);
    PRETEND_USE(path);
    errno = ENOTSUP;
#endif
    return ret >= 0;
}

static bool list(int fd, const string& path, vector<string>* names, flags flags, nspace)
{
    ssize_t ret = -1;
    AutoBuf buf;

#if defined(PXAFREEBSD)
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = extattr_list_link(path.c_str(), EXTATTR_NAMESPACE_USER, 0, 0);
        } else {
            ret = extattr_list_file(path.c_str(), EXTATTR_NAMESPACE_USER, 0, 0);
        }
    } else {
        ret = extattr_list_fd(fd, EXTATTR_NAMESPACE_USER, 0, 0);
    }
    if (ret < 0) 
        return false;
    if (!buf.alloc(ret+1)) // NEEDED on FreeBSD (no ending null)
        return false;
    buf.buf[ret] = 0;
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = extattr_list_link(path.c_str(), EXTATTR_NAMESPACE_USER, buf.buf, ret);
        } else {
            ret = extattr_list_file(path.c_str(), EXTATTR_NAMESPACE_USER, buf.buf, ret);
        }
    } else {
        ret = extattr_list_fd(fd, EXTATTR_NAMESPACE_USER, buf.buf, ret);
    }
#elif defined(PXALINUX)
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = llistxattr(path.c_str(), nullptr, 0);
        } else {
            ret = listxattr(path.c_str(), nullptr, 0);
        }
    } else {
        ret = flistxattr(fd, nullptr, 0);
    }
    if (ret < 0) 
        return false;
    if (!buf.alloc(ret+1)) // Don't want to deal with possible ret=0
        return false;
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = llistxattr(path.c_str(), buf.buf, ret);
        } else {
            ret = listxattr(path.c_str(), buf.buf, ret);
        }
    } else {
        ret = flistxattr(fd, buf.buf, ret);
    }
#elif defined(__APPLE__)
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = listxattr(path.c_str(), 0, 0, XATTR_NOFOLLOW);
        } else {
            ret = listxattr(path.c_str(), 0, 0, 0);
        }
    } else {
        ret = flistxattr(fd, 0, 0, 0);
    }
    if (ret < 0) 
        return false;
    if (!buf.alloc(ret+1)) // Don't want to deal with possible ret=0
        return false;
    if (fd < 0) {
        if (flags & PXATTR_NOFOLLOW) {
            ret = listxattr(path.c_str(), buf.buf, ret, XATTR_NOFOLLOW);
        } else {
            ret = listxattr(path.c_str(), buf.buf, ret, 0);
        }
    } else {
        ret = flistxattr(fd, buf.buf, ret, 0);
    }
#else
    PRETEND_USE(fd);
    PRETEND_USE(flags);
    PRETEND_USE(path);
    errno = ENOTSUP;
#endif

    if (ret < 0)
        return false;

    char *bufstart = buf.buf;

    // All systems return a 0-separated string list except FreeBSD/NetBSD which has length, value
    // pairs, length is a byte.
#if defined(PXAFREEBSD)
    char *cp = buf.buf;
    unsigned int len;
    while (cp < buf.buf + ret) {
        len = *cp;
        *cp = 0;
        cp += len + 1;
    }
    bufstart = buf.buf + 1;
    *cp = 0; // don't forget, we allocated one more
#endif

    if (ret > 0) {
        size_t pos = 0;
        while (pos < static_cast<size_t>(ret)) {
            string n = string(bufstart + pos);
            string n1;
            if (pxname(PXATTR_USER, n, &n1)) {
                names->push_back(n1);
            }
            pos += n.length() + 1;
        }
    }
    return true;
}

static const string nullstring("");

bool get(const string& path, const string& _name, string *value, flags flags, nspace dom)
{
    return get(-1, path, _name, value, flags, dom);
}
bool get(int fd, const string& _name, string *value, flags flags, nspace dom)
{
    return get(fd, nullstring, _name, value, flags, dom);
}
bool set(const string& path, const string& _name, const string& value, flags flags, nspace dom)
{
    return set(-1, path, _name, value, flags, dom);
}
bool set(int fd, const string& _name, const string& value, flags flags, nspace dom)
{
    return set(fd, nullstring, _name, value, flags, dom);
}
bool del(const string& path, const string& _name, flags flags, nspace dom) 
{
    return del(-1, path, _name, flags, dom);
}
bool del(int fd, const string& _name, flags flags, nspace dom) 
{
    return del(fd, nullstring, _name, flags, dom);
}
bool list(const string& path, vector<string>* names, flags flags, nspace dom)
{
    return list(-1, path, names, flags, dom);
}
bool list(int fd, vector<string>* names, flags flags, nspace dom)
{
    return list(fd, nullstring, names, flags, dom);
}

#if defined(PXALINUX) || defined(COMPAT1)
static const string userstring("user.");
#else
static const string userstring("");
#endif
bool sysname(nspace dom, const string& pname, string* sname)
{
    if (dom != PXATTR_USER) {
        errno = EINVAL;
        return false;
    }
    *sname = userstring + pname;
    return true;
}

bool pxname(nspace, const string& sname, string* pname) 
{
    if (!userstring.empty() && sname.find(userstring) != 0) {
        errno = EINVAL;
        return false;
    }
    *pname = sname.substr(userstring.length());
    return true;
}

} // namespace pxattr


#endif // Known systems.
