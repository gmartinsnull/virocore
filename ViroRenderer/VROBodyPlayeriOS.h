//
//  VROBodyPlayeriOS.h
//  ViroRenderer
//
//  Created by vik.advani on 1/21/19.
//  Copyright © 2019 Viro Media. All rights reserved.
//

#ifndef VROBodyPlayeriOS_h
#define VROBodyPlayeriOS_h

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <map>
#include "VROBodyPlayer.h"
#include "VROTime.h"
#include "VROMatrix4f.h"
#include "VROBodyAnimData.h"

class VRORenderContext;
class BodyPlaybackInfo {
public:
    BodyPlaybackInfo(std::shared_ptr<VROBodyAnimData> data) {
        _bodyAnimData = data;
        _currentPlaybackRow = 0;
        _playStatus = VROBodyPlayerStatus::Initialized;
    }

    std::map<VROBodyJointType, VROVector3f> getCurrentRowJointsAsMap()
    {
        return _bodyAnimData->getAnimRowJoints(_currentPlaybackRow);
    }

    void start() {
        if (_playStatus == VROBodyPlayerStatus::Initialized || _playStatus == VROBodyPlayerStatus::Finished)
        {
            _currentPlaybackRow = 0;
            _startPlaybackTime = VROTimeCurrentMillis();
            _playStatus = VROBodyPlayerStatus::Start;
        } else if(_playStatus == VROBodyPlayerStatus::Paused) {
             double currentTime = VROTimeCurrentMillis();
            _startPlaybackTime = currentTime -_processTimeWhenPaused;
            _playStatus = VROBodyPlayerStatus::Playing;
        }
    }

    void pause() {
        double currentTime = VROTimeCurrentMillis();
        _processTimeWhenPaused = currentTime - _startPlaybackTime;
        _playStatus = VROBodyPlayerStatus::Paused;
    }

    void setTime(double time) {
        long totalRows = _bodyAnimData->getTotalRows();
        if (totalRows == 0) {
            return;
        }

        if (time > _totalPlaybackTime) {
            _currentPlaybackRow = totalRows -1;
        }

        long currentRow = returnRowClosestToTime(time, 0, totalRows);
        if (currentRow <= totalRows-1) {
            _currentPlaybackRow = currentRow;
            _startPlaybackTime = VROTimeCurrentMillis() - getCurrentRowTimestamp();
        }
     }

    long getCurrentRow() {
        return _currentPlaybackRow;
    }

    double getStartTime() {
        return _startPlaybackTime;
    }

    VROMatrix4f getInitWorldMatrix() {
        return _bodyAnimData->getModelStartWorldMatrix();
    }

    double getCurrentRowTimestamp() {
        return _bodyAnimData->getAnimRowTimestamp(_currentPlaybackRow);
    }

    void incrementAnimRow() {
        _currentPlaybackRow++;
        long length = _bodyAnimData->getTotalRows();
        if (_currentPlaybackRow > 0 && _currentPlaybackRow < length && _playStatus == VROBodyPlayerStatus::Start) {
            _playStatus = VROBodyPlayerStatus::Playing;
        } else if (_currentPlaybackRow >= length) {
            if (_playStatus == VROBodyPlayerStatus::Playing) {
                _playStatus = VROBodyPlayerStatus::Finished;
            }
        }
    }

    VROBodyPlayerStatus getPlayStatus() {
        return  _playStatus;
    }

    double getTotalTime() {
        return _totalPlaybackTime;
    }

    bool isFinished() {
        long length = _bodyAnimData->getTotalRows();
        if (_currentPlaybackRow >= length) {
            return true;
        }
        return false;
    }

private:
    
    long returnRowClosestToTime(double time, long lowerBoundIndex, long upperBoundIndex) {
        if (lowerBoundIndex == upperBoundIndex) {
            return lowerBoundIndex;
        }
        
        long midRow = lowerBoundIndex + (upperBoundIndex-lowerBoundIndex)/2;
        double timestamp = _bodyAnimData->getAnimRowTimestamp(midRow);
       
        if (time > timestamp) {
            return (returnRowClosestToTime(time, midRow+1, upperBoundIndex));
        } else {
            return (returnRowClosestToTime(time, lowerBoundIndex, midRow));
        }
    }

    long _currentPlaybackRow;
    VROBodyPlayerStatus _playStatus;
    std::shared_ptr<VROBodyAnimData> _bodyAnimData;
    double _startPlaybackTime;
    double _processTimeWhenPaused;
    double _totalPlaybackTime;
};

class VROBodyPlayeriOS : public VROBodyPlayer {

public:
    VROBodyPlayeriOS();

    virtual ~VROBodyPlayeriOS() {}
    void start();
    void pause();
    void prepareAnimation(std::shared_ptr<VROBodyAnimData> bodyAnimData);
    void setTime(double time);
    void onFrameWillRender(const VRORenderContext &context);
    void onFrameDidRender(const VRORenderContext &context);

private:
    std::shared_ptr<BodyPlaybackInfo> _playbackInfo;
};

#endif /* VROBodyPlayeriOS_h */
