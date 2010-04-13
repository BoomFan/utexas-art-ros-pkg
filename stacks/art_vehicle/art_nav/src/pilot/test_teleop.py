#!/usr/bin/python
#
# Qt python script to send tele-operation commands for pilot tests
#
#   Copyright (C) 2009 Austin Robot Technology
#
#   License: Modified BSD Software License Agreement
#
#   Author: Jack O'Quin
#
# $Id$

PKG_NAME = 'art_nav'

import sys
import os
import signal
import threading
from PyQt4 import QtGui
from PyQt4 import QtCore

import roslib;
roslib.load_manifest(PKG_NAME)
import rospy
from art_nav.msg import CarCommand

carcmd = rospy.Publisher('pilot/cmd', CarCommand)
rospy.init_node('teleop')

# set path name for resolving icons
icons_path = os.path.join(roslib.packages.get_pkg_dir(PKG_NAME), "icons")

def find_icon(dir, basename, extlist=['.svg', '.png']):
    """Find icon file with basename in dir.

    extlist: a list of possible extensions
    returns: full path name if file found, None otherwise
"""
    for ext in extlist:
        pathname = os.path.join(dir, basename + ext)
        if os.path.exists(pathname):
            return pathname
    return None

def pkg_icon(name):
    """Resolve package-relative icon name to path name.
    """
    pname = find_icon(icons_path, name)
    if pname == None:
        raise NameError, 'icon file not found'
    return pname


class MainWindow(QtGui.QMainWindow):
    "Create vehicle tele-operation control window."

    def __init__(self, carcmd):
        QtGui.QMainWindow.__init__(self)

        self.carcmd = carcmd

        self.resize(350, 250)
        self.setIconSize(QtCore.QSize(32,32))
        self.setWindowTitle('Vehicle Tele-Operation')

        exit = QtGui.QAction(QtGui.QIcon(pkg_icon('exit')), 'Exit', self)
        exit.setShortcut('Ctrl+Q')
        self.connect(exit, QtCore.SIGNAL('triggered()'), QtCore.SLOT('close()'))

        center_wheel = QtGui.QAction(QtGui.QIcon(pkg_icon('go-first')),
                            'center wheel', self)
        center_wheel.setShortcut(QtCore.Qt.Key_Home)
        self.connect(center_wheel, QtCore.SIGNAL('triggered()'),
                     self.center_wheel)

        go_left = QtGui.QAction(QtGui.QIcon(pkg_icon('go-previous')),
                            'steer left', self)
        go_left.setShortcut(QtCore.Qt.Key_Left)
        self.connect(go_left, QtCore.SIGNAL('triggered()'), self.go_left)

        slow_down = QtGui.QAction(QtGui.QIcon(pkg_icon('go-down')),
                            'slow down', self)
        slow_down.setShortcut(QtCore.Qt.Key_Down)
        self.connect(slow_down, QtCore.SIGNAL('triggered()'), self.slow_down)

        speed_up = QtGui.QAction(QtGui.QIcon(pkg_icon('go-up')),
                              'speed up', self)
        speed_up.setShortcut(QtCore.Qt.Key_Up)
        self.connect(speed_up, QtCore.SIGNAL('triggered()'), self.speed_up)

        go_right = QtGui.QAction(QtGui.QIcon(pkg_icon('go-next')),
                              'steer right', self)
        go_right.setShortcut(QtCore.Qt.Key_Right)
        self.connect(go_right, QtCore.SIGNAL('triggered()'), self.go_right)

        stop_car = QtGui.QAction(QtGui.QIcon(pkg_icon('go-bottom')),
                                 'stop car', self)
        stop_car.setShortcut(QtCore.Qt.Key_End)
        self.connect(stop_car, QtCore.SIGNAL('triggered()'), self.stop_car)


        menubar = self.menuBar()
        file = menubar.addMenu('&File')
        file.addAction(exit)
        file.addAction(center_wheel)
        file.addAction(go_left)
        file.addAction(slow_down)
        file.addAction(stop_car)
        file.addAction(speed_up)
        file.addAction(go_right)

        toolbar = self.addToolBar('Controls')
        toolbar.addAction(exit)
        toolbar.addAction(center_wheel)
        toolbar.addAction(go_left)
        toolbar.addAction(slow_down)
        toolbar.addAction(stop_car)
        toolbar.addAction(speed_up)
        toolbar.addAction(go_right)

        self.car_msg = CarCommand()
        self.car_msg.header.stamp = rospy.Time.now()
        self.car_msg.velocity = 0.0
        self.car_msg.angle = 0.0
        self.carcmd.publish(self.car_msg)

        self.updateStatusBar()

    def updateStatusBar(self):
        self.statusBar().showMessage('speed: '
                                     + str(self.car_msg.velocity)
                                     + ' m/s,    angle: '
                                     + str(self.car_msg.angle)
                                     + ' deg')

    def adjustCarCmd(self, v, a):
        self.car_msg.header.stamp = rospy.Time.now()
        self.car_msg.velocity += v
        self.car_msg.angle += a
        self.carcmd.publish(self.car_msg)
        self.updateStatusBar()

    def center_wheel(self):
        "center steering wheel"
        self.adjustCarCmd(0.0, -self.car_msg.angle)

    def go_left(self):
        "steer one degree left"
        self.adjustCarCmd(0.0, 0.25)

    def go_right(self):
        "steer one degree right"
        self.adjustCarCmd(0.0, -0.25)    # one degree right

    def slow_down(self):
        "go one m/s slower"
        self.adjustCarCmd(-1.0, 0.0)    # one m/s slower

    def speed_up(self):
        "go one m/s faster"
        self.adjustCarCmd(1.0, 0.0)     # one m/s faster

    def stop_car(self):
        "stop car immediately"
        self.adjustCarCmd(-self.car_msg.velocity, 0.0)


class QtThread(threading.Thread):

    def __init__(self, carcmd):
        self.carcmd = carcmd
        threading.Thread.__init__(self)

    def run(self):
        # run the program
        app = QtGui.QApplication(sys.argv)
        
        teleop = MainWindow(self.carcmd)
        teleop.show()

        # run Qt main loop until window terminates
        exit_status = app.exec_()

        rospy.signal_shutdown('Qt exit')

        sys.exit(exit_status)


if __name__ == '__main__':

    #rospy.loginfo('starting tele-operation')

    try:
        QtThread(carcmd).start()
        rospy.spin()
    except rospy.ROSInterruptException: pass

    #rospy.loginfo('tele-operation completed')