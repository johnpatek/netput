@0x95f270d1e351654f;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("netput::rpc");

interface Netput {
    connect @0(request :ConnectRequest) ->(response :ConnectResponse);
    push @1 (event: Event) -> ();
    disconnect @2 (request :DisconnectRequest) ->(response :DisconnectResponse);
}

struct ConnectRequest {
    userData @0 :Data;
}

struct ConnectResponse {
    message :union {
        sessionId @0 :Text;
        error @1 :Text;
    }
}

struct DisconnectRequest {
    sessionId @0 :Text;
}

struct DisconnectResponse {
    error @0 :Text;
}

struct Event {
    sessionId @0 :Text;
    info :union {
        mouseMotion @1 :MouseMotionEvent;
        mouseButton @2 :MouseButtonEvent;
        mouseWheel @3 :MouseWheelEvent;
        keyboard @4 :KeyboardEvent;
        window @5 :WindowEvent;
    }
}

enum InputState {
    released @0;
    pressed @1;
}

enum MouseButton {
    left @0;
    middle @1;
    right @2;
    x1 @3;
    x2 @4;
}

enum WindowEventType {
    shownType @0;
    hiddenType @1;
    exposedType @2;
    movedType @3;
    resizedType @4;
    minimizedType @5;
    maximizedType @6;
    restoredType @7;
    mouseEnterType @8;
    mouseLeaveType @9;
    focusGainedType @10;
    focusLostType @11;
}

struct MouseMotionEvent {
    timestamp @0 :UInt64;
    windowId @1 :UInt32;
    stateMask @2 :MouseStateMask;
    x @3 :Int32;
    y @4 :Int32;
    relativeX @5 :Int32;
    relativeY @6 :Int32;
    struct MouseStateMask {
        left @0 :InputState;
        middle @1 :InputState;
        right @2 :InputState;
        x1 @3 :InputState;
        x2 @4 :InputState;
    }
}

struct MouseButtonEvent {
    timestamp @0 :UInt64;
    windowId @1 :UInt32;
    button @2 :MouseButton;
    state @3 :InputState;
    double @4 :Bool;
    x @5 :Int32;
    y @6 :Int32;
}

struct MouseWheelEvent {
    timestamp @0 :UInt64;
    windowId @1 :UInt32;
    x @2 :Int32;
    y @3 :Int32;
    preciseX @4 :Float32;
    preciseY @5 :Float32;
}

struct KeyboardEvent {
    timestamp @0 :UInt64;
    windowId @1 :UInt32;
    state @2 :InputState;
    repeat @3 :Bool;
    keyCode @4 :UInt32;
}

struct WindowEvent {
    timestamp @0 :UInt64;
    windowId @1 :UInt32;
    type @2 :WindowEventType;
    arg1 @3 :Int32;
    arg2 @4 :Int32;
}