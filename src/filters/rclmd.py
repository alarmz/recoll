#!/usr/bin/env python3
# Copyright (C) 2024 Alberti, Andrea
#
# License: GPL 2.1
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
#
# Script name: rclmd.py
# Author: Andrea Alberti

import os
import re
import sys
import rclexecm  # For communicating with recoll
from rclbasehandler import RclBaseHandler
from datetime import datetime

try:
    import yaml
except:
    print("RECFILTERROR HELPERNOTFOUND python3:PyYAML")
    sys.exit(1)

####
_htmlprefix = b"""<html><head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
"""
####
_htmlmidfix = b"""</head><body><pre>
"""
####
_htmlsuffix = b"""
</pre></body></html>"""
####


def get_file_times(file_path):
    # Get modification time (mtime)
    modification_time = os.path.getmtime(file_path)

    # Get change time (ctime)
    if sys.platform == "darwin":  # macOS
        # On macOS, use st_ctime for change time
        stat_info = os.stat(file_path)
        change_time = stat_info.st_ctime

        # Get birth (creation) time
        if hasattr(stat_info, "st_birthtime"):
            birth_time = stat_info.st_birthtime
        else:
            birth_time = os.path.getctime(file_path)
    elif sys.platform == "win32":  # Windows
        # On Windows, os.path.getctime() gives creation time (used as birth time)
        birth_time = os.path.getctime(file_path)
        change_time = birth_time  # Windows doesn't have a change time like Linux/macOS
    else:  # Linux or other Unix systems
        # On Linux, os.path.getctime() typically gives the last metadata change time (ctime)
        change_time = os.path.getctime(file_path)

        # Linux often doesn't track creation time, so we fallback to ctime
        birth_time = os.path.getctime(file_path)

    # Round the Unix timestamps to the nearest second and convert to string
    modification_time_unix = str(round(modification_time))
    change_time_unix = str(round(change_time))
    birth_time_unix = str(round(birth_time))

    return modification_time_unix, change_time_unix, birth_time_unix


class MDhandler(RclBaseHandler):
    def __init__(self, em):
        super(MDhandler, self).__init__(em)

        self.RCLMD_CREATED = os.environ.get("RCLMD_CREATED", None)
        self.RCLMD_MODIFIED = os.environ.get("RCLMD_CREATED", None)
        self.RCLMD_DATEFORMAT = os.environ.get("RCLMD_DATEFORMAT", None)

    def parse_md_file(self, text_content):
        """Extract YAML front matter and return it along with the body content."""
        frontmatter = {}
        body_content = text_content

        if text_content.startswith("---"):
            end = text_content.find("---", 3)
            if end != -1:
                yaml_content = text_content[3:end].strip()
                frontmatter = yaml.safe_load(yaml_content)
                body_content = text_content[
                    end + 3 :
                ].strip()  # Get the content after the front matter

        return frontmatter, body_content

    def extract_tags(self, frontmatter):
        """Extract tags from the YAML front matter."""
        stripped_tags = []
        if "tags" in frontmatter:
            tags = frontmatter["tags"]
            if tags is not None:
                stripped_tags = [
                    tag.lstrip("#") for tag in frontmatter["tags"]
                ]  # Remove leading '#' if present
        return stripped_tags

    def html_text(self, filename):
        basename = os.path.splitext(os.path.basename(filename))[0].decode("utf-8")

        # Regular expression to match a date followed by a space and then the rest of the title
        pattern = r"^\d{4}-\d{2}-\d{2}\s+(.*)"

        # Search for the pattern in the filename
        match = re.search(pattern, basename)

        # Extract the title if the pattern matches
        if match:
            title = match.group(1)
        else:
            title = basename

        # self.em.log(f"Filename: {filename.decode('utf-8')}")
        # Read the file content
        with open(filename, "rb") as file:
            content = file.read()

        # Start generating the HTML output
        output = _htmlprefix

        output += (
            b'<meta name="title" content="'
            + rclexecm.htmlescape(rclexecm.makebytes(title))
            + b'">\n'
        )

        # self.em.log(f"Content:\n{content}")
        # Convert content to string (assuming UTF-8 encoding)
        text_content = content.decode("utf-8")

        # Parse the front matter and get the body content
        frontmatter, body_content = self.parse_md_file(text_content)

        # We start with empty variables and try to fill them in using data from the frontmatter
        created = None
        modified = None
        # Process frontmatter
        if frontmatter is not {}:
            # If the datetime format is provided
            if self.RCLMD_DATEFORMAT:
                if self.RCLMD_CREATED and "created" in frontmatter:
                    date_format = "%Y-%m-%d, %H:%M:%S"
                    date_obj = datetime.strptime(
                        frontmatter[self.RCLMD_CREATED], date_format
                    )
                    created = str(round(date_obj.timestamp()))

                if self.RCLMD_MODIFIED and "modified" in frontmatter:
                    date_format = "%Y-%m-%d, %H:%M:%S"
                    date_obj = datetime.strptime(
                        frontmatter[self.RCLMD_MODIFIED], date_format
                    )
                    created = str(round(date_obj.timestamp()))

            # Extract tags from the front matter
            tags = self.extract_tags(frontmatter)

            # Log the extracted tags (optional for debugging)
            # self.em.log(f"Extracted tags: {tags}")

            # Add meta tags for each extracted tag (using MDT as the prefix)
            # self.em.log(f"TAGS: {tags}")
            for tag in tags:
                output += (
                    b'<meta name="tags" content="'
                    + rclexecm.htmlescape(rclexecm.makebytes(tag))
                    + b'">\n'
                )

        # Fallback case when the creation and modification time could not be determined from the frontmatter
        if created is None or modified is None:
            modified_from_file, changed_from_file, created_from_file = get_file_times(
                filename
            )
            if created is None:
                created = created_from_file
            if modified is None:
                modified = modified_from_file

        # self.em.log("Field 'created' set to: " + created)
        # self.em.log("Field 'modified' set to: " + modified)

        self.em.setfield("created", created)
        self.em.setfield("modified", modified)

        output += _htmlmidfix
        output += rclexecm.htmlescape(body_content.encode("utf-8"))

        output += _htmlsuffix

        self.outputmimetype = "text/html"
        # self.em.log(f"{output.decode('utf-8')}")

        return output


if __name__ == "__main__":
    proto = rclexecm.RclExecM()
    extract = MDhandler(proto)
    rclexecm.main(proto, extract)
