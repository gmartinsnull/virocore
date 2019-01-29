//
//  VROBodyAnimData.h
//  ViroRenderer
//
//  Created by Vik Advani on 1/24/19.
//  Copyright © 2019 Viro Media. All rights reserved.
//

#ifndef VROBodyAnimData_h
#define VROBodyAnimData_h

#include <string>
#include <memory>
#include <map>
#include "VROVector3f.h"
#include "VROMatrix4f.h"
#include "VROBodyTracker.h"

/*
  Stores data related to body animation captured with VROBodyTrackerController.
 */
class VROBodyAnimData {
public:
    VROBodyAnimData() { }

#pragma - VROBodyAnimData getters
    // get total time of animation in milliseconds.
    double getTotalTime() {
        return _totalTime;
    }

    // get version of body tracking animation.
    std::string getVersion() {
        return _version;
    }

    // get the recorded model animations world start matrix.
    VROMatrix4f getModelStartWorldMatrix() {
        return _worldStartMatrix;
    }

    // get total animation rows. Each row consists of a timestamp and set of joint positions.
    unsigned long getTotalRows() {
        return _animationRows.size();
    }

    // retrieve a map of joint type to joint positions for a row by index.
    std::map<VROBodyJointType, VROVector3f> getAnimRowJoints(long index) {
        return _animationRows[index];
    }

    // get animation row timestamp by row index.
    double getAnimRowTimestamp(long index) {
        return _animationRowTimestamps[index];
    }

#pragma - VROBodyAnimData getters
    
    // set the version as a string.
    void setVersion(std::string version) {
        _version = version;
    }

    // set the total time of this animation in milliseconds.
    void setTotalTime(double totalTime) {
        _totalTime = totalTime;
    }

    // set the start world matrix of the model that was recorded.
    void setModelStartWorldMatrix(VROMatrix4f matrix) {
        _worldStartMatrix = matrix;
    }

    // add an animation row that consists of world joint positions at the provided timestamp.
    void addAnimRow(double timestamp, std::map<VROBodyJointType, VROVector3f> jointMap) {
        _animationRowTimestamps.push_back(timestamp);
        _animationRows.push_back(jointMap);
    }

private:
    double _totalTime;
    int _totalAnimRows;
    std::string _version;
    VROMatrix4f _worldStartMatrix;
    std::vector<double> _animationRowTimestamps;
    std::vector<std::map<VROBodyJointType, VROVector3f>> _animationRows;
};

/*
Records data for a body animation that is tracked with VROBodyTrackerController.
Sequence to record an animation:

startRecording(startWorldMatrix) // begin recording with model start world matrix passed in.
   //For each joint callback:
   beginRecordingRow(); //invoked when recording another set of data of world joint positions and timestamps.
     addJointToRow(jointName, jointPos); //invoke for joint you want to record on current row.
   endRecordingRow() // invoke when done recording this row of data.
 
 When finished recording, invoke toJSON() to serialize the data into a C String.
 */
class VROBodyAnimDataRecorder {
public:
    VROBodyAnimDataRecorder() {};
    // begin recording body animation.
    virtual void startRecording(VROMatrix4f startWorldTransform) = 0;
    
    // stop recording the body animation.
    virtual void stopRecording() = 0;
    
    // begin recording new set of joint data at specific timestamp. Timestamp is marked as invocation of this method.
    virtual void beginRecordedRow() = 0;

    // add the current jointName and world joint position to the current recording row. This must be invoked after beginRecordedRow() and before endRecordedRow().
    virtual void addJointToRow(std::string jointName, VROVector3f jointPos) = 0;

    // end recording of joint data.
    virtual void endRecordedRow() = 0;
    
    /*
    Convert current recorded data to JSON as standard string. Format of JSON is the following:

    The format of the JSON animation is the following:
    {
    totalTime: (float) //total time of animation in milliseconds).
    animRows:
        [{     // Array of joint data that is given to VROBodyPlayerDelegate at specific timestamp.
        timestamp: (float) timestamp in milliseconds when this joint data should execute.
        joints:
            {
            Neck:[x,y,z],
            Shoulder:[x,y,z]...jointName:[x,y,z]
            } // Joints contains list of joints from kVROBodyBoneTags with x,y,z array data that  represent the joint in local space.
        }]
    }
    */
    virtual std::string toJSON() = 0;
};

/*
  Reads a JSON encoded standard C string and converts to a VROBodyAnimData structure.
 */
class VROBodyAnimDataReader {
public:
      VROBodyAnimDataReader() {};
      virtual std::shared_ptr<VROBodyAnimData> fromJSON(std::string jsonData) = 0;
};
#endif /* VROBodyAnimData_h */
