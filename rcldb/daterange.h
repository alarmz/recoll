#ifndef _DATERANGE_H_INCLUDED_
#define _DATERANGE_H_INCLUDED_

#include <xapian.h>

using namespace std;

static const string xapday_prefix = "D";
static const string xapmonth_prefix = "M";
static const string xapyear_prefix = "Y";
namespace Rcl {
extern Xapian::Query date_range_filter(int y1, int m1, int d1, int y2, int m2, int d2);
}

#ifdef EXT4_BIRTH_TIME
static const string xapbriday_prefix = "BD";
static const string xapbrimonth_prefix = "BM";
static const string xapbriyear_prefix = "BY";
namespace Rcl {
extern Xapian::Query brdate_range_filter(int y1, int m1, int d1, int y2, int m2, int d2);
}
#endif
#endif /* _DATERANGE_H_INCLUDED_ */
