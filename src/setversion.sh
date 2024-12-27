#!/bin/sh

# If VERSION is experimental like 1.40.0pre1, can't keep the pre1 for meson
# because it gets into the shared lib version. 
VERSION=`cat RECOLL-VERSION.txt`
VERSIONCLEAN=`cat RECOLL-VERSION.txt | sed -e 's/pre.*//'`
DATE=`ls --time-style=long-iso -l RECOLL-VERSION.txt | awk '{print $6}'`
SOVERSION=`cat RECOLL-SOVERSION.txt`

sed -i -E -e '/^#define[ \t]+PACKAGE_VERSION/c\'\
"#define PACKAGE_VERSION \"$VERSION\"" \
common/autoconfig-win.h common/autoconfig-mac.h

sed -i -E -e '/VERSIONCOMMENT/c\'\
"    version: '$VERSIONCLEAN', # VERSIONCOMMENT keep this here, used by setversion.sh" \
meson.build
sed -i -E -e '/SONAMECOMMENT/c\'\
"recoll_soversion = '$SOVERSION' # SONAMECOMMENT keep this here, used by setversion.sh" \
meson.build


## This sort of works for extracting the changelog lines, but it would
## be complicated to make it really right because of multiline entries
## etc.
#changelines=`awk '/^recoll/{flag = 1;next};/^ --/{exit} {if ($0=="") next;printf "<li>%s</li>\\\\\n", $0}' \
#  ../packaging/debian/debian/changelog `
#cat <<EOF
#$changelines
#EOF

sed -i -E -e "/<releases>/a\
\ \ \ \ <release version=\"$VERSION\" date=\"$DATE\">\\
      <description>\\
        <ul>\\
          <li>New recoll release</li>\\
        </ul>\\
      </description>\\
    </release>" \
desktop/org.recoll.recoll.appdata.xml
