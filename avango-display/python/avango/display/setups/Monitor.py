# -*- Mode:Python -*-

##########################################################################
#                                                                        #
# This file is part of AVANGO.                                           #
#                                                                        #
# Copyright 1997 - 2010 Fraunhofer-Gesellschaft zur Foerderung der       #
# angewandten Forschung (FhG), Munich, Germany.                          #
#                                                                        #
# AVANGO is free software: you can redistribute it and/or modify         #
# it under the terms of the GNU Lesser General Public License as         #
# published by the Free Software Foundation, version 3.                  #
#                                                                        #
# AVANGO is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of         #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the           #
# GNU General Public License for more details.                           #
#                                                                        #
# You should have received a copy of the GNU Lesser General Public       #
# License along with AVANGO. If not, see <http://www.gnu.org/licenses/>. #
#                                                                        #
##########################################################################

import avango.script
import avango.osg
import avango.display
import avango.utils
import os


class Monitor(avango.display.Display):

    def __init__(self, inspector, options, generate_window=True, window_translation = avango.osg.make_trans_mat(0, 1.7, -0.6), user_translation = avango.osg.make_trans_mat(avango.osg.Vec3(0., 1.7, 0.))):
        super(Monitor, self).__init__("Monitor", inspector)

        self._subdisplay_window = {}
        self._subdisplay_window_events = {}
        self._subdisplay_keyboard = {}
        self._subdisplay_camera = {}
        self._subdisplay_view = {}

        self._screen_identifier = os.environ.get('DISPLAY', '')
        
        trackball_token = "trackball"
        self._enable_trackball=False
        if options.has_key(trackball_token) and self._bool_dict.has_key(options[trackball_token].lower()):
            self._enable_trackball=self._bool_dict[options[trackball_token].lower()]
            
        stereo_token = 'stereo'
        self._stereo_mode = False
        if options.has_key(stereo_token) and self._bool_dict.has_key(options[stereo_token].lower()):
            self._stereo_mode=self._bool_dict[options[stereo_token].lower()]
            
            #DEBUG
            print 'HELLO STEREO MODE ' + str(self._stereo_mode)
        
        if generate_window:
            window = self.make_window(0, 0,  1024, 768, 0.4, 0.3, self._stereo_mode, self._screen_identifier, 2)
            window.Name.value = "AVANGO"
            window.Decoration.value = True
            window.AutoHeight.value = True
            window.ShowCursor.value = True
            
            self.add_window(window, window_translation, 0)
            self._subdisplay_window[""]=window
        
        user = avango.display.nodes.User()
        user.Matrix.value = user_translation
        self.add_user(user)
    
    def make_view(self, subdisplay):
        print "Monitor._make_view"
        
        display_view = avango.display.nodes.MonitorView()
        if subdisplay == "":
            super(Monitor, self).make_view(subdisplay, display_view)
                
        # In the Monitor setting each subdisplay simply get a new window
        else:
            window = self.make_window(0, 0, 1024, 768, 0.4, 0.3, self._stereo_mode, self._screen_identifier, 2)
            window.Decoration.value = True
            window.AutoHeight.value = True
            window.ShowCursor.value = True
            window.Title.value = subdisplay
            window.Name.value = subdisplay
            
            window_translation = avango.osg.make_trans_mat(0, 1.7, -0.6)
            current_user = 0
        
            eye_offset = 0.0
            if window.StereoMode.value != avango.osg.viewer.stereo_mode.STEREO_MODE_NONE:
                eye_offset = 0.03

            camera, view = self.make_camera_with_viewport(
                display_view, eye_offset, window_translation, window)
            camera.EyeTransform.connect_from(self._users[current_user].Matrix)
            
            #self.connect_view_field(user_selector.ViewOut)
            self._subdisplay_window[subdisplay] = window
            
            #print "<" + str(subdisplay) + "> make_view"
            self.add_view(view)
            
            self.view_created(camera,view,subdisplay)
            
        #generate and connect keyboard
        self._subdisplay_keyboard[subdisplay] = avango.display.KeyboardDevice()
        self._subdisplay_keyboard[subdisplay].connect(self._subdisplay_window_events[subdisplay])
        
        #configure trackball
