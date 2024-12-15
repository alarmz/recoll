#!/bin/sh

VERSION=`cat RECOLL-VERSION.txt`
DATE=`ls --time-style=long-iso -l RECOLL-VERSION.txt | awk '{print $6}'`

sed -i -E -e '/^#define[ \t]+PACKAGE_VERSION/c\'\
"#define PACKAGE_VERSION \"$VERSION\"" \
common/autoconfig-win.h common/autoconfig-mac.h

sed -i -E -e '/VERSIONCOMMENT/c\'\
"    version: '$VERSION', # VERSIONCOMMENT keep this here, used by setversion.sh" \
meson.build

sed -i -E -e '/<release version=/c\'\
"    <release version=\"$VERSION\" date=\"$DATE\">" \
desktop/org.recoll.recoll.appdata.xml
