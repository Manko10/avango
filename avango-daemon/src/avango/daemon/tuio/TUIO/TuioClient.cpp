/*
 TUIO C++ Library - part of the reacTIVision project
 http://reactivision.sourceforge.net/
 
 Copyright (c) 2005-2009 Martin Kaltenbrunner <mkalten@iua.upf.edu>
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "TuioClient.h"

using namespace TUIO;
using namespace osc;

#ifndef WIN32
static void* ClientThreadFunc( void* obj )
#else
static DWORD WINAPI ClientThreadFunc( LPVOID obj )
#endif
{
	static_cast<TuioClient*>(obj)->socket->Run();
	return 0;
};

void TuioClient::lockObjectList() {
	if(!connected) return;
#ifndef WIN32	
	pthread_mutex_lock(&objectMutex);
#else
	WaitForSingleObject(objectMutex, INFINITE);
#endif		
}

void TuioClient::unlockObjectList() {
	if(!connected) return;
#ifndef WIN32	
	pthread_mutex_unlock(&objectMutex);
#else
	ReleaseMutex(objectMutex);
#endif
}

void TuioClient::lockCursorList() {
	if(!connected) return;
#ifndef WIN32	
	pthread_mutex_lock(&cursorMutex);
#else
	WaitForSingleObject(cursorMutex, INFINITE);
#endif		
}

void TuioClient::unlockCursorList() {
	if(!connected) return;
#ifndef WIN32	
	pthread_mutex_unlock(&cursorMutex);
#else
	ReleaseMutex(cursorMutex);
#endif		
}

void TuioClient::lockHandList() {
    if(!connected) return;
#ifndef WIN32
    pthread_mutex_lock(&handMutex);
#else
    WaitForSingleObject(handMutex, INFINITE);
#endif
}

void TuioClient::unlockHandList() {
    if(!connected) return;
#ifndef WIN32
    pthread_mutex_unlock(&handMutex);
#else
    ReleaseMutex(handMutex);
#endif
}

void TuioClient::lockFingerList() {
    if(!connected) return;
#ifndef WIN32
    pthread_mutex_lock(&fingerMutex);
#else
    WaitForSingleObject(fingerMutex, INFINITE);
#endif
}

void TuioClient::unlockFingerList() {
    if(!connected) return;
#ifndef WIN32
    pthread_mutex_unlock(&fingerMutex);
#else
    ReleaseMutex(fingerMutex);
#endif
}

TuioClient::TuioClient(int port)
: socket      (NULL)
, currentFrame(-1)
, maxCursorID (-1)
, thread      (NULL)
, locked      (false)
, connected   (false)
{
	try {
		socket = new UdpListeningReceiveSocket(IpEndpointName( IpEndpointName::ANY_ADDRESS, port ), this );
	} catch (std::exception &e) { 
		std::cerr << "could not bind to UDP port " << port << std::endl;
		socket = NULL;
	}
	
	if (socket!=NULL) {
		if (!socket->IsBound()) {
			delete socket;
			socket = NULL;
		} else std::cout << "listening to TUIO messages on UDP port " << port << std::endl;
	}	
}

TuioClient::~TuioClient() {	
	delete socket;
}

void TuioClient::ProcessBundle( const ReceivedBundle& b, const IpEndpointName& remoteEndpoint) {
	
	try {
		for( ReceivedBundle::const_iterator i = b.ElementsBegin(); i != b.ElementsEnd(); ++i ){
			if( i->IsBundle() )
				ProcessBundle( ReceivedBundle(*i), remoteEndpoint);
			else
				ProcessMessage( ReceivedMessage(*i), remoteEndpoint);
		}
	} catch (MalformedBundleException& e) {
		std::cerr << "malformed OSC bundle: " << e.what() << std::endl;
	}
	
}

void TuioClient::ProcessMessage( const ReceivedMessage& msg, const IpEndpointName& remoteEndpoint) {
    try {
		ReceivedMessageArgumentStream args = msg.ArgumentStream();
		ReceivedMessage::const_iterator arg = msg.ArgumentsBegin();
		
		if( strcmp( msg.AddressPattern(), "/tuio/2Dobj" ) == 0 ){
			
			const char* cmd;
			args >> cmd;
			
			if (strcmp(cmd,"set")==0) {	
												
				int32 s_id, c_id;
				float xpos, ypos, angle, xspeed, yspeed, rspeed, maccel, raccel;
				args >> s_id >> c_id >> xpos >> ypos >> angle >> xspeed >> yspeed >> rspeed >> maccel >> raccel;
				
				lockObjectList();
				std::list<TuioObject*>::iterator tobj;
				for (tobj=objectList.begin(); tobj!= objectList.end(); tobj++)
					if((*tobj)->getSessionID()==(long)s_id) break;
				
				if (tobj == objectList.end()) {
					
					TuioObject *addObject = new TuioObject((long)s_id,(int)c_id,xpos,ypos,angle);
					frameObjects.push_back(addObject);

				} else if ( ((*tobj)->getX()!=xpos) || ((*tobj)->getY()!=ypos) || ((*tobj)->getAngle()!=angle) || ((*tobj)->getXSpeed()!=xspeed) || ((*tobj)->getYSpeed()!=yspeed) || ((*tobj)->getRotationSpeed()!=rspeed) || ((*tobj)->getMotionAccel()!=maccel) || ((*tobj)->getRotationAccel()!=raccel) ) {

					TuioObject *updateObject = new TuioObject((long)s_id,(*tobj)->getSymbolID(),xpos,ypos,angle);
					updateObject->update(xpos,ypos,angle,xspeed,yspeed,rspeed,maccel,raccel);
					frameObjects.push_back(updateObject);
					
				}
				unlockObjectList();

			} else if (strcmp(cmd,"alive")==0) {
				
				int32 s_id;
				aliveObjectList.clear();
				while(!args.Eos()) {
					args >> s_id;
					aliveObjectList.push_back((long)s_id);
				}
				
			} else if (strcmp(cmd,"fseq")==0) {
				
				int32 fseq;
				args >> fseq;
				bool lateFrame = false;
				if (fseq>0) {
					if (fseq>currentFrame) currentTime = TuioTime::getSessionTime();
					if ((fseq>=currentFrame) || ((currentFrame-fseq)>100)) currentFrame = fseq;
					else lateFrame = true;
				} else if ((TuioTime::getSessionTime().getTotalMilliseconds()-currentTime.getTotalMilliseconds())>100) {
					currentTime = TuioTime::getSessionTime();
				}
			
				if (!lateFrame) {
					
					lockObjectList();
					//find the removed objects first
					for (std::list<TuioObject*>::iterator tobj=objectList.begin(); tobj != objectList.end(); tobj++) {
						std::list<long>::iterator iter = find(aliveObjectList.begin(), aliveObjectList.end(), (*tobj)->getSessionID());
						if (iter == aliveObjectList.end()) {
							(*tobj)->remove(currentTime);
							frameObjects.push_back(*tobj);							
						}
					}
					unlockObjectList();
					
					for (std::list<TuioObject*>::iterator iter=frameObjects.begin(); iter != frameObjects.end(); iter++) {
						TuioObject *tobj = (*iter);

						TuioObject *frameObject = NULL;
						switch (tobj->getTuioState()) {
							case TUIO_REMOVED:
								frameObject = tobj;
								frameObject->remove(currentTime);

								for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
									(*listener)->removeTuioObject(frameObject);

								lockObjectList();								
								for (std::list<TuioObject*>::iterator delobj=objectList.begin(); delobj!=objectList.end(); delobj++) {
									if((*delobj)->getSessionID()==frameObject->getSessionID()) {
										objectList.erase(delobj);
										break;
									}
								}
								unlockObjectList();
								break;
							case TUIO_ADDED:
								lockObjectList();
								frameObject = new TuioObject(currentTime,tobj->getSessionID(),tobj->getSymbolID(),tobj->getX(),tobj->getY(),tobj->getAngle());
								objectList.push_back(frameObject);
								unlockObjectList();
								
								for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
									(*listener)->addTuioObject(frameObject);
								
								break;
							default:
								
								lockObjectList();
								std::list<TuioObject*>::iterator iter;
								for (iter=objectList.begin(); iter != objectList.end(); iter++) {
									if((*iter)->getSessionID()==tobj->getSessionID()) {
										frameObject = (*iter);
										break;
									}
								}
								if(iter==objectList.end()) break;
								
								if ( (tobj->getX()!=frameObject->getX() && tobj->getXSpeed()==0) || (tobj->getY()!=frameObject->getY() && tobj->getYSpeed()==0) )
									frameObject->update(currentTime,tobj->getX(),tobj->getY(),tobj->getAngle());
								else
									frameObject->update(currentTime,tobj->getX(),tobj->getY(),tobj->getAngle(),tobj->getXSpeed(),tobj->getYSpeed(),tobj->getRotationSpeed(),tobj->getMotionAccel(),tobj->getRotationAccel());
								unlockObjectList();
								
								for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
									(*listener)->updateTuioObject(frameObject);
								
						}
						delete tobj;
					}

					for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
						(*listener)->refresh(currentTime);
					
				} else {
					for (std::list<TuioObject*>::iterator iter=frameObjects.begin(); iter != frameObjects.end(); iter++) {
						TuioObject *tobj = (*iter);
						delete tobj;
					}
				}
				
				frameObjects.clear();
			}
		} else if( strcmp( msg.AddressPattern(), "/tuio/2Dcur" ) == 0 ) {
			const char* cmd;
			args >> cmd;
			
			if (strcmp(cmd,"set")==0) {	

				int32 s_id;
				float xpos, ypos, xspeed, yspeed, maccel;				
				args >> s_id >> xpos >> ypos >> xspeed >> yspeed >> maccel;
				
				lockCursorList();
				std::list<TuioCursor*>::iterator tcur;
				for (tcur=cursorList.begin(); tcur!= cursorList.end(); tcur++)
					if((*tcur)->getSessionID()==(long)s_id) break;
				
				if (tcur==cursorList.end()) {
									
					TuioCursor *addCursor = new TuioCursor((long)s_id,-1,xpos,ypos);
					frameCursors.push_back(addCursor);

				} else if ( ((*tcur)->getX()!=xpos) || ((*tcur)->getY()!=ypos) || ((*tcur)->getXSpeed()!=xspeed) || ((*tcur)->getYSpeed()!=yspeed) || ((*tcur)->getMotionAccel()!=maccel) ) {

					TuioCursor *updateCursor = new TuioCursor((long)s_id,(*tcur)->getCursorID(),xpos,ypos);
					updateCursor->update(xpos,ypos,xspeed,yspeed,maccel);
					frameCursors.push_back(updateCursor);

				}
				unlockCursorList();
				
			} else if (strcmp(cmd,"alive")==0) {
				
				int32 s_id;
				aliveCursorList.clear();
				while(!args.Eos()) {
					args >> s_id;
					aliveCursorList.push_back((long)s_id);
				}
				
			} else if( strcmp( cmd, "fseq" ) == 0 ){
				
				int32 fseq;
				args >> fseq;
				bool lateFrame = false;
				if (fseq>0) {
					if (fseq>currentFrame) currentTime = TuioTime::getSessionTime();
					if ((fseq>=currentFrame) || ((currentFrame-fseq)>100)) currentFrame = fseq;
					else lateFrame = true;
				}  else if ((TuioTime::getSessionTime().getTotalMilliseconds()-currentTime.getTotalMilliseconds())>100) {
					currentTime = TuioTime::getSessionTime();
				}
			
				if (!lateFrame) {
					
					lockCursorList();
					// find the removed cursors first
					for (std::list<TuioCursor*>::iterator tcur=cursorList.begin(); tcur != cursorList.end(); tcur++) {
						std::list<long>::iterator iter = find(aliveCursorList.begin(), aliveCursorList.end(), (*tcur)->getSessionID());
							
						if (iter == aliveCursorList.end()) {
							(*tcur)->remove(currentTime);
							frameCursors.push_back(*tcur);
						}
					}
					unlockCursorList();

                    // remove stall fingers
                    lockFingerList();
                    fingerList.remove_if([&](TuioFinger* finger) {
                        bool stall= (std::find(aliveCursorList.begin(), aliveCursorList.end(), finger->getSessionID()) == aliveCursorList.end());
                        if (stall) {
                            for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++){
                                (*listener)->removeTuioFinger(finger);
                            }
                        }
                        return stall;
                    });
                    unlockFingerList();

                    // remove stall hands
                    lockHandList();
                    handList.remove_if([&](TuioHand* hand) {
                        auto fingerIDs = hand->getFingerIDs();
                        bool stall = true;
                        for (auto i : fingerIDs) {
                            stall &= (std::find(aliveCursorList.begin(), aliveCursorList.end(), i) == aliveCursorList.end());
                        }
                        if (stall) {
                            for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++){
                                (*listener)->removeTuioHand(hand);
                            }
                        }
                        return stall;
                    });
                    unlockHandList();
					
					for (std::list<TuioCursor*>::iterator iter=frameCursors.begin(); iter != frameCursors.end(); iter++) {
						TuioCursor *tcur = (*iter);
						
						int c_id = -1;
						TuioCursor *frameCursor = NULL;
						switch (tcur->getTuioState()) {
							case TUIO_REMOVED:
								frameCursor = tcur;
								frameCursor->remove(currentTime);

                                {
                                    std::list<TuioFinger*>::iterator removeFingerIter;
                                    for (removeFingerIter = fingerList.begin(); removeFingerIter != fingerList.end(); ++removeFingerIter) {
                                        if ((*removeFingerIter)->getSessionID() == frameCursor->getSessionID()) break;
                                    }

                                    for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++) {
                                        (*listener)->removeTuioCursor(frameCursor);
                                        if (removeFingerIter != fingerList.end())
                                            (*listener)->removeTuioFinger(*removeFingerIter);
                                    }
                                }

								lockCursorList();
								for (std::list<TuioCursor*>::iterator delcur=cursorList.begin(); delcur!=cursorList.end(); delcur++) {
									if((*delcur)->getSessionID()==frameCursor->getSessionID()) {
										cursorList.erase(delcur);
										break;
									}
								}

								if (frameCursor->getCursorID()==maxCursorID) {
									maxCursorID = -1;
									delete frameCursor;
									
									if (cursorList.size()>0) {
										std::list<TuioCursor*>::iterator clist;
										for (clist=cursorList.begin(); clist != cursorList.end(); clist++) {
											c_id = (*clist)->getCursorID();
											if (c_id>maxCursorID) maxCursorID=c_id;
										}
			
										freeCursorBuffer.clear();
										for (std::list<TuioCursor*>::iterator flist=freeCursorList.begin(); flist != freeCursorList.end(); flist++) {
											TuioCursor *freeCursor = (*flist);
											if (freeCursor->getCursorID()>maxCursorID) delete freeCursor;
											else freeCursorBuffer.push_back(freeCursor);
										}	
										freeCursorList = freeCursorBuffer;

									} else {
										for (std::list<TuioCursor*>::iterator flist=freeCursorList.begin(); flist != freeCursorList.end(); flist++) {
											TuioCursor *freeCursor = (*flist);
											delete freeCursor;
										}
										freeCursorList.clear();
									}
								} else if (frameCursor->getCursorID()<maxCursorID) {
									freeCursorList.push_back(frameCursor);
								} 
								
								unlockCursorList();
								break;
							case TUIO_ADDED:
								
								lockCursorList();
								c_id = (int)cursorList.size();
								if (((int)(cursorList.size())<=maxCursorID) && ((int)(freeCursorList.size())>0)) {
									std::list<TuioCursor*>::iterator closestCursor = freeCursorList.begin();
									
									for(std::list<TuioCursor*>::iterator iter = freeCursorList.begin();iter!= freeCursorList.end(); iter++) {
										if((*iter)->getDistance(tcur)<(*closestCursor)->getDistance(tcur)) closestCursor = iter;
									}
									
									TuioCursor *freeCursor = (*closestCursor);
									c_id = freeCursor->getCursorID();
									freeCursorList.erase(closestCursor);
									delete freeCursor;
								} else maxCursorID = c_id;									
								
								frameCursor = new TuioCursor(currentTime,tcur->getSessionID(),c_id,tcur->getX(),tcur->getY());
								cursorList.push_back(frameCursor);
								
								delete tcur;
								unlockCursorList();

                                {
                                    std::list<TuioFinger*>::iterator newFingerIter;
                                    for (newFingerIter = fingerList.begin(); newFingerIter != fingerList.end(); ++newFingerIter) {
                                        if ((*newFingerIter)->getSessionID() == frameCursor->getSessionID()) break;
                                    }

                                    for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++) {
                                        (*listener)->addTuioCursor(frameCursor);
                                        if (newFingerIter != fingerList.end())
                                            (*listener)->addTuioFinger(*newFingerIter);
                                    }
                                }
								
								break;
							default:
								
								lockCursorList();
								std::list<TuioCursor*>::iterator iter;
								for (iter=cursorList.begin(); iter != cursorList.end(); iter++) {
									if((*iter)->getSessionID()==tcur->getSessionID()) {
										frameCursor = (*iter);
										break;
									}
								}
								
								if ( (tcur->getX()!=frameCursor->getX() && tcur->getXSpeed()==0) || (tcur->getY()!=frameCursor->getY() && tcur->getYSpeed()==0) )
									frameCursor->update(currentTime,tcur->getX(),tcur->getY());
								else
									frameCursor->update(currentTime,tcur->getX(),tcur->getY(),tcur->getXSpeed(),tcur->getYSpeed(),tcur->getMotionAccel());
						
								delete tcur;
								unlockCursorList();

                                {
                                    std::list<TuioFinger*>::iterator updateFingerIter;
                                    for (updateFingerIter = fingerList.begin(); updateFingerIter != fingerList.end(); ++updateFingerIter) {
                                        if ((*updateFingerIter)->getSessionID() == frameCursor->getSessionID()) break;
                                    }

                                    for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++){
                                        (*listener)->updateTuioCursor(frameCursor);
                                        if (updateFingerIter != fingerList.end())
                                            (*listener)->updateTuioFinger(*updateFingerIter);
                                    }
                                }
						}	
					}
					
					for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++)
						(*listener)->refresh(currentTime);
					
				} else {
					for (std::list<TuioCursor*>::iterator iter=frameCursors.begin(); iter != frameCursors.end(); iter++) {
						TuioCursor *tcur = (*iter);
						delete tcur;
					}
				}
				
				frameCursors.clear();
			}
        } else if ( strcmp( msg.AddressPattern(), "/tuiox/finger" ) == 0 ) {
            int32 s_id;
            float xpos, ypos, xspeed, yspeed, xellipse,yellipse, minoraxis, majoraxis, incl;
            args >> s_id >> xpos >> ypos >> xspeed >> yspeed >> xellipse >> yellipse >> minoraxis >> majoraxis >> incl;

            lockFingerList();
            std::list<TuioFinger*>::iterator tfinger;
            for ( tfinger = fingerList.begin(); tfinger != fingerList.end(); ++tfinger) {
                if (( *tfinger)->getSessionID() == (long)s_id) break;
            }

            if ( tfinger == fingerList.end()) {
                TuioFinger* addFinger = new TuioFinger(
                                            (long)s_id,
                                            xpos, ypos,
                                            xspeed, yspeed,
                                            xellipse, yellipse,
                                            minoraxis, majoraxis,
                                            incl
                );
                fingerList.push_back(addFinger);
            } else {
                (*tfinger)->update(xpos, ypos, xspeed, yspeed, xellipse, yellipse, minoraxis, majoraxis, incl);
            }

            unlockFingerList();
        } else if ( strcmp( msg.AddressPattern(), "/tuiox/hand" ) == 0 ) {
            int32 s_id, hand_class, f1, f2, f3, f4, f5;
            args >> s_id >> hand_class >> f1 >> f2 >> f3 >> f4 >> f5;

            lockHandList();
            std::list<TuioHand*>::iterator thand;
            for ( thand = handList.begin(); thand != handList.end(); ++thand) {
                if (( *thand)->getSessionID() == (long)s_id) break;
            }

            if ( thand == handList.end()) {
                TuioHand* addHand = new TuioHand(
                                            (long)s_id, (TuioHand::Class)hand_class,
                                            (long)f1, (long)f2, (long)f3, (long)f4, (long)f5
                );
                handList.push_back(addHand);
                for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++){
                    (*listener)->addTuioHand(addHand);
                }
            } else {
                (*thand)->update((long)f1, (long)f2, (long)f3, (long)f4, (long)f5);
                for (std::list<TuioListener*>::iterator listener=listenerList.begin(); listener != listenerList.end(); listener++){
                    (*listener)->updateTuioHand(*thand);
                }
            }

            unlockHandList();
        }
    } catch( Exception& e ){
		std::cerr << "error parsing TUIO message: "<< msg.AddressPattern() <<  " - " << e.what() << std::endl;
    }
}

void TuioClient::ProcessPacket( const char *data, int size, const IpEndpointName& remoteEndpoint ) {
	try {
		ReceivedPacket p( data, size );
		if(p.IsBundle()) ProcessBundle( ReceivedBundle(p), remoteEndpoint);
		else ProcessMessage( ReceivedMessage(p), remoteEndpoint);
	} catch (MalformedBundleException& e) {
		std::cerr << "malformed OSC bundle: " << e.what() << std::endl;
	}
}

void TuioClient::connect(bool lk) {

#ifndef WIN32	
	/*pthread_mutexattr_settype(&attr_p, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&cursorMutex,&attr_p);
	pthread_mutex_init(&objectMutex,&attr_p);*/
	pthread_mutex_init(&cursorMutex,NULL);
    pthread_mutex_init(&objectMutex,NULL);
    pthread_mutex_init(&fingerMutex,NULL);
    pthread_mutex_init(&handMutex,NULL);
