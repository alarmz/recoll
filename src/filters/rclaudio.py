#!/usr/bin/env python3
"""Audio extractor for Recoll, using mutagen for metadata and optionally using whisper
for speech to text."""

import sys
import os
import gc
import time
import datetime
import re

import rclexecm
from rclbasehandler import RclBaseHandler
import rclconfig

try:
    import mutagen
    from mutagen import File
    from mutagen.id3 import ID3
except ImportError:
    print("RECFILTERROR HELPERNOTFOUND python3:mutagen")
    sys.exit(1)

re_pairnum = re.compile(r"[([]*([0-9]+),\s*([0-9]+)")

_htmlprefix = b"""<html><head>
<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">
</head><body><pre>"""
_htmlsuffix = b"""</pre></body></html>"""

# The 'Easy' mutagen tags conversions are incomplete. We do it ourselves.
# TPA,TPOS,disc DISCNUMBER/TOTALDISCS
# TRCK,TRK,trkn TRACKNUMBER/TOTALTRACKS
# The conversions here are consistent with the ones in MinimServer (2019-03),
# including the rating stuff and TXXX. Lacking: Itunes '----' handling ?

# The 'GROUP' tag is a specific minimserver tag used to create
# sub-containers inside a folder. We used to use 'CONTENTGROUP' for
# this, which was wrong, the latter is a vaguely defined "music
# category" thing.
tagdict = {
    "ALBUM ARTIST": "ALBUMARTIST",
    "ALBUM": "ALBUM",
    "ALBUMARTIST": "ALBUMARTIST",
    "ALBUMARTISTSORT": "ALBUMARTISTSORT",
    "ALBUMSORT": "ALBUMSORT",
    "ARTIST": "ARTIST",
    "ARTISTSORT": "ARTISTSORT",
    "BPM": "BPM",
    "COM": "COMMENT",
    "COMM": "COMMENT",
    "COMMENT": "COMMENT",
    "COMPILATION": "COMPILATION",
    "COMPOSER": "COMPOSER",
    "COMPOSERSORT": "COMPOSERSORT",
    "CONDUCTOR": "CONDUCTOR",
    "CONTENTGROUP": "CONTENTGROUP",
    "COPYRIGHT": "COPYRIGHT",
    "DATE": "DATE",
    "DISCNUMBER": "DISCNUMBER",
    "DISCSUBTITLE": "DISCSUBTITLE",
    "DISCTOTAL": "TOTALDISCS",
    "ENCODEDBY": "ENCODEDBY",
    "ENSEMBLE": "ORCHESTRA",
    "GENRE": "GENRE",
    "GROUP": "GROUP",
    "ISRC": "ISRC",
    "LABEL": "LABEL",
    "LANGUAGE": "LANGUAGE",
    "LYRICIST": "LYRICIST",
    "LYRICS": "LYRICS",
    "MOOD": "MOOD",
    "ORCHESTRA": "ORCHESTRA",
    "PERFORMER": "PERFORMER",
    "POP": "RATING1",
    "POPM": "RATING1",
    "ORIGINALARTIST": "ORIGINALARTIST",
    "ORIGINALDATE": "ORIGINALDATE",
    "RELEASEDATE": "RELEASEDATE",
    "REMIXER": "REMIXER",
    "SUBTITLE": "SUBTITLE",
    "TAL": "ALBUM",
    "TALB": "ALBUM",
    "TBP": "BPM",
    "TBPM": "BPM",
    "TCM": "COMPOSER",
    "TCMP": "COMPILATION",
    "TCO": "GENRE",
    "TCOM": "COMPOSER",
    "TCON": "GENRE",
    "TCOP": "COPYRIGHT",
    "TCP": "COMPILATION",
    "TCR": "COPYRIGHT",
    "TDA": "DATE",
    "TDAT": "DATE",
    "TDOR": "ORIGINALDATE",
    "TDRC": "DATE",
    "TDRL": "RELEASEDATE",
    "TEN": "ENCODEDBY",
    "TENC": "ENCODEDBY",
    "TEXT": "LYRICIST",
    "TIT1": "CONTENTGROUP",
    "TIT2": "TITLE",
    "TIT3": "SUBTITLE",
    "TITLE": "TITLE",
    "TITLESORT": "TITLESORT",
    "TLA": "LANGUAGE",
    "TLAN": "LANGUAGE",
    "TMOO": "MOOD",
    "TOA": "ORIGINALARTIST",
    "TOPE": "ORIGINALARTIST",
    "TOR": "ORIGINALDATE",
    "TORY": "ORIGINALDATE",
    "TOTALDISCS": "TOTALDISCS",
    "TOTALTRACKS": "TOTALTRACKS",
    "TP1": "ARTIST",
    "TP2": "ALBUMARTIST",
    "TP3": "CONDUCTOR",
    "TP4": "REMIXER",
    "TPA": "DISCNUMBER",
    "TPB": "LABEL",
    "TPE1": "ARTIST",
    "TPE2": "ALBUMARTIST",
    "TPE3": "CONDUCTOR",
    "TPE4": "REMIXER",
    "TPOS": "DISCNUMBER",
    "TPUB": "LABEL",
    "TRACK": "TRACKNUMBER",
    "TRACKNUM": "TRACKNUMBER",
    "TRACKNUMBER": "TRACKNUMBER",
    "TRACKTOTAL": "TOTALTRACKS",
    "TRC": "ISRC",
    "TRCK": "TRACKNUMBER",
    "TRDA": "DATE",
    "TRK": "TRACKNUMBER",
    "TS2": "ALBUMARTISTSORT",
    "TSA": "ALBUMSORT",
    "TSC": "COMPOSERSORT",
    "TSO2": "ALBUMARTISTSORT",
    "TSOA": "ALBUMSORT",
    "TSOC": "COMPOSERSORT",
    "TSOP": "ARTISTSORT",
    "TSOT": "TITLESORT",
    "TSP": "ARTISTSORT",
    "TSRC": "ISRC",
    "TSST": "DISCSUBTITLE",
    "TST": "TITLESORT",
    "TT1": "CONTENTGROUP",
    "TT2": "TITLE",
    "TT3": "SUBTITLE",
    "TXT": "LYRICIST",
    "TXXX:ORCHESTRA": "ORCHESTRA",
    "TXX:ORCHESTRA": "ORCHESTRA",
    "TYE": "DATE",
    "TYER": "DATE",  # wikipedia id3: YEAR
    "ULT": "LYRICS",
    "USLT": "LYRICS",
    "SLT": "LYRICS",
    "SYLT": "LYRICS",
    "YEAR": "DATE",
    "aART": "ALBUMARTIST",
    "cond": "CONDUCTOR",
    "cpil": "COMPILATION",
    "cprt": "COPYRIGHT",
    "disk": "DISCNUMBER",
    "gnre": "GENRE",
    "labl": "LABEL",
    "soaa": "ALBUMARTISTSORT",
    "soal": "ALBUMSORT",
    "soar": "ARTISTSORT",
    "soco": "COMPOSERSORT",
    "sonm": "TITLESORT",
    "tmpo": "BPM",
    "trkn": "TRACKNUMBER",
    "\xa9ART": "ARTIST",
    "\xa9alb": "ALBUM",
    "\xa9cmt": "COMMENT",
    "\xa9con": "CONDUCTOR",
    "\xa9day": "DATE",
    "\xa9gen": "GENRE",
    "\xa9grp": "CONTENTGROUP",
    "\xa9lyr": "LYRICS",
    "\xa9nam": "TITLE",
    "\xa9ope": "ORIGINALARTIST",
    "\xa9too": "ENCODEDBY",
    "\xa9wrt": "COMPOSER",
}


