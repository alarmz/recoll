import sys
import re
import lxml.etree as ET

class MetaFixer(object):
    def __init__(self):
        pass

#    def metafixelt(self, nm, xml):
#        print("MetaFixer: metafixelt: %s" % ET.tostring(xml), file=sys.stderr)
#        return "hello"
    
    def metafix(self, nm, txt):
        print("Metafixer: mfix: nm [%s] txt [%s]" % (nm, txt), file=sys.stderr)
        if nm == 'pdf:Producer':
            txt += " metafixerunique"
        elif nm == 'someothername':
            # do something else
            pass
        elif nm == 'stillanother':
            # etc.
            pass
        
        return txt

    def wrapup(self, metaheaders):
        #print("Metafixer: wrapup: %s" % metaheaders, file=sys.stderr)
        pass
