//
//  VROInputControllerBase.cpp
//  ViroRenderer
//
//  Copyright © 2017 Viro Media. All rights reserved.
//

#include "VROInputControllerBase.h"
#include "VROTime.h"
#include "VROPortal.h"

static bool sSceneBackgroundAdd = true;

VROInputControllerBase::VROInputControllerBase() {
    _lastKnownPosition = VROVector3f(0,0,0);
    _lastDraggedNodePosition = VROVector3f(0,0,0);
    _lastClickedNode = nullptr;
    _lastHoveredNode = nullptr;
    _lastDraggedNode = nullptr;
    _currentPinchedNode = nullptr;
    _currentRotateNode = nullptr;
    _scene = nullptr;
    _currentControllerStatus = VROEventDelegate::ControllerStatus::Unknown;
    
#if VRO_PLATFORM_IOS
    if (kDebugSceneBackgroundDistance) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            debugMoveReticle();
        });
    }
#endif
}

void VROInputControllerBase::debugMoveReticle() {
    if (sSceneBackgroundAdd) {
        kSceneBackgroundDistance += 0.1;
        if (kSceneBackgroundDistance > 20) {
            sSceneBackgroundAdd = false;
        }
    }
    else {
        kSceneBackgroundDistance -= 0.1;
        if (kSceneBackgroundDistance < 0) {
            sSceneBackgroundAdd = true;
        }
    }

#if VRO_PLATFORM_IOS
    pinfo("Background distance is %f", kSceneBackgroundDistance);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        debugMoveReticle();
    });
#endif
}


void VROInputControllerBase::onButtonEvent(int source, VROEventDelegate::ClickState clickState){
    // Return if we have not focused on any node upon which to trigger events.
    if (_hitResult == nullptr) {
        return;
    }

    VROVector3f hitLoc = _hitResult->getLocation();
    std::vector<float> pos = {hitLoc.x, hitLoc.y, hitLoc.z};
    if (_hitResult->isBackgroundHit()) {
        pos.clear();
    }

    // Notify internal delegates
    for (std::shared_ptr<VROEventDelegate> delegate : _delegates) {
        delegate->onClick(source, clickState, pos);
    }

    std::shared_ptr<VRONode> focusedNode = getNodeToHandleEvent(VROEventDelegate::EventAction::OnClick, _hitResult->getNode());
    if (focusedNode != nullptr) {
        focusedNode->getEventDelegate()->onClick(source, clickState, pos);
    }

    /*
     If we have completed a ClickUp and ClickDown event sequentially for a
     given Node, trigger an onClicked event.
     
     NOTE: This only tracks the last node that was CLICKED_DOWN regardless of source;
     it does not consider the corner case where DOWN / UP events may be performed from
     different sources.
     */
    if (clickState == VROEventDelegate::ClickUp) {
        if (_hitResult->getNode() == _lastClickedNode) {
            for (std::shared_ptr<VROEventDelegate> delegate : _delegates){
                delegate->onClick(source, VROEventDelegate::ClickState::Clicked, pos);
            }
            if (focusedNode != nullptr && focusedNode->getEventDelegate() && _lastClickedNode != nullptr) {
                focusedNode->getEventDelegate()->onClick(source,
                                                         VROEventDelegate::ClickState::Clicked,
                                                         pos);
            }
        }
        _lastClickedNode = nullptr;
        if (_lastDraggedNode != nullptr) {
            _lastDraggedNode->_draggedNode->setIsBeingDragged(false);
        }
        _lastDraggedNode = nullptr;
    } else if (clickState == VROEventDelegate::ClickDown){
        _lastClickedNode = _hitResult->getNode();

        // Identify if object is draggable.
        std::shared_ptr<VRONode> draggableNode
                = getNodeToHandleEvent(VROEventDelegate::EventAction::OnDrag,
                                       _hitResult->getNode());
        
        if (draggableNode == nullptr){
            return;
        }
        
        draggableNode->setIsBeingDragged(true);

        /*
         Grab and save a reference to the draggedNode that we will be tracking.
         Grab and save the distance of the hit result from the controller.
         Grab and save the hit location from the hit test and original draggedNode position.
         For each of the above, store them within _lastDraggedNode to be used later
         within onMove to calculate the new dragged location of the draggedNode
         in reference to the controller's movement.
         */
        std::shared_ptr<VRODraggedObject> draggedObject = std::make_shared<VRODraggedObject>();
        draggedObject->_draggedDistanceFromController = _hitResult->getLocation().distanceAccurate(_lastKnownPosition);
        draggedObject->_originalHitLocation = _hitResult->getLocation();
        draggedObject->_originalDraggedNodePosition = draggableNode->getPosition();
        draggedObject->_draggedNode = draggableNode;

        // Grab the forwardOffset (delta from the controller's forward in reference to the user).
        draggedObject->_forwardOffset = getDragForwardOffset();

        _lastDraggedNode = draggedObject;
    }
}