#else
	cursorMutex = CreateMutex(NULL,FALSE,"cursorMutex");
	objectMutex = CreateMutex(NULL,FALSE,"objectMutex");
    fingerMutex = CreateMutex(NULL,FALSE,"fingerMutex");
    handMutex   = CreateMutex(NULL,FALSE,"handMutex");
#endif		
		
	if (socket==NULL) return;
	TuioTime::initSession();
	currentTime.reset();
	
	locked = lk;
	if (!locked) {
#ifndef WIN32
		pthread_create(&thread , NULL, ClientThreadFunc, this);
#else
		DWORD threadId;
		thread = CreateThread( 0, 0, ClientThreadFunc, this, 0, &threadId );
#endif
	} else socket->Run();
	
	connected = true;
	unlockCursorList();
	unlockObjectList();
    unlockFingerList();
    unlockHandList();
}

void TuioClient::disconnect() {
	
	if (socket==NULL) return;
	socket->Break();
	
	if (!locked) {
#ifdef WIN32
		if( thread ) CloseHandle( thread );
#endif
		thread = 0;
		locked = false;
	}
	
#ifndef WIN32	
	pthread_mutex_destroy(&cursorMutex);
	pthread_mutex_destroy(&objectMutex);
    pthread_mutex_destroy(&fingerMutex);
    pthread_mutex_destroy(&handMutex);
#else
	CloseHandle(cursorMutex);
	CloseHandle(objectMutex);
    CloseHandle(fingerMutex);
    CloseHandle(handMutex);
#endif

	aliveObjectList.clear();
    aliveCursorList.clear();

	for (std::list<TuioObject*>::iterator iter=objectList.begin(); iter != objectList.end(); iter++)
		delete (*iter);
	objectList.clear();

	for (std::list<TuioCursor*>::iterator iter=cursorList.begin(); iter != cursorList.end(); iter++)
		delete (*iter);
	cursorList.clear();

    for (std::list<TuioFinger*>::iterator iter=fingerList.begin(); iter != fingerList.end(); iter++)
        delete (*iter);
    fingerList.clear();

    for (std::list<TuioHand*>::iterator iter=handList.begin(); iter != handList.end(); iter++)
        delete (*iter);
    handList.clear();
	
	for (std::list<TuioCursor*>::iterator iter=freeCursorList.begin(); iter != freeCursorList.end(); iter++)
		delete(*iter);
	freeCursorList.clear();

	connected = false;
}

