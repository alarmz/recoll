#!/usr/bin/env python3
# Yep, Actually a python script. Test the Python rclconfig module

import sys
import os
import shutil
import platform
import subprocess

def msg(s):
    print(f"{s}", file=sys.stderr)

########### Doing some of the work that shared.sh does for the shell-based tests
if "TMPDIR" in os.environ:
    tmpdir = os.environ["TMPDIR"]
else:
    tmpdir = "/tmp"
mydir = os.path.dirname(__file__)
myname = os.path.splitext(os.path.basename(__file__))[0]
reffn = os.path.join(mydir, myname + ".txt")
outputfn = os.path.join(tmpdir, "recolltsttmp", os.path.basename(mydir)+ ".out")
difffn = os.path.join(tmpdir, "recolltsttmp", os.path.basename(mydir)+ ".diff")

# Looking for our Python files. We search for it relative to the recollindex command. Things depend
# on the platform
recollindex = shutil.which("recollindex")
if not recollindex:
    msg("Recollindex not found")
    sys.exit(1)

if platform.system() == "Windows":
    pydir = os.path.join(os.path.dirname(recollindex), "Share", "Filters")
else:
    usrdir = os.path.dirname(recollindex)
    # arg deal with the merged /bin /usr/bin issue
    if usrdir == "/bin":
        usrdir = "/usr/bin"
    usrdir = os.path.dirname(usrdir)
    pydir = os.path.join(usrdir, "share", "recoll", "filters")

sys.path.insert(0, pydir)
sys.path.insert(0, "/home/dockes/projets/fulltext/recoll/src/python/recoll/recoll")

outputfd = open(outputfn, "w")

def output(s):
    print(f"{s}", file=outputfd)

###### Done with the shared.sh stuff
########## Aux funcs

def fconfigure(ifn, ofn, changes):
    with open(ifn, "r") as rf:
        data = rf.read()
    for k,v in changes.items():
        data = data.replace(k, v)
    with open(ofn, "w") as wf:
        wf.write(data)
    
def check(a,b):
    if a != b:
        msg(f"[{a}] should be [{b}]")
        sys.exit(1)

######

# The test input config
configdir = os.path.join(mydir, "config")
os.environ["RECOLL_CONFDIR"] = configdir
docdir = os.path.join(mydir, "docs")
            
mimemap = os.path.join(configdir, "mimemap")
fconfigure(mimemap + ".in", mimemap, {"@MYDIR@": mydir,})
recollconf = os.path.join(configdir, "recoll.conf")
fconfigure(recollconf + ".in", recollconf, {"@MYDIR@": mydir,})

import rclconfig
import conftree

        
config = rclconfig.RclConfig()

cfdir = config.getConfDir()
check(cfdir, configdir)

value = config.getConfParam("logfilename")
check(value, "/home/dockes/tmp/logrcltst")
value = config.getConfParam("LogFileName")
check(value, "/home/dockes/tmp/logrcltst")
value = config.getConfParam(b"LogFileName")
check(value, b"/home/dockes/tmp/logrcltst")

value = config.getConfParam(b"unac_except_trans")
check(value.decode("UTF-8"), "åå")

mime = config.mimeType(os.path.join(docdir, "ex1.xml"))
check(mime, "text/xml")
config.setKeyDir(os.path.join(docdir, "okular-notes"))
mime = config.mimeType(os.path.join(docdir, "okular-notes", "ex2.xml"))
check(mime, "application/x-okular-notes")

value = config.getConfParam("onlyNames")
check(value, "")
config.setKeyDir(os.path.join(mydir, "docs", "onlynames"))
value = config.getConfParam("onlyNames")
value = conftree.stringToStrings(value)
check(value, ["*.matchesonepat", "*.matchestwopat"])

### Dynconf
extradbs = rclconfig.RclExtraDbs(config)
xdbs = extradbs.getActDbs()
check(xdbs, [b'/home/dockes/.recoll-upnp/xapiandb', b'/home/dockes/.recoll/xapiandb'])



############# common post-proc. Here does nothing because we test values and don't write anything to
############# the output at the moment.
outputfd.close()
diffd = open(difffn, "w")
cmd = ["diff", "-u", "-w", reffn, outputfn]
subprocess.run(cmd, stdout = diffd)
    