void VROInputControllerBase::onTouchpadEvent(int source, VROEventDelegate::TouchState touchState,
                                             float posX,
                                             float posY) {
    // Avoid spamming similar TouchDownMove events.
    VROVector3f currentTouchedPosition = VROVector3f(posX, posY, 0);
    if (touchState == VROEventDelegate::TouchState::TouchDownMove &&
        _lastTouchedPosition.isEqual(currentTouchedPosition)) {
        return;
    }
    _lastTouchedPosition = currentTouchedPosition;

    // Notify internal delegates
    for (std::shared_ptr<VROEventDelegate> delegate : _delegates) {
        delegate->onTouch(source, touchState, posX, posY);
    }

    // Return if we have not focused on any node upon which to trigger events.
    if (_hitResult == nullptr) {
        return;
    }

    std::shared_ptr<VRONode> focusedNode = getNodeToHandleEvent(
            VROEventDelegate::EventAction::OnTouch, _hitResult->getNode());
    if (focusedNode != nullptr) {
        focusedNode->getEventDelegate()->onTouch(source, touchState, posX, posY);
    }
}

void VROInputControllerBase::onMove(int source, VROVector3f position, VROQuaternion rotation, VROVector3f forward) {
    _lastKnownRotation = rotation;
    _lastKnownPosition = position;
    _lastKnownForward = forward;
    if (_hitResult == nullptr) {
        return;
    }

    // Trigger orientation delegate callbacks for non-scene elements.
    for (std::shared_ptr<VROEventDelegate> delegate : _delegates) {
        delegate->onGazeHit(source, *_hitResult.get());
        delegate->onMove(source, _lastKnownRotation.toEuler(), _lastKnownPosition, _lastKnownForward);
    }

    // Trigger orientation delegate callbacks within the scene.
    processOnFuseEvent(source, _hitResult->getNode());

    std::shared_ptr<VRONode> gazableNode
            = getNodeToHandleEvent(VROEventDelegate::EventAction::OnHover, _hitResult->getNode());
    processGazeEvent(source, gazableNode);

    std::shared_ptr<VRONode> movableNode
            = getNodeToHandleEvent(VROEventDelegate::EventAction::OnMove, _hitResult->getNode());
    if (movableNode != nullptr) {
        movableNode->getEventDelegate()->onMove(source, _lastKnownRotation.toEuler(),
                                                _lastKnownPosition, _lastKnownForward);
    }
    
    // Update draggable objects if needed unless we have a pinch motion.
    if (_lastDraggedNode != nullptr && ((_currentPinchedNode == nullptr) && (_currentRotateNode == nullptr))) {
        processDragging(source);
    }
}

void VROInputControllerBase::processDragging(int source) {
    // Calculate the new drag location
    VROVector3f adjustedForward = _lastKnownForward + _lastDraggedNode->_forwardOffset;
    VROVector3f newSimulatedHitPosition = _lastKnownPosition + (adjustedForward  * _lastDraggedNode->_draggedDistanceFromController);
    VROVector3f draggedOffset = newSimulatedHitPosition - _lastDraggedNode->_originalHitLocation;
    VROVector3f draggedToLocation = _lastDraggedNode->_originalDraggedNodePosition + draggedOffset;
    std::shared_ptr<VRONode> draggedNode = _lastDraggedNode->_draggedNode;
    draggedNode->setPosition(draggedToLocation);
    
    /*
     To avoid spamming the JNI / JS bridge, throttle the notification
     of onDrag delegates to a certain degree of accuracy.
     */
    float distance = draggedToLocation.distance(_lastDraggedNodePosition);
    if (distance < ON_DRAG_DISTANCE_THRESHOLD) {
        return;
    }
    
    // Update last known dragged position and notify delegates
    _lastDraggedNodePosition = draggedToLocation;
    if (draggedNode != nullptr && draggedNode->getEventDelegate()){
        draggedNode->getEventDelegate()->onDrag(source, draggedToLocation);
    }
    for (std::shared_ptr<VROEventDelegate> delegate : _delegates) {
        delegate->onDrag(source, draggedToLocation);
    }
}

