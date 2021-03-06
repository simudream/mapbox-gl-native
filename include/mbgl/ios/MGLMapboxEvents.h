#import <Foundation/Foundation.h>

extern NSString *const MGLEventTypeMapLoad;
extern NSString *const MGLEventTypeMapTap;
extern NSString *const MGLEventTypeMapDragEnd;
extern NSString *const MGLEventTypeLocation;

extern NSString *const MGLEventKeyLatitude;
extern NSString *const MGLEventKeyLongitude;
extern NSString *const MGLEventKeyZoomLevel;
extern NSString *const MGLEventKeyPushEnabled;
extern NSString *const MGLEventKeyEmailEnabled;
extern NSString *const MGLEventKeyGestureID;

extern NSString *const MGLEventGestureSingleTap;
extern NSString *const MGLEventGestureDoubleTap;
extern NSString *const MGLEventGestureTwoFingerSingleTap;
extern NSString *const MGLEventGestureQuickZoom;
extern NSString *const MGLEventGesturePanStart;
extern NSString *const MGLEventGesturePinchStart;
extern NSString *const MGLEventGestureRotateStart;

@interface MGLMapboxEvents : NSObject

// You must call these methods from the main thread.
//
+ (void) setToken:(NSString *)token;
+ (void) setAppName:(NSString *)appName;
+ (void) setAppVersion:(NSString *)appVersion;

// You can call this method from any thread. Significant work will
// be dispatched to a low-priority background queue and all
// resulting calls are guaranteed threadsafe.
//
// Events or attributes passed could be accessed on non-main threads,
// so you must not reference UI elements from within any arguments.
// Copy any values needed first or create dedicated methods in this
// class for threadsafe access to UIKit classes.
//
+ (void) pushEvent:(NSString *)event withAttributes:(NSDictionary *)attributeDictionary;

// You can call these methods from any thread.
//
+ (BOOL) checkPushEnabled;

// You can call this method from any thread.
//
+ (void) flush;

@end
