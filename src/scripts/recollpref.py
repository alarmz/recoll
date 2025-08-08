#!/usr/bin/python3
#
# Help adding a preference to the Recoll GUI.
# - Edit the guiutils.[cpp|h] files to read/store/write the pref
# - Edit the uiprefs_w.cpp file to add callbacks for reading and setting the value from the prefs
#   GUI
# - Print a uiprefs.ui code fragment. You will need to edit/insert it at an appropriate place in the
#   file

import os
import sys
import tempfile
import shutil

def trace(s):
    print("%s" % s, file=sys.stderr)

def fatal(s):
    trace(s)
    sys.exit(1)

def Usage():
    trace('Usage: recollpref.py varname section type')
    trace(' section: ui preview reslist restable shortcuts search extradbs misc')
    fatal(' type: checkbox combobox lineedit spinbox')


RCLSRC = os.path.dirname(sys.argv[0])
RCLSRC = os.path.join(RCLSRC, "..")
RCLSRC = os.path.join(RCLSRC, "qtgui")
print(f"RCLSRC {RCLSRC}")                      

guiutils_h = os.path.join(RCLSRC, "guiutils.h")
guiutils_cpp = os.path.join(RCLSRC, "guiutils.cpp")
uiprefs_w_cpp= os.path.join(RCLSRC, "uiprefs_w.cpp")

if not os.access(guiutils_h, os.W_OK):
    fatal(f"{guiutils_h} not found or not writable")

def insertbeforemarker(fn, delim, data):
    fw = tempfile.NamedTemporaryFile(delete=False)
    tmpfn = fw.name
    fr = open(fn, "r")
    for line in fr:
        if line.strip() == delim:
            fw.write(data.encode('utf-8'))
        fw.write(line.encode('utf-8'))
    fw.close()
    shutil.move(tmpfn, fn)


if len(sys.argv) != 4:
    Usage()

g_varname = sys.argv[1]
g_section = sys.argv[2]
g_type = sys.argv[3]

if  not g_type in ["checkbox", "combobox", "lineedit", "spinbox"]:
    Usage()
if not g_section in ["ui","preview","reslist","restable","shortcuts","search","extradbs","misc"]:
    Usage()

trace("============= uiprefs.ui ====================")

if g_type == "checkbox":
    print(
'''       <item>
        <widget class="QCheckBox" name="%sCB">
         <property name="toolTip">
          <string>TOOLTIP?</string>
         </property>
         <property name="text">
          <string>PLEASE ADD SOME DESCRIPTION.</string>
         </property>
         <property name="checked">
          <bool>false</bool>
         </property>
        </widget>
       </item>''' % g_varname)
elif g_type == "combobox":
    print(
'''       <item>
        <layout class="QHBoxLayout">
         <item>
          <widget class="QLabel" name="%sLBL">
           <property name="text">
            <string>DESCRIPTIVE TEXT?</string>
           </property>
           <property name="wordWrap">
            <bool>false</bool>
           </property>
          </widget>
         </item>
         <item>
          <widget class="QComboBox" name="%sCMB">
           <property name="minimumSize">
            <size>
             <width>10</width>
             <height>0</height>
            </size>
           </property>
           <property name="toolTip">
            <string>TOOLTIP ?</string>
           </property>
           <property name="editable">
            <bool>no</bool>
           </property>
           <property name="insertPolicy">
            <enum>QComboBox::NoInsert</enum>
           </property>
          </widget>
         </item>
        </layout>
       </item>''' % (g_varname, g_varname))
elif g_type == "lineedit":
    print(
'''         <item>
          <layout class="QHBoxLayout">
           <item>
            <widget class="QLabel" name="%sLBL">
             <property name="text">
              <string>DESCRIPTION?</string>
             </property>
             <property name="wordWrap">
              <bool>false</bool>
             </property>
            </widget>
           </item>
           <item>
            <widget class="QLineEdit" name="%sLE">
             <property name="minimumSize">
              <size>
               <width>30</width>
               <height>0</height>
              </size>
             </property>
            </widget>
           </item>
           <item>
            <spacer name="%sHSPC">
             <property name="orientation">
              <enum>Qt::Horizontal</enum>
             </property>
             <property name="sizeHint" stdset="0">
              <size>
               <width>40</width>
               <height>20</height>
              </size>
             </property>
            </spacer>
           </item>
          </layout>
         </item>''' % (g_varname, g_varname, g_varname))