void VROInputControllerBase::onPinch(int source, float scaleFactor, VROEventDelegate::PinchState pinchState) {
    if(pinchState == VROEventDelegate::PinchState::PinchStart) {
        if(_hitResult == nullptr) {
            return;
        }
        _lastPinchScale = scaleFactor;
        _currentPinchedNode = getNodeToHandleEvent(VROEventDelegate::EventAction::OnPinch, _hitResult->getNode());
    }
    
    if(_currentPinchedNode && pinchState == VROEventDelegate::PinchState::PinchMove) {
        if(fabs(scaleFactor - _lastPinchScale) < ON_PINCH_SCALE_THRESHOLD) {
            return;
        }
    }

    if(_currentPinchedNode && _currentPinchedNode->getEventDelegate()) {
        _currentPinchedNode->getEventDelegate()->onPinch(source, scaleFactor, pinchState);
        if(pinchState == VROEventDelegate::PinchState::PinchEnd) {
            _currentPinchedNode = nullptr;
        }
    }
}


void VROInputControllerBase::onRotate(int source, float rotationFactor, VROEventDelegate::RotateState rotateState) {
    if(rotateState == VROEventDelegate::RotateState::RotateStart) {
        if(_hitResult == nullptr) {
            return;
        }
        _lastRotation = rotationFactor;
        _currentRotateNode = getNodeToHandleEvent(VROEventDelegate::EventAction::OnRotate, _hitResult->getNode());
    }
    
    if(_currentRotateNode && rotateState == VROEventDelegate::RotateState::RotateMove) {
        if(fabs(rotationFactor - _lastRotation) < ON_ROTATE_THRESHOLD) {
            return;
        }
    }
    
    if(_currentRotateNode && _currentRotateNode->getEventDelegate()) {
        _currentRotateNode->getEventDelegate()->onRotate(source, rotationFactor, rotateState);
        if(rotateState == VROEventDelegate::RotateState::RotateEnd) {
            _currentRotateNode = nullptr;
        }
    }
}

void VROInputControllerBase::updateHitNode(const VROCamera &camera, VROVector3f origin, VROVector3f ray) {
    if (_scene == nullptr || _lastDraggedNode != nullptr) {
        return;
    }

    // Perform hit test re-calculate forward vectors as needed.
    _hitResult = std::make_shared<VROHitTestResult>(hitTest(camera, origin, ray, true));
}

void VROInputControllerBase::onControllerStatus(int source, VROEventDelegate::ControllerStatus status){
    if (_currentControllerStatus == status){
        return;
    }

    _currentControllerStatus = status;

    // Notify internal delegates
    for (std::shared_ptr<VROEventDelegate> delegate : _delegates) {
        delegate->onControllerStatus(source, status);
    }

    // Return if we have not focused on any node upon which to trigger events.
    if (_hitResult == nullptr){
        return;
    }

    std::shared_ptr<VRONode> focusedNode
            = getNodeToHandleEvent(VROEventDelegate::EventAction::OnControllerStatus, _hitResult->getNode());
    if (focusedNode != nullptr){
        focusedNode->getEventDelegate()->onControllerStatus(source, status);
    }
}

void VROInputControllerBase::onSwipe(int source, VROEventDelegate::SwipeState swipeState) {
    // Notify internal delegates
    for (std::shared_ptr<VROEventDelegate> delegate : _delegates) {
        delegate->onSwipe(source, swipeState);
    }

    // Return if we have not focused on any node upon which to trigger events.
    if (_hitResult == nullptr){
        return;
    }

    std::shared_ptr<VRONode> focusedNode
            = getNodeToHandleEvent(VROEventDelegate::EventAction::OnSwipe, _hitResult->getNode());
    if (focusedNode != nullptr){
        focusedNode->getEventDelegate()->onSwipe(source, swipeState);
    }
}

void VROInputControllerBase::onScroll(int source, float x, float y) {
    // Notify internal delegates
    for (std::shared_ptr<VROEventDelegate> delegate : _delegates) {
        delegate->onScroll(source, x, y);
    }

    // Return if we have not focused on any node upon which to trigger events.
    if (_hitResult == nullptr){
        return;
    }

    std::shared_ptr<VRONode> focusedNode
            = getNodeToHandleEvent(VROEventDelegate::EventAction::OnScroll, _hitResult->getNode());
    if (focusedNode != nullptr){
        focusedNode->getEventDelegate()->onScroll(source, x, y);
    }
}

