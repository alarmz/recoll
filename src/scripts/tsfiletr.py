#!/usr/bin/python3
"""
Input a Qt message file (.ts) and send all empty "unfinished" translations to an LLM (openai atm)
for translation.

Most of the code is for preserving the format of a standard .ts for minimum editing.
It would probably be much more efficient to send multiple messages at a time to avoid the
repeated prompt. Maybe, with a proper prompt, the llm could do the whole file actually...

It costs a few cents per file for a big update so it's not worth optimizing.
"""

import sys
import xml.sax
import json
import os
import getopt

from openai import OpenAI

g_nollmcall = True

_progname = "program"

def msg(s, file=sys.stderr):
    print(f"{s}", file=file)

def Usage(err="", file=sys.stderr):
    if err:
        msg(err, file=file)
    msg(f"Usage: {_progname} [-h | --help] [-r | --real] <fn>", file=file)
    sys.exit(1)


openai_query_params = {
    "model": "gpt-3.5-turbo",
    "temperature": 0,
    "max_tokens": 1024
}
openai_client = None

def ask_openai(prompt: str, openai_query_params=openai_query_params) -> str:
    global openai_client
    if openai_client is None:
        openai_client = OpenAI(
            api_key=os.environ["OPENAI_API_KEY"],  # default actually.
        )
    response = openai_client.chat.completions.create(
        messages=[
            {
                "role": "user",
                "content": prompt,
            }
        ],
        **openai_query_params,
    )
    msg = response.choices[0].message.content
    return msg

base_prompt = "Translate the following text fragment from English to {lang}, in the context of a " \
    "text search GUI. Text fragment: "

def translate_msg(msg, lang):
    prompt = base_prompt.format(lang=lang)
    result = ask_openai(prompt + msg)
    return result

def xmlescape(t):
    return xml.sax.saxutils.escape(t, entities={"\"" : "&quot;", "'" : "&apos;"})

language_codes = {
    "ar": "Arabic",
    "cs": "Czech",
    "da": "Danish",
    "de": "German",
    "el": "Greek",
    "en": "English",
    "es": "Spanish", "es-ES": "Spanish",
    "fi": "Finnish",
    "fr": "French",
    "he": "Hebrew",
    "hi": "Hindi",
    "hu": "Hungarian",
    "id": "Indonesian",
    "it": "Italian",
    "ja": "Japanese", "ja_JP": "Japanese",
    "ko": "Korean",
    "lt": "Lithuanian",
    "nl": "Dutch",
    "no": "Norwegian",
    "pl": "Polish",
    "pt": "Portuguese",
    "ru": "Russian", "ru_RU": "Russian",
    "sv": "Swedish", "sv_SE": "Swedish",
    "th": "Thai",
    "tr": "Turkish",
    "uk": "Ukrainian",
    "zh": "Chinese", "zh_CN": "Chinese",
    "vi": "Vietnamese"
}

class handler(xml.sax.handler.ContentHandler):
    def __init__(self):
        super().__init__()
        self.txt = ""
        self.unfinished = False
        self.source = ""
        self.depth = 0
        self.indent = ""
        self.txts = ["source", "translation", "name"]
        print("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE TS>")

    def startElement(self, name, attrs):
        if name == "TS":
            self.language = attrs["language"]
            self.langfull = language_codes[self.language]
            #self.sourcelanguage = attrs["sourcelanguage"]
        if name == "translation" and "type" in attrs and attrs["type"] == "unfinished":
            self.unfinished = True
        else:
            self.unfinished = False
        out = f"<{name} "
        for k,v in attrs.items():
            out += f"{k}={xml.sax.saxutils.quoteattr(v)} "
        out = out.rstrip() + ">"
        if not name in self.txts:
            out += "\n"
        print(self.indent + out, end="")
        if name != "TS":
            self.depth += 1
            self.indent = "    " * self.depth

    def endElement(self, name):
        t = self.txt.lstrip()
        if t:
            print(xmlescape(t), end="")
        if name == "source":
            self.source = t
        elif name == "translation" and self.unfinished and len(t.strip()) == 0:
            if g_nollmcall:
                print(f"TRANSLATION({self.source}, {self.langfull})", end="")
            else:
                print(f"{xmlescape(translate_msg(self.source, self.langfull))}", end="")
        self.txt = ""
        if name != "TS":
            self.depth -= 1
            self.indent = "    " * self.depth
        if not name in self.txts:
            print(self.indent, end="")
        print(f"</{name}>")
            
    def characters(self, content):
        self.txt += content


if __name__ == "__main__":
    _progname = os.path.basename(sys.argv[0])
    try:
        options, args = getopt.getopt(sys.argv[1:], "hr", longopts=["help", "real"])
    except getopt.GetoptError as err:
        Usage(err)
    for opt,val in options:
        if opt in ("-r", "--real"):
            g_nollmcall = False
        elif opt in ("-h", "--help"):
            Usage(file=sys.stdout)
        else:
            Usage()

    if len(args) != 1:
        Usage()
    h = handler()
    xml.sax.parse(args[0], h)
