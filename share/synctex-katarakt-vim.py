#!/usr/bin/python

# Dependencies:
#
#   python-gobject2
#   python-dbus
#

# Thorsten Wißmann, 2015
# in case of bugs, contact:    edu _at_ thorsten-wissmann _dot_ de

def print_help():
    print("""Usage: {0} SESSIONNAME [PDFFILE]

  A katarakt wrapper for synctex communication between vim and katarakt.

  SESSIONNAME is the name of your vim session, i.e. you have to start a vim
  session *yourself* and *beforehand* with

      vim --servername SESSIONNAME

Behaviour:

  This script starts katarakt for PDFFILE. If no PDFFILE is specified, it tries
  to autodetect the correct pdf file (by finding a corresponding .synctex.gz
  file in the current directory).

  It registers a keybinding in vim (per default "ZE"), to jump to the
  corresponding page in katarakt.

  If Katarakt emits the edit signal (usually on Ctrl+LeftMouse), then it
  opens the corresponding tex file in vim and jumps to the corresponding
  line in the texfile.

  If the user presses ZE in the editor, a message is sent to this script.
  This script calls synctex and sends the pdf-coordinates to katarakt.

  When katarakt quits, then this script exits as well.

Requirements:

  You need to compile the latex document using the option -synctex=1 in order to
  obtain the required .synctex.gz file.""".format(sys.argv[0]))



import gobject
import sys
import dbus
import dbus.service
import subprocess
import time # only for sleep()
import threading
import os
import re # for regular expressions
from dbus.mainloop.glib import DBusGMainLoop


# tell dbus that we use the gobject main loop

DBusGMainLoop(set_as_default=True)
loop = gobject.MainLoop()


#########################################
#               Settings                #
#########################################


def detect_synctexfile():
    # try to find the synctex.gz file in the current directory
    a = re.compile(".*\.synctex\.gz$")
    files = [f for f in os.listdir('.') if os.path.isfile(f) and a.match(f)]
    return files

def die(msg):
    print(msg)
    exit(1)

try:
    session_name = sys.argv[1]
except IndexError:
    print_help()
    exit(0)

if session_name == "-h" or session_name == "--help":
    print_help()
    exit(0)

try:
    pdf_filename = sys.argv[2]
except IndexError:
    try:
        synctex_filename = detect_synctexfile()[0]
        pdf_filename = re.sub("\.synctex\.gz$", ".pdf", synctex_filename)
        print("auto-detected {0}".format(pdf_filename))
    except IndexError:
        die("no *.synctex.gz file found in current directory")

vim_session = session_name
vim_view_keybind = "ZE"

pdfprocess = subprocess.Popen(['katarakt', pdf_filename])
pdf_pid = pdfprocess.pid

view_command = ("qdbus katarakt.pid%d" % pdf_pid +
                " / katarakt.SourceCorrelate.view" +
                " %{output} %{page} %{x} %{y}")

# connect to dbus
bus = dbus.SessionBus()

# wait for katarakt to show up
# it doesn't seem to be trivial to wait for an
# application to show up at dbus. hence the (still racy) hack

katarakt_booted = False
while pdfprocess.pid != None and not katarakt_booted:
    try:
        katarakt = bus.get_object('katarakt.pid%d' % pdf_pid, '/')
        iface = dbus.Interface(katarakt, 'katarakt.SourceCorrelate')
        katarakt_booted = True
    except dbus.exceptions.DBusException:
        time.sleep(0.01)

# register an own service on the session bus
busName = dbus.service.BusName('katarakt.synctex.vim.' + session_name,
                               bus = dbus.SessionBus())

class BridgeObject(dbus.service.Object):
    def __init__(self, object_path):
       dbus.service.Object.__init__(self, busName,
                                        object_path)

    @dbus.service.method(dbus_interface='katarakt.bridge',
                         in_signature='sii', out_signature='')

    def View(self, filename, line, col):
        subprocess.call([
            "synctex", "view",
            "-i", "%d:%d:%s" % (line,col,filename),
            "-o", pdf_filename,
            "-x", view_command,
        ])

# create the dbus object and bind it to the bus
BridgeObject('/')

# inject the keybinding for viewing into the vim session
returncode = subprocess.call([
    "vim", "--servername", vim_session,
    "--remote-send",
    ("<ESC>:map %s" % vim_view_keybind
    + " :exec \"execute ('!qdbus katarakt.synctex.vim.%s" % session_name
    + " / katarakt.bridge.View % ' . line('.') . ' 0')\"<lt>CR><lt>CR><CR><CR>")
])

if returncode != 0:
    print("Error when trying to register keybinding in the vim session \"{0}\"".format(vim_session))
    exit(1)



# callback if the signal for edit is sent by katarakt
def on_edit(filename,page,x,y):
    #print ("go to page %d at %d,%d" % (page,x,y))
    subprocess.call([
        "synctex", "edit",
        "-o", ("%d:%d:%d:%s" % (1+page,x,y,filename)),
        "-x", "vim --servername '" + vim_session + "' --remote-silent '+%{line}' %{input}",
        ])

iface.connect_to_signal("edit", on_edit)


# Main loop and cleanup:
def quit_if_pdf_exits():
    pdfprocess.wait()
    loop.quit()

thread = threading.Thread(target=quit_if_pdf_exits, args=())
thread.start()

# enable threads again, otherwise the quit_if_pdf_exits-thread will not be
# executed
gobject.threads_init()
loop.run()