void VROInputControllerBase::processGazeEvent(int source, std::shared_ptr<VRONode> newNode) {
    if (_lastHoveredNode == newNode) {
        return;
    }

    VROVector3f hitLoc = _hitResult->getLocation();
    std::vector<float> pos = {hitLoc.x, hitLoc.y, hitLoc.z};
    if (_hitResult->isBackgroundHit()) {
        pos.clear();
    }
    
    if (newNode && newNode->getEventDelegate()) {
        std::shared_ptr<VROEventDelegate> delegate = newNode->getEventDelegate();
        if (delegate) {
            delegate->onHover(source, true, pos);
        }
    }

    if (_lastHoveredNode && _lastHoveredNode->getEventDelegate()) {
        std::shared_ptr<VROEventDelegate> delegate = _lastHoveredNode->getEventDelegate();
        if (delegate) {
            delegate->onHover(source, false, pos);
        }
    }

    _lastHoveredNode = newNode;
}

void VROInputControllerBase::processOnFuseEvent(int source, std::shared_ptr<VRONode> newNode) {
    std::shared_ptr<VRONode> focusedNode = getNodeToHandleEvent(VROEventDelegate::EventAction::OnFuse, newNode);
    if (_currentFusedNode != focusedNode){
        notifyOnFuseEvent(source, kOnFuseReset);
        _fuseTriggerAtMillis = kOnFuseReset;
        _haveNotifiedOnFuseTriggered = false;
        _currentFusedNode = focusedNode;
    }

    // Do nothing if no onFuse node is found
    if (!focusedNode || !_currentFusedNode->getEventDelegate()){
        return;
    }

    if (_fuseTriggerAtMillis == kOnFuseReset){
        _fuseTriggerAtMillis
                = VROTimeCurrentMillis() + _currentFusedNode->getEventDelegate()->getTimeToFuse();
    }

    // Compare the fuse time with the current time to get the timeToFuseRatio and notify delegates.
    // When the timeToFuseRatio counts down to 0, it is an indication that the node has been "onFused".
    if (!_haveNotifiedOnFuseTriggered){
        float delta = _fuseTriggerAtMillis - VROTimeCurrentMillis();
        float timeToFuseRatio = delta / _currentFusedNode->getEventDelegate()->getTimeToFuse();

        if (timeToFuseRatio <= 0.0f){
            timeToFuseRatio = 0.0f;
            _haveNotifiedOnFuseTriggered = true;
        }

        notifyOnFuseEvent(source, timeToFuseRatio);
    }
}

void VROInputControllerBase::notifyOnFuseEvent(int source, float timeToFuseRatio) {
    for (std::shared_ptr<VROEventDelegate> delegate : _delegates) {
        delegate->onFuse(source, timeToFuseRatio);
    }

    if (_currentFusedNode && _currentFusedNode->getEventDelegate()){
        _currentFusedNode->getEventDelegate()->onFuse(source, timeToFuseRatio);
    }
}

VROHitTestResult VROInputControllerBase::hitTest(const VROCamera &camera, VROVector3f origin, VROVector3f ray, bool boundsOnly) {
    std::vector<VROHitTestResult> results;
    std::shared_ptr<VROPortal> sceneRootNode = _scene->getRootNode();

    // Grab all the nodes that were hit
    std::vector<VROHitTestResult> nodeResults = sceneRootNode->hitTest(camera, origin, ray, boundsOnly);
    results.insert(results.end(), nodeResults.begin(), nodeResults.end());

    // Sort and get the closest node
    std::sort(results.begin(), results.end(), [](VROHitTestResult a, VROHitTestResult b) {
        return a.getDistance() < b.getDistance();
    });

    // Return the closest hit element, if any.
    for (int i = 0; i < results.size(); i ++) {
        if (!results[i].getNode()->getIgnoreEventHandling()) {
            return results[i];
        }
    }
    
    VROVector3f backgroundPosition = origin + (ray * kSceneBackgroundDistance);
    VROHitTestResult sceneBackgroundHitResult = { sceneRootNode, backgroundPosition,
                                                  kSceneBackgroundDistance, true, camera };
    return sceneBackgroundHitResult;
}

std::shared_ptr<VRONode> VROInputControllerBase::getNodeToHandleEvent(VROEventDelegate::EventAction action,
                                                                      std::shared_ptr<VRONode> node){
    // Base condition, we are asking for the scene's root node's parent, return.
    if (node == nullptr) {
        return nullptr;
    }

    std::shared_ptr<VROEventDelegate> delegate = node->getEventDelegate();
    if (delegate != nullptr && delegate->isEventEnabled(action)){
        return node;
    } else {
        return getNodeToHandleEvent(action, node->getParentNode());
    }
}
