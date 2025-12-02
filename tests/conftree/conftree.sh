#!/usr/bin/env python3
# Yep, Actually a python script. Test the Python conftree module

import sys
import os
import shutil
import platform
import subprocess

def msg(s):
    print(f"{s}", file=sys.stderr)

### Doing some of the work that shared.sh does for the shell-based tests
if "TMPDIR" in os.environ:
    tmpdir = os.environ["TMPDIR"]
else:
    tmpdir = "/tmp"
mydir = os.path.dirname(__file__)
reffn = os.path.join(mydir, "conftree.txt")
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

## Done with the shared.sh stuff

import conftree


outputfd = open(outputfn, "w")

def output(s):
    print(f"{s}", file=outputfd)
    
# The test input config
configfn = os.path.join(mydir, "cftest.conf")

# Set casesensitive to False. Parameter names will always be case-insensitive. Submap names
# sensitivity will depend on the platform as these are filesystem paths.
conf = conftree.ConfTree(configfn, casesensitive = False)

output("Testing gets in top level, variable names should not be case-sensitive")
for nm, ref in (("skippedNames-", "recoll.ini"),
                ("SKIPPEDNAMES-", "recoll.ini"),
                (b"skippedNames-", b"recoll.ini"),
                ):
    value = conf.get(nm)
    if value != ref:
        msg(f"get({nm}) should be [{ref}] but got [{value}]")
        sys.exit(1)
    output(f"get({nm}) -> {value}")


output("\nTesting gets in submaps, variable names should not be case-sensitive.\n"
       " submap names are case-sensitive or not depending on platform fs case-sensitivity"
       )
smtuples = [("submapvar", "/some/PATH", "submapvarvalue"),
            ("smvar1", "/some/PATH", "smvar1value"),
            ("smvar2", "/some/PATH", "smvar2value"),
            ("smvar2", "/some/", "smvar2valueup"),
            (b"SubmapVAR", "/some/PATH", b"submapvarvalue"),
            ("submapvar", "", None),
            ("skippedNames-", "/some/PATH", "recoll.ini"),
            ]

if platform.system() in ("Windows", "Darwin"):
    smtuples.append(("SUBMAPVAR", "/some/path", "submapvarvalue"))
else:
    smtuples.append(("SUBMAPVAR", "/some/path", None))

for nm, sk, ref in smtuples:
    value = conf.get(nm, sk)
    if value != ref:
        msg(f"get({nm}, {sk}) should be [{ref}] but got [{value}]")
        sys.exit(1)
    output(f"get({nm}, {sk}) -> {value}")
    
outputfd.close()


diffd = open(difffn, "w")
cmd = ["diff", "-u", "-w", reffn, outputfn]
subprocess.run(cmd, stdout = diffd)
    