elif g_type == "spinbox":
    print(
'''       <item>
        <layout class="QHBoxLayout" stretch="0,0,1">
         <item>
          <widget class="QLabel" name="%sLBL">
           <property name="sizePolicy">
            <sizepolicy hsizetype="Preferred" vsizetype="Preferred">
             <horstretch>1</horstretch>
             <verstretch>0</verstretch>
            </sizepolicy>
           </property>
           <property name="toolTip">
            <string>TOOLTIP?</string>
           </property>
           <property name="text">
            <string>DESCRIPTION?:</string>
           </property>
           <property name="wordWrap">
            <bool>false</bool>
           </property>
          </widget>
         </item>
         <item>
          <widget class="QSpinBox" name="%sSB">
           <property name="minimum">
            <number>-1</number>
           </property>
           <property name="value">
            <number>0</number>
           </property>
          </widget>
         </item>
         <item>
          <spacer name="%sHSPC">
           <property name="orientation">
            <enum>Qt::Horizontal</enum>
           </property>
           <property name="sizeHint" stdset="0">
            <size>
             <width>40</width>
             <height>20</height>
            </size>
           </property>
          </spacer>
         </item>
        </layout>
       </item>''' % (g_varname, g_varname, g_varname))
else:
    Usage()

trace("============= guiutils.h ====================")

if g_type == "checkbox":
        value = "    bool %s{false};\n" % g_varname
        sttngsTP = "Bool"
        sttngsDFLT = "false"
        widsuff = "CB"
elif g_type == "combobox":
        value = "    QString %s;\n" % g_varname
        sttngsTP = "String"
        sttngsDFLT = '""'
        widsuff = "CMB"
elif g_type == "lineedit":
        value="    std::string %s;\n" % g_varname
        sttngsTP = "String"
        sttngsDFLT = '""'
        widsuff = "LE"
elif g_type == "spinbox":
        value = "    int %s{0};\n" % g_varname
        sttngsTP = "Int"
        sttngsDFLT = "0"
        widsuff = "SB"
else:
 Usage()

insertbeforemarker(guiutils_h, "/*INSERTHERE*/", value)

trace("============= guiutils.cpp ====================")

value = '    SETTING_RW(prefs.%s, "/Recoll/%s/%s", %s, %s)\n' % \
    (g_varname, g_section, g_varname, sttngsTP, sttngsDFLT)
insertbeforemarker(guiutils_cpp, "/*INSERTHERE*/", value)

trace("============= uiprefs_w.cpp setFromPrefs ====================")

if g_type == "checkbox":
    value = "    %s%s->setChecked(prefs.%s);\n" % (g_varname, widsuff, g_varname)
elif g_type == "combobox":
    value = "    %s%s->setCurrentText(u8s2qs(prefs.%s));\n" % (g_varname, widsuff, g_varname)
elif g_type == "lineedit":
    value = "    %s%s->setText(u8s2qs(prefs.%s));\n" % (g_varname, widsuff, g_varname)
elif g_type == "spinbox":
    value = "    %s%s->setValue(prefs.%s);\n" % (g_varname, widsuff, g_varname)
else:
    Usage()
insertbeforemarker(uiprefs_w_cpp, "/*INSERTHERE_LOAD*/", value)

trace("============= uiprefs_w.cpp accept ====================")
if g_type == "checkbox":
    value="    prefs.%s = %s%s->isChecked();\n" % (g_varname, g_varname, widsuff)
elif g_type == "combobox":
    value="    prefs.%s = %s%s->currentText();\n" % (g_varname, g_varname, widsuff)
elif g_type == "lineedit":
    value="    prefs.%s = %s%s->text();\n" % (g_varname, g_varname, widsuff)
elif g_type == "spinbox":
    value="    prefs.%s = %s%s->value();\n" % (g_varname, g_varname, widsuff)
else:
    Usage()

insertbeforemarker(uiprefs_w_cpp, "/*INSERTHERE_ACCEPT*/", value)
