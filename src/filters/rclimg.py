#!/usr/bin/env python3

# Python version of the Image Tag extractor for Recoll, using pyexiv2. This is less thorough than
# the rclimg Perl version using exiftool but may still be useful if you don't want to use Perl.
#
# The Python extension interfaces with the C++ libexiv2 library by using Boost-Python, which is
# cumbersome. Its default build uses g++ which probably makes it incompatible with the default
# Windows Python distributions. I did not try to port it on Windows, where we use the more basic
# piexif-based rclimg1.py for running OCR.
#
# This has been extended to support running OCR, which the Perl script does not do.


import sys
import os
import re
import subprocess

import rclexecm
from rclbasehandler import RclBaseHandler

try:
    import pyexiv2
except:
    print("RECFILTERROR HELPERNOTFOUND python3:pyexiv2")
    sys.exit(1)

# Hex number regexp, used to skip undecoded/unknown keys
khexre = re.compile(r".*\.0[xX][0-9a-fA-F]+$")

pyexiv2_titles = {
    "Xmp.dc.subject",
    "Xmp.dc.title",
    "Xmp.lr.hierarchicalSubject",
    "Xmp.MicrosoftPhoto.LastKeywordXMP",
}

iptc_titles = {
    "Iptc.Application2.Headline",
    "Iptc.Application2.Caption",
    }

# Keys for which we set meta tags
meta_pyexiv2_keys = {
    "Xmp.dc.subject",
    "Xmp.lr.hierarchicalSubject",
    "Xmp.MicrosoftPhoto.LastKeywordXMP",
    "Xmp.digiKam.TagsList",
    "Exif.Photo.DateTimeDigitized",
    "Exif.Photo.DateTimeOriginal",
    "Exif.Image.DateTime",
}

exiv2_dates = [
    "Exif.Photo.DateTimeOriginal",
    "Exif.Image.DateTime",
    "Exif.Photo.DateTimeDigitized",
]


class ImgTagExtractor(RclBaseHandler):
    def __init__(self, em):
        super(ImgTagExtractor, self).__init__(em)
        self.config = self.em.config()
        
    def html_text(self, filename):
        ok = False

        metadata = pyexiv2.ImageMetadata(filename)
        metadata.read()

        #### Head stuff: use fields which we know to handle as title, keywords or dates.
        keys = metadata.exif_keys + metadata.iptc_keys + metadata.xmp_keys
        mdic = {}
        for k in keys:
            # we skip numeric keys and undecoded makernote data
            if k != "Exif.Photo.MakerNote" and not khexre.match(k):
                mdic[k] = str(metadata[k].raw_value)

        docdata = b"<html><head>\n"

        # Look for title data, collapsing identical values.
        ttdata = set()
        for k in pyexiv2_titles.union(iptc_titles):
            if k in mdic:
                ttdata.add(rclexecm.htmlescape(mdic[k]))
        if ttdata:
            title = ""
            for v in ttdata:
                v = v.replace("[", "").replace("]", "").replace("'", "")
                title += v + " "
            docdata += rclexecm.makebytes("<title>" + title + "</title>\n")

        # Dates
        for k in exiv2_dates:
            if k in mdic:
                # Recoll wants: %Y-%m-%d %H:%M:%S.
                # We get 2014:06:27 14:58:47
                dt = mdic[k].replace(":", "-", 2)
                docdata += (
                    b'<meta name="date" content="' + rclexecm.makebytes(dt) + b'">\n'
                )
                break

        # Keywords
        for k, v in mdic.items():
            if k == "Xmp.digiKam.TagsList":
                docdata += (
                    b'<meta name="keywords" content="'
                    + rclexecm.makebytes(rclexecm.htmlescape(mdic[k]))
                    + b'">\n'
                )

        docdata += b"</head><body>\n"

        #### Body stuff 
        self.config.setKeyDir(os.path.dirname(filename))
        s = self.config.getConfParam("imgocr")

        if rclexecm.configparamtrue(s):
            # Run image OCR
            htmlprefix = b"<H3>O_C_R T_E_X_T</H3>\n<PRE>"
            htmlsuffix = b"</PRE>"
            cmd = [
                sys.executable,
                os.path.join(_execdir, "rclocr.py"),
                filename,
            ]
            try:
                global ocrproc
                ocrproc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
                data, stderr = ocrproc.communicate()
                ocrproc = None
                if len(data) > 1:
                    docdata += htmlprefix + rclexecm.htmlescape(data) + htmlsuffix
            except Exception as e:
                self.em.rclog(f"{cmd} failed: {e}")
                pass

        # Use all extracted fields as main text. Not that useful but ensure that
        # everything is indexed and allows previewing them
        flddata = b""
        for k, v in mdic.items():
            flddata += rclexecm.makebytes(k + " : " + rclexecm.htmlescape(mdic[k]) + "<br />")
        if len(flddata) > 1:
            docdata += b"<H3>F_I_E_L_D_S:</H3>\n<PRE>"
            docdata += flddata
        docdata += b"</body></html>"

        return docdata


if __name__ == "__main__":
    _execdir = os.path.dirname(sys.argv[0])
    proto = rclexecm.RclExecM()
    extract = ImgTagExtractor(proto)
    rclexecm.main(proto, extract)