def tobytes(s):
    if type(s) == type(b""):
        return s
    if type(s) != type(""):
        s = str(s)
    return s.encode("utf-8", errors="replace")


# mp3:      album, title, artist, genre, date, tracknumber
# flac:     album, title, artist, genre, xxx, tracknumber
# oggvorbis:album, title, artist, genre, date, tracknumber
class AudioTagExtractor(RclBaseHandler):

    def __init__(self, em):
        """Set tag fixing up."""
        super(AudioTagExtractor, self).__init__(em)
        self.config = rclconfig.RclConfig()
        self.recoll_confdir = self.config.confdir
        self.process_stt = False
        self.preview_mode = os.environ.get("RECOLL_FILTER_FORPREVIEW", "no")
        if self.config.getConfParam("speechtotext") == "whisper":
            self.process_stt = True

        tagfixerfn = self.config.getConfParam("audiotagfixerscript")
        self.tagfix = None
        if tagfixerfn:
            import runpy

            try:
                d = runpy.run_path(tagfixerfn)
                self.tagfix = d["tagfix"]
                self.tagfix()
            except Exception as ex:
                self.em.rclog("tagfix script import failed: %s" % ex)
                pass

    def _showMutaInfo(self, mutf):
        self.em.rclog("%s" % mutf.info.pprint())
        for prop in dir(mutf.info):
            self.em.rclog(f"mutinfo: {prop} -> {getattr( mutf.info, prop)}")

    def _fixrating(self, minf):
        if "RATING1" in minf:
            if not "RATING" in minf:
                val = int(minf["RATING1"]) // 51 + 1
                if val > 5:
                    val = 5
                if val < 1:
                    val = 1
                minf["RATING"] = str(val)
            del minf["RATING1"]

    def _embeddedImageFormat(self, mutf):
        # self.em.rclog("_embeddedImage: MIME: %s"%mutf.mime)
        try:
            # This fails if we're passed a mutagen.ID3 instead of File
            mime = mutf.mime
        except:
            mime = []

        # First pretend that this is an ID3. These can come inside multiple file formats, so don't
        # try to select on mime.
        for tagname in mutf.keys():
            if tagname.startswith("APIC:"):
                # self.em.rclog("mp3 img: %s" % mutf[tagname].mime)
                return "jpg" if mutf[tagname].mime == "image/jpeg" else "png"

        if "audio/flac" in mime:
            if mutf.pictures:
                return "jpg" if mutf.pictures[0].mime == "image/jpeg" else "png"
        elif "audio/mp4" in mime:
            if "covr" in mutf.keys():
                format = mutf["covr"][0].imageformat
                if format == mutagen.mp4.AtomDataType.JPEG:
                    return "jpg"
                else:
                    return "png"
        return ""

    def parsedate(self, dt):
        """Try to make sense of data found in the date fields.

        Date formats found in actual files (any date field):
        [1961] [1967-01-01] [1996-11-04T08:00:00Z] [] [0000]  [1994-08-08 07:00]

        We don't try to process the time part.
        The method translates the date into a Unix timestamp
        which means possible trouble for pre-1970 recordings (negative time).
        Oldest possible date with 32 bits time stamp is 1901, which is ok though.

        Previous recoll versions had an alias from date to dmtime, which
        was wrong, because dmtime is the unix integer time. We have
        removed the alias, and set dmtime from the parsed date value.
        """
        try:
            dt = dt.decode("utf-8", errors="ignore")
            if len(dt) > 10:
                dt = dt[0:10]
            date_parts = dt.split("-")
            if (
                len(date_parts) > 3
                or len(date_parts) == 2
                or len(date_parts[0]) != 4
                or date_parts[0] == "0000"
            ):
                return ""
            if len(date_parts) == 1:
                pdt = datetime.datetime.strptime(dt, "%Y")
            elif len(date_parts) == 3:
                pdt = datetime.datetime.strptime(dt, "%Y-%m-%d")
            val = time.mktime(pdt.timetuple())
            return b"%d" % val
        except:
            return b"0"

    def _extractaudioparams(self, filename, minf, mutf):
        """Extract audio parameters from mutagen output.

        Not all file types supply all or even use the same property names.
        Translate to consistent str keys and encoded values into our fields dict.
        """
        for prop, dflt in [
            ("sample_rate", 44100),
            ("channels", 2),
            ("length", 0),
            ("bitrate", 0),
        ]:
            try:
                minf[prop] = getattr(mutf.info, prop)
            except Exception as e:
                # self.em.rclog("NO %s prop: %s" % (prop, e))
                minf[prop] = dflt

        if minf["bitrate"] == 0 and minf["length"] > 0:
            br = int(os.path.getsize(filename) * 8 / minf["length"])
            minf["bitrate"] = br

        minf["duration"] = minf["length"]
        del minf["length"]

        # Bits/samp is named sample_size or bits_per_sample (depend on file tp)
        try:
            minf["bits_per_sample"] = getattr(mutf.info, "bits_per_sample")
        except:
            try:
                minf["bits_per_sample"] = getattr(mutf.info, "sample_size")
            except:
                # self.em.rclog("using default bits_per_sample")
                minf["bits_per_sample"] = 16

        for tag, val in minf.items():
            minf[tag] = tobytes(val)

    def _fixtagname(self, tag):
        """Translate native tag name to our filetype-independant ones.

        Mostly uses the big tagdict dictionary, with other adjustments.
        """
        # Variations on the COMM tag:
        #   - "COMM::eng" We don't know what to do with the language (or possible other attributes),
        #     so get rid of it for now.
        #   - Also possible: "COMM:ID3v1 Comment:eng" "COMM:iTunNORM:eng" "COMM:Performers:eng"
        if tag.find("COMM:") == 0:
            tag = "COMM"
        #     TXXX:TOTALTRACKS TXXX:ORCHESTRA
        elif tag.find("TXXX:") == 0:
            tag = tag[5:]
            if tag.startswith("QuodLibet::"):
                tag = tag[11:]
        elif tag.find("TXX:") == 0:
            tag = tag[4:]

        if tag.upper() in tagdict:
            tag = tag.upper()

        if tag in tagdict:
            # self.em.rclog("Original tag: <%s>, type0 %s val <%s>" % (tag, type(val), val))
            ntag = tagdict[tag].lower()
            # self.em.rclog("New tag: %s" % ntag)
        else:
            if not tag.isalnum():
                return None
            ntag = tag.lower()
            # self.em.rclog(f"Processing unexpected tag: {tag}, value {val}")
        return ntag

    def _processdiscortracknumber(self, minf, ntag, val):
        """Disc and track numbers are special, and output in varying ways."""
        # TPA,TPOS,disc DISCNUMBER/TOTALDISCS
        # TRCK,TRK,trkn TRACKNUMBER/TOTALTRACKS
        for what in ("disc", "track"):
            k = what + "number"
            if ntag == k:
                if isinstance(val, list):
                    val = val[0]
                if not isinstance(val, tuple):
                    val = str(val)
                    mo = re_pairnum.match(val)
                    if mo:
                        val = (mo.group(1), mo.group(2))
                    else:
                        val = val.split("/")
                else:
                    # self.em.rclog(f"{k} : tuple: {val} tp1 {type(val[0])} tp2 {type(val[1])}")
                    pass
                minf[k] = tobytes(val[0])
                if len(val) == 2 and val[1] != 0:
                    k1 = "total" + what + "s"
                    # self.em.rclog(f"Setting {k1} : {val[1]}")
                    minf[k1] = tobytes(val[1])
                # self.em.rclog(f"{k} finally: {minf[k]}")
                return True
        return False

    def _processtags(self, mutf, minf, filename):
        """Extract and process metadata tags."""
        for tag, val in mutf.items():
            # self.em.rclog(f"TAG <{tag}> VALUE <{val}>")
            # The names vary depending on the file type. Make them consistent
            ntag = self._fixtagname(tag)
            if not ntag:
                continue
            # self.em.rclog(f"Translated tag: <{ntag}>")

            if self._processdiscortracknumber(minf, ntag, val):
                continue

            try:
                # id3v2.4 can send multiple values as 0-separated strings. Change this to a
                # list. mutagen should probably do this for us... Some file types do return lists
                # of values, e.g. FLAC.
                if not isinstance(val, (list, tuple)):
                    val = str(val)
                    val = val.split("\x00")
                # Change list to string for sending up to recoll.
                try:
                    if isinstance(
                        val,
                        (
                            list,
                            tuple,
                        ),
                    ):
                        if isinstance(val[0], str):
                            val = " | ".join(val)
                        else:
                            # Usually mp4.MP4Cover: ignore
                            # self.em.rclog(f"Got value for {ntag} which is list of "
                            #              f"non-strings: {type(val[0])}")
                            continue
                except Exception as err:
                    self.em.rclog(f"Trying list join: {err} for {filename}")
                # self.em.rclog(f"VAL NOW <{val}>")
                val = tobytes(val)
                if ntag in minf:
                    # Note that it would be nicer to use proper CSV quoting
                    minf[ntag] += b" | " + val
                else:
                    minf[ntag] = val
                # self.em.rclog(f"Tag <{ntag}> -> <{val}>")
            except Exception as err:
                self.em.rclog(f"tag: {tag} {minf[ntag]}: {err}. fn {filename}")

        self._fixrating(minf)
        if "orchestra" in minf:
            val = minf["orchestra"]
            if val.startswith(b"orchestra="):
                minf["orchestra"] = val[10:]
        # self.em.rclog(f"minf after tags {minf}")

    def transcribe_via_whisper(self, filename: str):
        """Manage whisper stt access via a lock file.

        This is necessary to keep from exhausting memory and from running longer then needed.
        The speech to text engine is fully concurrent and asynchronous, so the indexing
        behavior of starting multiple indexing processes at once works against us here.

        We will queue up and wait until the resource is available.
        """
        # A valid whisper openai model file, such as "small.en" or "small"
        sttdataset = self.config.getConfParam("sttmodel")
        device_name = self.config.getConfParam("sttdevice")
        lock_file_name = os.path.join(self.recoll_confdir, "stt.lock")
        raw_result = {"segments": {}}
        # os.environ["PYTORCH_CUDA_ALLOC_CONF"] = "max_split_size_mb:1024"
        # os.environ["CUDA_LAUNCH_BLOCKING"] = "1"
        try:
            from filelock import FileLock
        except ImportError:
            print("RECFILTERROR HELPERNOTFOUND python3:filelock")
            sys.exit(1)
        try:
            with FileLock(lock_file_name):
                # self.em.rclog(f"Acquired stt file lock: {lock_file_name} to process {filename}.")
                try:
                    import whisper
                    import torch
                except ImportError:
                    print("RECFILTERROR HELPERNOTFOUND python3:openai-whisper")
                    sys.exit(1)

                if sttdataset not in whisper.available_models():
                    self.em.rclog(
                        f"Invalid stt model specified, skipping speech transcription "
                        "for {filename}."
                    )
                else:
                    if device_name:
                        stt_model = whisper.load_model(
                            name=sttdataset, device=device_name
                        )
                    else:
                        stt_model = whisper.load_model(name=sttdataset)
                    try:
                        torch.cuda.empty_cache()
                        raw_result = stt_model.transcribe(filename)
                        del stt_model
                    except Exception as ex:
                        self.em.rclog(
                            f"Whisper speech to text transcription error: {ex}, "
                            "skipping transcription of {filename}."
                        )
                    finally:
                        torch.cuda.empty_cache()
                        gc.collect()
                        # self.em.rclog(torch.cuda.memory_summary())
            # self.em.rclog(f"Released stt file lock for: {filename}.")
        except Exception as ex:
            self.em.rclog(
                f"Whisper speech to text lock error: {ex}, skipping transcription "
                "of {filename}."
            )
        return raw_result

    def speech_to_text(self, filename: str):
        """Output html content containing the speech to text content found in the audio track.

        This reuses recolls page numbering mechanism, wherein a slash f designates a new page,
        i.e. a form feed.  In the case of an audio track, we are using a page to represent a second
        in time.

        We fill the output with empty page form feeds unless we have text that starts during that
        second/page, then we output the content and append the form feed to it.

        When the audio/video player is called from opened from recoll using a snippet of indexed
        speech to text, it should open to the correct second where the snippet was played. Not all
        audio players and formats do this accurately.

        """
        output_array = []
        result_dict = {}

        import rclocrcache
        import json

        # The cache can find data either based on file metadata, or, in case, e.g. the file has been
        # renamed, based on a data hash. We limit the hash size to 3mb (which will be taken as 3 1mb
        # slices at the beginning, middle and end of the file
        cache = rclocrcache.OCRCache(self.config, hashmb=3)
        incache, json_result = cache.get(filename)
        if incache:
            raw_result = json.loads(json_result.decode("UTF-8"))
        else:
            raw_result = self.transcribe_via_whisper(filename)
            cache.store(filename, json.dumps(raw_result).encode("UTF-8"))

        for segment in raw_result["segments"]:
            if "start" in segment:
                segment_start = int(segment["start"])
                if segment_start in result_dict:
                    result_dict[segment_start] += " " + segment["text"]
                else:
                    result_dict[segment_start] = segment["text"]
        max_seconds = 0
        if result_dict:
            max_seconds = max(result_dict.keys()) + 1
        for i in range(max_seconds):
            if i in result_dict:
                output_array.append(result_dict.get(i))
            else:
                output_array.append("")
        if self.preview_mode == "yes":
            return "\n".join([e for e in output_array if e])
        else:
            return "\f\n".join(output_array)

    def html_text(self, filename):
        # self.em.rclog(f"processing {filename}")
        if not self.inputmimetype:
            raise Exception("html_text: input MIME type not set")
        self.config.setKeyDir(os.path.dirname(filename))
        if self.config.getConfParam("speechtotext") == "whisper":
            self.process_stt = True
        else:
            self.process_stt = False

        # The field storage dictionary
        minf = {}

        # We open the file here in order to specify the buffering parameter. For some reason it
        # makes a huge difference in performance (e.g. 6x) at least in some cases with an
        # NFS-mounted volume. mutagen itself does not specify a buffering parameter (as of
        # 1.45.1). No idea if this is a Python, or Linux kernel (or ?) issue.
        with open(filename, "rb", buffering=4096) as fileobj:
            strex = ""
            mutf = None
            try:
                mutf = File(fileobj)
            except Exception as ex:
                strex = str(ex)
                try:
                    # Note: this would work only in the off chance that the file format is not
                    # recognized, but the file does begin with an ID3 tag.
                    fileobj.seek(0)
                    mutf = ID3(fileobj)
                except Exception as ex:
                    strex += "  " + str(ex)
            if mutf is None:
                # Note: mutagen will fail the open (and raise) for a valid file with no tags. Maybe
                # we should just return an empty text in this case? We seem to get an empty str(ex)
                # in this case, and a non empty one for, e.g. permission denied, but I am not sure
                # that the emptiness will be consistent for all file types. The point of detecting
                # this would be to avoid error messages and useless retries.
                if not strex:
                    return b""
                else:
                    raise Exception(f"Open failed: {strex}")
            # self._showMutaInfo(mutf)

            self._extractaudioparams(filename, minf, mutf)
            self._processtags(mutf, minf, filename)

            # Check for embedded image. We just set a flag.
            embdimg = self._embeddedImageFormat(mutf)
            if embdimg:
                # self.em.rclog("Embedded image format: %s" % embdimg)
                minf["embdimg"] = tobytes(embdimg)

        self.em.setfield("charset", b"utf-8")
        if self.tagfix:
            self.tagfix(minf)

        if "date" in minf:
            uxtime = self.parsedate(minf["date"])
            if uxtime:
                minf["dmtime"] = uxtime

        for tag, val in minf.items():
            # self.em.rclog("%s -> %s" % (tag, val))
            self.em.setfield(tag, val)
            # Compat with old version
            if tag == "artist":
                self.em.setfield("author", val)
        html_output = _htmlprefix
        #################
        # Document text: use the mutagen pprint function. The values may be somewhat
        # different from what is in the metadata above because of the corrections we apply or
        # different format choices.
        try:
            docdata = tobytes(mutf.pprint())
        except Exception as err:
            docdata = ""
            self.em.rclog(f"Doc pprint error: {err}")

        stt_results = b""
        if self.process_stt and "LYRICS" not in mutf:
            stt_results = self.speech_to_text(filename.decode("utf-8")).encode("utf-8")
        html_output += docdata
        html_output += "\n".encode("utf-8") + stt_results
        html_output += _htmlsuffix
        # self.em.rclog(f"Results: {html_output}")
        return html_output


if __name__ == "__main__":
    proto = rclexecm.RclExecM()
    extract = AudioTagExtractor(proto)
    rclexecm.main(proto, extract)
