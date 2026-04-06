#!/usr/bin/env python

from recoll import rclconfig

def trstack():
    conf = rclconfig.ConfStack("recoll.conf",
                               ('/home/dockes/.recoll/',
                                '/usr/local/share/recoll/examples/'))
    nms = ('topdirs', 'thrQSizes', 'skippedNames', 'loglevel')
    for nm in nms:
        print nm, "=>", conf.get(nm)

def trtree(fname):
    conf = rclconfig.ConfTree(fname, True)

    #nsk = (('intoto', '/a/b/'), ('intoto', ''), ('intoto', '/a/b/c'), ('intoto', '/c'))
    nsk = (('intoto', ''), ('intoto', '/'), ('intoto', '/a/b/c'),
           ('intoto', 'a'))
    for nm, sk in nsk:
        print sk, nm, "=>", conf.get(nm, sk)
        print

def trsimple(fname):
    conf = rclconfig.ConfSimple(fname)
    nms = ('thrQSizes',)
    for nm in nms:
        print nm, "=>", conf.get(nm)
    
#    nsk = (('intoto', '/toto'), ('intoto', ''))
#    for nm, sk in nsk:
#        print sk, nm, "=>", conf.get(nm, sk)

def trconfig():
    config = rclconfig.RclConfig()
    names = ("topdirs", "indexallfilenames")
    for nm in names:
        print "%s: [%s]" % (nm, config.getConfParam(nm))

def trextdbs():
    config = rclconfig.RclConfig("/home/dockes/.recoll-prod")
    extradbs = rclconfig.RclExtraDbs(config)
    print extradbs.getActDbs()
    
#trconfig()
#trsimple(sys.argv[1])

trextdbs()