void TuioClient::addTuioListener(TuioListener *listener) {
	listenerList.push_back(listener);
}

void TuioClient::removeTuioListener(TuioListener *listener) {
	std::list<TuioListener*>::iterator result = find(listenerList.begin(),listenerList.end(),listener);
	if (result!=listenerList.end()) listenerList.remove(listener);
}

TuioObject* TuioClient::getTuioObject(long s_id) {
	lockObjectList();
	for (std::list<TuioObject*>::iterator iter=objectList.begin(); iter != objectList.end(); iter++) {
		if((*iter)->getSessionID()==s_id) {
			unlockObjectList();
			return (*iter);
		}
	}	
	unlockObjectList();
	return NULL;
}

TuioCursor* TuioClient::getTuioCursor(long s_id) {
	lockCursorList();
	for (std::list<TuioCursor*>::iterator iter=cursorList.begin(); iter != cursorList.end(); iter++) {
		if((*iter)->getSessionID()==s_id) {
			unlockCursorList();
			return (*iter);
		}
	}	
	unlockCursorList();
	return NULL;
}

TuioFinger* TuioClient::getTuioFinger(long s_id) {
    lockFingerList();
    for (std::list<TuioFinger*>::iterator iter=fingerList.begin(); iter != fingerList.end(); iter++) {
        if((*iter)->getSessionID()==s_id) {
            unlockFingerList();
            return (*iter);
        }
    }
    unlockFingerList();
    return NULL;
}

TuioHand* TuioClient::getTuioHand(long s_id) {
    lockHandList();
    for (std::list<TuioHand*>::iterator iter=handList.begin(); iter != handList.end(); iter++) {
        if((*iter)->getSessionID()==s_id) {
            unlockHandList();
            return (*iter);
        }
    }
    unlockHandList();
    return NULL;
}

std::list<TuioObject*> TuioClient::getTuioObjects() {
	lockObjectList();
	std::list<TuioObject*> listBuffer = objectList;
	unlockObjectList();
	return listBuffer;
}

std::list<TuioCursor*> TuioClient::getTuioCursors() {
	lockCursorList();
	std::list<TuioCursor*> listBuffer = cursorList;
	unlockCursorList();
	return listBuffer;
}

std::list<TuioFinger*> TuioClient::getTuioFingers() {
    lockFingerList();
    std::list<TuioFinger*> listBuffer = fingerList;
    unlockFingerList();
    return listBuffer;
}

std::list<TuioHand*> TuioClient::getTuioHands() {
    lockHandList();
    std::list<TuioHand*> listBuffer = handList;
    unlockHandList();
    return listBuffer;
}