#        display_view.EnableTrackball.value = self._enable_trackball
        toggle_field = avango.utils.make_key_toggle_trigger_alternate(
                        avango.utils.make_bool2_and(self._subdisplay_keyboard[subdisplay].KeyShift,
                                                    self._subdisplay_keyboard[subdisplay].KeyF1),
                       self._enable_trackball)
        display_view.EnableTrackball.connect_from(toggle_field)
        
        time_sensor = avango.nodes.TimeSensor()
        self.keep_alive(time_sensor)
        trackball = avango.utils.nodes.Trackball()
        trackball.Enable.connect_from(display_view.EnableTrackball)
        toggle_field = avango.utils.make_key_toggle_trigger_alternate(
                        avango.utils.make_bool2_and(self._subdisplay_keyboard[subdisplay].KeyShift,
                                                    self._subdisplay_keyboard[subdisplay].KeyEnter),
                       True)
        trackball.CenterToBoundingSphere.connect_from(toggle_field)
        trackball.BoundingSphere.connect_from(display_view.BoundingSphereRoot.value.BoundingSphere)
        trackball.TimeIn.connect_from(time_sensor.Time)
        trackball.SpinningTimeThreshold.value = 0.5
        trackball.Direction.connect_from(self._subdisplay_window[subdisplay].MousePositionNorm)
        trackball.RotateTrigger.connect_from(self._subdisplay_window_events[subdisplay].MouseButtons_OnlyMiddle)
        trackball.PanTrigger.connect_from(self._subdisplay_window_events[subdisplay].MouseButtons_LeftAndMiddle)
        trackball.ZoomTrigger.connect_from(self._subdisplay_window_events[subdisplay].MouseButtons_OnlyRight)
        trackball.ResetTrigger.connect_from(self._subdisplay_keyboard[subdisplay].KeySpace)
        
        display_view.Camera.connect_from(trackball.Matrix)
        self.keep_alive(trackball)
        #TODO Enable the osg auto near/far plane computation
        display_view.Near.value = 0.1 
        display_view.Far.value = 10000
        
        #add some default actions
        #show window decoration (Ctrl+Enter)
        toggle_field = avango.utils.make_key_toggle_trigger_alternate(
                          avango.utils.make_bool2_and(self._subdisplay_keyboard[subdisplay].KeyCtrl,
                                                      self._subdisplay_keyboard[subdisplay].KeyEnter),
                          self._subdisplay_window[subdisplay].Decoration.value)
        display_view.WindowDecoration.connect_from(toggle_field)
        self._subdisplay_window[subdisplay].Decoration.connect_from(display_view.WindowDecoration)
        
        #toggle fullscreen (Alt+Enter)
        toggle_field = avango.utils.make_key_toggle_trigger( 
                            avango.utils.make_bool2_and(self._subdisplay_keyboard[subdisplay].KeyAlt,
                                                        self._subdisplay_keyboard[subdisplay].KeyEnter) )
        display_view.ToggleFullScreen.connect_from(toggle_field)
        self._subdisplay_window[subdisplay].ToggleFullScreen.connect_from(display_view.ToggleFullScreen)
        
        #optimize the scenegraph(Alt+O)
        node_optimizer = avango.utils.nodes.NodeOptimizer()
        node_optimizer.Node.connect_from(display_view.Root)
        toggle_field = avango.utils.make_key_released_trigger( 
                            avango.utils.make_bool2_and(self._subdisplay_keyboard[subdisplay].KeyAlt,
                                                        self._subdisplay_keyboard[subdisplay].KeyO) )
        node_optimizer.Trigger.connect_from(toggle_field)
        self.keep_alive(node_optimizer)
        
        return display_view
 
    
    def view_created(self, camera, view, subdisplay):
        'Primary window is automatically created by the constructor. All other windows are created by explicit calls of make_view(subdisplay)'
        # It seems that a view is only allowed to be associated with one EventField at a time.
        
        if self._subdisplay_window_events.has_key(subdisplay):
            return
        else:
            window_event = avango.osg.viewer.nodes.EventFields(View = view)
            self._subdisplay_window_events[subdisplay] = window_event
            self._subdisplay_window[subdisplay].DragEvent.connect_from(window_event.DragEvent)
            self._subdisplay_window[subdisplay].MoveEvent.connect_from(window_event.MoveEvent)
            self._subdisplay_camera[subdisplay] = camera
            self._subdisplay_view[subdisplay] = view
            
    def get_camera(self, subdisplay):
        if self._subdisplay_camera.has_key(subdisplay):
            return self._subdisplay_camera[subdisplay]
        return None
    
    def get_view(self, subdisplay):
        if self._subdisplay_view.has_key(subdisplay):
            return self._subdisplay_view[subdisplay]
        return None
    
    def get_window(self, subdisplay):
        if self._subdisplay_window.has_key(subdisplay):
            return self._subdisplay_window[subdisplay]
        return None

    def make_dominant_user_device(self, user, interface, subdisplay):
        if subdisplay not in self._subdisplay_camera:
            return avango.display.nodes.Device()

        if interface == "Keyboard":
            return self._subdisplay_keyboard[subdisplay]

        elif interface == "Mouse":
            mouse = avango.display.MouseDevice()
            mouse.connect(self._subdisplay_window_events[subdisplay],
                          self._subdisplay_camera[subdisplay],
                          self._subdisplay_window[subdisplay]
                          )
            return mouse
        
        else:
            device = avango.display.nodes.Device()
            device.Matrix.connect_from(self._subdisplay_camera[subdisplay].MouseNearTransform)
            device.Button1.connect_from(self._subdisplay_window_events[subdisplay].MouseButtons_OnlyLeft)
            return device


    def make_device(self, device, interface):
        if device == "SpaceMouse" or interface == "Relative6DOF":

            if not self._device_service:
                self._device_service = avango.daemon.DeviceService()
    
            sensor = avango.daemon.nodes.DeviceSensor(DeviceService = self._device_service,
                                                      Station = "spacemousestation")
            self.keep_alive(sensor)
    
            spacemouse = avango.display.nodes.SpaceMouseDevice()
            spacemouse.SensorAbsX.connect_from(sensor.Value0)
            spacemouse.SensorAbsY.connect_from(sensor.Value1)
            spacemouse.SensorAbsZ.connect_from(sensor.Value2)
            spacemouse.SensorAbsRX.connect_from(sensor.Value3)
            spacemouse.SensorAbsRY.connect_from(sensor.Value4)
            spacemouse.SensorAbsRZ.connect_from(sensor.Value5)
            spacemouse.SensorRelX.connect_from(sensor.Value6)
            spacemouse.SensorRelY.connect_from(sensor.Value7)
            spacemouse.SensorRelZ.connect_from(sensor.Value8)
            spacemouse.SensorRelRX.connect_from(sensor.Value9)
            spacemouse.SensorRelRY.connect_from(sensor.Value10)
            spacemouse.SensorRelRZ.connect_from(sensor.Value11)
            spacemouse.SensorBtnA0.connect_from(sensor.Button0)
            spacemouse.SensorBtnA1.connect_from(sensor.Button1)
            spacemouse.SensorBtnB0.connect_from(sensor.Button9)
            spacemouse.SensorBtnB1.connect_from(sensor.Button10)
            spacemouse.SensorBtnB2.connect_from(sensor.Button3)
            spacemouse.SensorBtnB3.connect_from(sensor.Button4)
    
            time_sensor = avango.nodes.TimeSensor()
            self.keep_alive(time_sensor)
            spacemouse.TimeIn.connect_from(time_sensor.Time)
    
            return spacemouse
        
        elif device == "DTrackVRPN":
            
            assert(len(interface)==2)
            #interface must look like this ["DTrack@localhost",[[1,"ve-dtrack-head1"], ...]]
            import avango.vrpn
            #create a dTrack device
            generic_dtrack_device = avango.vrpn.nodes.Device()
            generic_dtrack_device.VRPNID.value=interface[0]
            dtrack_device = avango.vrpn.nodes.DTrackDevice()
            #connect the tracker output from the generic device
            dtrack_device.TrackerInfo.connect_from(generic_dtrack_device.TrackerInfo)
            #register some interested tracker ids and names
            dtrack_device.populate_interested_target_ids(interface[1], False)
            
            return [generic_dtrack_device, dtrack_device]
        
        elif device == "Wiimote":
            import avango.vrpn
            #create a wiimote
            wiimote = avango.vrpn.nodes.Wiimote()
            wiimote.VRPNID.value=interface
            
            return wiimote
        
            
        elif device == "GamePad":
            
            if not self._device_service:
                self._device_service = avango.daemon.DeviceService()
                
            sensor = avango.daemon.nodes.DeviceSensor(DeviceService = self._device_service,
                                                           Station = "gamepadstation")
            self.keep_alive(sensor)
 
            gamepad = avango.display.nodes.GamePadDevice()
            gamepad.connect(sensor)
 
            time_sensor = avango.nodes.TimeSensor()
            self.keep_alive(time_sensor)
            gamepad.TimeIn.connect_from(time_sensor.Time)
            
            return gamepad
        
