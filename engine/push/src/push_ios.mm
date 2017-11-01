#include "push_utils.h"

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/json.h>
#include <script/script.h>
#include <extension/extension.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#define LIB_NAME "push"

struct Push;

/*# Push notifications API documentation
 *
 * Functions and constants for interacting with local, as well as
 * Apple's and Google's push notification services. These API:s only exist on mobile platforms.
 * [icon:ios] [icon:android]
 *
 * @document
 * @name Push notifications
 * @namespace push
 */

struct PushListener
{
    PushListener()
    {
        Clear();
    }

    void Clear()
    {
        m_L = 0;
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }

    lua_State* m_L;
    int        m_Callback;
    int        m_Self;
};

struct Push
{
    Push()
    {
        Clear();
    }

    void Clear() {
        m_L = 0;
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
        m_AppDelegate = 0;
        m_Listener.Clear();
        if (m_SavedNotification) {
            [m_SavedNotification release];
        }
        m_SavedNotification = 0;
        m_SavedNotificationOrigin = DM_PUSH_EXTENSION_ORIGIN_LOCAL;
        m_SavedWasActivated = false;
        m_ScheduledID = -1;
    }

    lua_State*           m_L;
    int                  m_Callback;
    int                  m_Self;
    id<UIApplicationDelegate> m_AppDelegate;
    PushListener         m_Listener;
    NSDictionary*        m_SavedNotification;
    int                  m_SavedNotificationOrigin;
    bool                 m_SavedWasActivated;

    int m_ScheduledID;
};

Push g_Push;
static void PushError(lua_State*L, NSError* error)
{
    // Could be extended with error codes etc
    if (error != 0) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, [error.localizedDescription UTF8String]);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

static void UpdateScheduleIDCounter()
{
    for (id obj in [[UIApplication sharedApplication] scheduledLocalNotifications]) {
        UILocalNotification* notification = (UILocalNotification*)obj;
        int current_id = [(NSNumber*)notification.userInfo[@"id"] intValue];
        if (current_id > g_Push.m_ScheduledID)
        {
            g_Push.m_ScheduledID = current_id;
        }
    }

}

static void RunCallback(lua_State* L, int cb, int self, NSData* deviceToken, NSError* error)
{
    int top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, cb);

    // Setup self
    lua_rawgeti(L, LUA_REGISTRYINDEX, self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L)) {
        dmLogError("Could not run push callback because the instance has been deleted.");
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
        return;
    }

    if (deviceToken) {
        lua_pushlstring(L, (const char*) [deviceToken bytes], [deviceToken length]);
    } else {
        lua_pushnil(L);
    }

    if (error) {
        PushError(L, error);
    } else {
        lua_pushnil(L);
    }

    int ret = dmScript::PCall(L, 3, 0);
    if (ret != 0) {
        dmLogError("Error running push callback");
    }

    g_Push.m_L = 0;
    g_Push.m_Callback = LUA_NOREF;
    g_Push.m_Self = LUA_NOREF;

    assert(top == lua_gettop(L));
}

static void ObjCToLua(lua_State*L, id obj)
{
    if ([obj isKindOfClass:[NSString class]]) {
        const char* str = [((NSString*) obj) UTF8String];
        lua_pushstring(L, str);
    } else if ([obj isKindOfClass:[NSNumber class]]) {
        lua_pushnumber(L, [((NSNumber*) obj) doubleValue]);
    } else if ([obj isKindOfClass:[NSNull class]]) {
        lua_pushnil(L);
    } else if ([obj isKindOfClass:[NSDictionary class]]) {
        NSDictionary* dict = (NSDictionary*) obj;
        lua_createtable(L, 0, [dict count]);
        for (NSString* key in dict) {
            lua_pushstring(L, [key UTF8String]);
            id value = [dict objectForKey:key];
            ObjCToLua(L, (NSDictionary*) value);
            lua_rawset(L, -3);
        }
    } else if ([obj isKindOfClass:[NSArray class]]) {
        NSArray* a = (NSArray*) obj;
        lua_createtable(L, [a count], 0);
        for (int i = 0; i < [a count]; ++i) {
            ObjCToLua(L, [a objectAtIndex: i]);
            lua_rawseti(L, -2, i+1);
        }
    } else {
        dmLogWarning("Unsupported value '%s' (%s)", [[obj description] UTF8String], [[[obj class] description] UTF8String]);
        lua_pushnil(L);
    }
}

static void RunListener(NSDictionary *userdata, bool local, bool wasActivated)
{
    if (g_Push.m_Listener.m_Callback != LUA_NOREF)
    {
        lua_State* L = g_Push.m_Listener.m_L;
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Push.m_Listener.m_Callback);

        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_Push.m_Listener.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        // Local notifications are supplied as JSON
        if (local) {

            dmJson::Document doc;
            NSString* json = (NSString*)[userdata objectForKey:@"payload"];
            dmJson::Result r = dmJson::Parse([json UTF8String], &doc);
            if (r == dmJson::RESULT_OK && doc.m_NodeCount > 0) {
                dmScript::JsonToLua(L, &doc, 0);
            } else {
                dmLogError("Failed to parse local push response (%d)", r);
            }
            dmJson::Free(&doc);

            lua_pushnumber(L, DM_PUSH_EXTENSION_ORIGIN_LOCAL);

        } else {
            ObjCToLua(L, userdata);
            lua_pushnumber(L, DM_PUSH_EXTENSION_ORIGIN_REMOTE);
        }

        lua_pushboolean(L, wasActivated);

        int ret = dmScript::PCall(L, 4, 0);
        if (ret != 0) {
            dmLogError("Error running push callback");
        }
        assert(top == lua_gettop(L));
    } else {
        dmLogWarning("No push listener set, saving message.");

        if (g_Push.m_SavedNotification) {
            [g_Push.m_SavedNotification release];
        }

        // Save notification as push.set_listener may not be set at this point, e.g. when launching the app
        // but clicking on the notification
        g_Push.m_SavedNotification = [[NSDictionary alloc] initWithDictionary:userdata copyItems:YES];
        g_Push.m_SavedNotificationOrigin = (local ? DM_PUSH_EXTENSION_ORIGIN_LOCAL : DM_PUSH_EXTENSION_ORIGIN_REMOTE);
        g_Push.m_SavedWasActivated = wasActivated;
    }
}

@interface PushAppDelegate : NSObject <UIApplicationDelegate>

@end

@implementation PushAppDelegate

- (void)application:(UIApplication *)application didReceiveRemoteNotification:(NSDictionary *)userInfo {
    bool wasActivated = (application.applicationState == UIApplicationStateInactive
        || application.applicationState == UIApplicationStateBackground);

    RunListener(userInfo, false, wasActivated);
}

- (void)application:(UIApplication *)application didReceiveLocalNotification:(UILocalNotification *)notification {
    bool wasActivated = (application.applicationState == UIApplicationStateInactive
        || application.applicationState == UIApplicationStateBackground);

    RunListener(notification.userInfo, true, wasActivated);
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    UILocalNotification *localNotification = [launchOptions objectForKey:UIApplicationLaunchOptionsLocalNotificationKey];
    if (localNotification) {
        [self application:application didReceiveLocalNotification:localNotification];
    }

    NSDictionary *remoteNotification = [launchOptions objectForKey:UIApplicationLaunchOptionsRemoteNotificationKey];
    if (remoteNotification) {
        [self application:application didReceiveRemoteNotification:remoteNotification];
    }

    return YES;
}

- (void)application:(UIApplication *)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken {
    if (g_Push.m_Callback != LUA_NOREF) {
        RunCallback(g_Push.m_L, g_Push.m_Callback, g_Push.m_Self, deviceToken, 0);
    }
}

- (void)application:(UIApplication *)application didFailToRegisterForRemoteNotificationsWithError:(NSError *)error {
    dmLogWarning("Failed to register remote notifications: %s\n", [error.localizedDescription UTF8String]);
    if (g_Push.m_Callback != LUA_NOREF) {
        RunCallback(g_Push.m_L, g_Push.m_Callback, g_Push.m_Self, 0, error);
    }
}

@end

/*# Register for push notifications
 * Send a request for push notifications. Note that the notifications table parameter
 * is iOS only and will be ignored on Android.
 *
 * @name push.register
 * @param notifications [type:table] the types of notifications to listen to. [icon:iOS]
 * @param callback [type:function(self, token, error)] register callback function.
 *
 * self
 * :        [type:object] The current object.
 *
 * token
 * :        [type:string] The returned push token if registration is successful.
 *
 * error
 * :        [type:table] A table containing eventual error information.
 *
 * @examples
 *
 * Register for push notifications on iOS. Note that the token needs to be converted on this platform.
 *
 * ```lua
 * local function push_listener(self, payload, origin)
 *      -- The payload arrives here.
 * end
 *
 * function init(self)
 *      local alerts = {push.NOTIFICATION_BADGE, push.NOTIFICATION_SOUND, push.NOTIFICATION_ALERT}
 *      push.register(alerts, function (self, token, error)
 *      if token then
 *           -- NOTE: %02x to pad byte with leading zero
 *           local token_string = ""
 *           for i = 1,#token do
 *               token_string = token_string .. string.format("%02x", string.byte(token, i))
 *           end
 *           print(token_string)
 *           push.set_listener(push_listener)
 *      else
 *           -- Push registration failed.
 *           print(error.error)
 *      end
 * end
 * ```
 *
 * Register for push notifications on Android.
 *
 * ```lua
 * local function push_listener(self, payload, origin)
 *      -- The payload arrives here.
 * end
 *
 * function init(self)
 *      push.register({}, function (self, token, error)
 *          if token then
 *               print(token)
 *               push.set_listener(push_listener)
 *          else
 *               -- Push registration failed.
 *               print(error.error)
 *          end
 *     end)
 * end
 * ```
 */
int Push_Register(lua_State* L)
{
    int top = lua_gettop(L);
    if (g_Push.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected push callback set");
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Push.m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_Push.m_Self);
        g_Push.m_L = 0;
        g_Push.m_Callback = LUA_NOREF;
        g_Push.m_Self = LUA_NOREF;
    }

    if (!lua_istable(L, 1)) {
    assert(top == lua_gettop(L));
    luaL_error(L, "First argument must be a table of notification types.");
    return 0;
    }

    UIRemoteNotificationType types = UIRemoteNotificationTypeNone;
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        int t = luaL_checkinteger(L, -1);
        types |= (UIRemoteNotificationType) t;
        lua_pop(L, 1);
    }
    g_Push.m_L = dmScript::GetMainThread(L);

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    g_Push.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Push.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    // iOS 8 API
    if ([[UIApplication sharedApplication] respondsToSelector:@selector(registerUserNotificationSettings:)]) {
        UIUserNotificationType uitypes = UIUserNotificationTypeNone;

        if (types & UIRemoteNotificationTypeBadge)
            uitypes |= UIUserNotificationTypeBadge;

        if (types & UIRemoteNotificationTypeSound)
            uitypes |= UIUserNotificationTypeSound;

        if (types & UIRemoteNotificationTypeAlert)
            uitypes |= UIUserNotificationTypeAlert;

        [[UIApplication sharedApplication] registerUserNotificationSettings:[UIUserNotificationSettings settingsForTypes:uitypes categories:nil]];
        [[UIApplication sharedApplication] registerForRemoteNotifications];

    } else {
        [[UIApplication sharedApplication] registerForRemoteNotificationTypes: types];
    }


    assert(top == lua_gettop(L));
    return 0;
}

/*# set push listener
 *
 * Sets a listener function to listen to push notifications.
 *
 * @name push.set_listener
 * @param listener [type:function(self, payload, origin, activated)] listener callback function.
 * Pass an empty function if you no longer wish to receive callbacks.
 *
 * `self`
 * :    [type:object] The current object
 *
 * `payload`
 * :    [type:function] the push payload
 *
 * `origin`
 * :    [type:constant] push.ORIGIN_LOCAL or push.ORIGIN_REMOTE
 *
 * `activated`
 * :    [type:boolean] true or false depending on if the application was
 *      activated via the notification.
 *
 * @examples
 *
 * Set the push notification listener.
 *
 * ```lua
 * local function push_listener(self, payload, origin, activated)
 *      -- The payload arrives here.
 *      pprint(payload)
 *      if origin == push.ORIGIN_LOCAL then
 *          -- This was a local push
 *          ...
 *      end
 *
 *      if origin == push.ORIGIN_REMOTE then
 *          -- This was a remote push
 *          ...
 *      end
 * end
 *
 * local init(self)
 *      ...
 *      -- Assuming that push.register() has been successfully called earlier
 *      push.set_listener(push_listener)
 * end
 * ```
 */
int Push_SetListener(lua_State* L)
{
    Push* push = &g_Push;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

    if (push->m_Listener.m_Callback != LUA_NOREF) {
        dmScript::Unref(push->m_Listener.m_L, LUA_REGISTRYINDEX, push->m_Listener.m_Callback);
        dmScript::Unref(push->m_Listener.m_L, LUA_REGISTRYINDEX, push->m_Listener.m_Self);
    }

    push->m_Listener.m_L = dmScript::GetMainThread(L);
    push->m_Listener.m_Callback = cb;

    dmScript::GetInstance(L);
    push->m_Listener.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    if (g_Push.m_SavedNotification) {
        RunListener(g_Push.m_SavedNotification, g_Push.m_SavedNotificationOrigin == DM_PUSH_EXTENSION_ORIGIN_LOCAL, g_Push.m_SavedWasActivated);
        [g_Push.m_SavedNotification release];
        g_Push.m_SavedNotification = 0;
    }
    return 0;
}

/*# set badge icon count [icon:iOS]
 *
 * Set the badge count for application icon.
 * This function is only available on iOS. [icon:iOS]
 *
 * @name push.set_badge_count
 * @param count [type:number] badge count
 */
int Push_SetBadgeCount(lua_State* L)
{
    int count = luaL_checkinteger(L, 1);
    [UIApplication sharedApplication].applicationIconBadgeNumber = count;
    return 0;
}

/*# Schedule a local push notification to be triggered at a specific time in the future
 *
 * Local push notifications are scheduled with this function.
 * The returned `id` value is uniquely identifying the scheduled notification
 * and can be stored for later reference.
 *
 * @name push.schedule
 * @param time [type:number] number of seconds into the future until the notification should be triggered
 * @param title [type:string] localized title to be displayed to the user if the application is not running
 * @param alert [type:string] localized body message of the notification to be displayed to the user if the application is not running
 * @param payload [type:string] JSON string to be passed to the registered listener function
 * @param notification_settings [type:table] table with notification and platform specific fields
 *
 * action [icon:iOS]
 * :    [type:string]
 *      The alert action string to be used as the title of the right button of the
 *      alert or the value of the unlock slider, where the value replaces
 *      "unlock" in "slide to unlock" text.
 *
 * badge_count [icon:iOS]
 * :    [type:number] The numeric value of the icon badge.
 *
 * <s>badge_number</s>
 * :    Deprecated! Use badge_count instead
 *
 * priority [icon:android]
 * :    [type:number]
 *      The priority is a hint to the device UI about how the notification
 *      should be displayed. There are five priority levels, from -2 to 2 where -1 is the
 *      lowest priority and 2 the highest. Unless specified, a default priority level of 2
 *      is used.
 *
 * @return id [type:number] unique id that can be used to cancel or inspect the notification
 * @return err [type:string] error string if something went wrong, otherwise nil
 * @examples
 *
 * This example demonstrates how to schedule a local notification:
 *
 * ```lua
 * -- Schedule a local push in 3 seconds
 * local payload = '{ "data" : { "field" : "Some value", "field2" : "Other value" } }'
 * id, err = push.schedule(3, "Update!", "There are new stuff in the app", payload, { action = "check it out" })
 * if err then
 *      -- Something went wrong
 *      ...
 * end
 * ```
 *
 */
int Push_Schedule(lua_State* L)
{
    int top = lua_gettop(L);

    int seconds = luaL_checkinteger(L, 1);
    if (seconds < 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "invalid seconds argument");
        return 2;
    }

    NSString* title = [NSString stringWithUTF8String:luaL_checkstring(L, 2)];
    NSString* message = [NSString stringWithUTF8String:luaL_checkstring(L, 3)];

    if (g_Push.m_ScheduledID == -1)
    {
        g_Push.m_ScheduledID = 0;
        UpdateScheduleIDCounter();
    }

    // param: userdata
    NSMutableDictionary* userdata = [NSMutableDictionary dictionaryWithCapacity:2];
    userdata[@"id"] = [NSNumber numberWithInt:g_Push.m_ScheduledID];
    if (top > 3) {
        userdata[@"payload"] = [NSString stringWithUTF8String:luaL_checkstring(L, 4)];
    } else {
        userdata[@"payload"] = nil;
    }

    UILocalNotification* notification = [[UILocalNotification alloc] init];
    if (notification == nil)
    {
        lua_pushnil(L);
        lua_pushstring(L, "could not allocate local notification");
        return 2;
    }

    notification.fireDate   = [NSDate dateWithTimeIntervalSinceNow:seconds];
    notification.timeZone   = [NSTimeZone defaultTimeZone];
    if ([notification respondsToSelector:@selector(alertTitle)]) {
        [notification setValue:title forKey:@"alertTitle"];
    }
    notification.alertBody  = message;
    notification.soundName  = UILocalNotificationDefaultSoundName;
    notification.userInfo   = userdata;

    // param: notification_settings
    if (top > 4) {
        luaL_checktype(L, 5, LUA_TTABLE);

        // action
        lua_pushstring(L, "action");
        lua_gettable(L, 5);
        if (lua_isstring(L, -1)) {
            notification.alertAction = [NSString stringWithUTF8String:lua_tostring(L, -1)];
        }
        lua_pop(L, 1);

        // badge_count
        lua_pushstring(L, "badge_count");
        lua_gettable(L, 5);
        bool badge_count_set = false;
        if (lua_isnumber(L, -1)) {
            notification.applicationIconBadgeNumber = lua_tointeger(L, -1);
            badge_count_set = true;
        }
        lua_pop(L, 1);

        // Deprecated, replaced by badge_count
        if(!badge_count_set)
        {
            lua_pushstring(L, "badge_number");
            lua_gettable(L, 5);
            if (lua_isnumber(L, -1)) {
                notification.applicationIconBadgeNumber = lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
        }

        // sound
        /*

        There is now way of automatically bundle files inside the .app folder (i.e. skipping
        archiving them inside the .darc), but to have custom notification sounds they need to
        be accessable from the .app folder.

        lua_pushstring(L, "sound");
        lua_gettable(L, 5);
        if (lua_isstring(L, -1)) {
            notification.soundName = [NSString stringWithUTF8String:lua_tostring(L, -1)];
        }
        lua_pop(L, 1);
        */
    }

    [[UIApplication sharedApplication] scheduleLocalNotification:notification];

    assert(top == lua_gettop(L));

    // need to remember notification so it can be canceled later on
    lua_pushnumber(L, g_Push.m_ScheduledID++);

    return 1;
}

/*# Cancel a scheduled local push notification
 *
 * Use this function to cancel a previously scheduled local push notification. The
 * notification is identified by a numeric id as returned by `push.schedule()`.
 *
 * @name push.cancel
 * @param id [type:number] the numeric id of the local push notification
 */
int Push_Cancel(lua_State* L)
{
    int cancel_id = luaL_checkinteger(L, 1);
    for (id obj in [[UIApplication sharedApplication] scheduledLocalNotifications]) {
        UILocalNotification* notification = (UILocalNotification*)obj;

        if ([(NSNumber*)notification.userInfo[@"id"] intValue] == cancel_id)
        {
            [[UIApplication sharedApplication] cancelLocalNotification:notification];
            return 0;
        }
    }

    return 0;
}

static void NotificationToLua(lua_State* L, UILocalNotification* notification)
{
    lua_createtable(L, 0, 6);

    lua_pushstring(L, "seconds");
    lua_pushnumber(L, [[notification fireDate] timeIntervalSinceNow]);
    lua_settable(L, -3);

    lua_pushstring(L, "title");
    if ([notification respondsToSelector:@selector(alertTitle)]) {
        lua_pushstring(L, [[notification valueForKey:@"alertTitle"] UTF8String]);
    } else {
        lua_pushnil(L);
    }
    lua_settable(L, -3);

    lua_pushstring(L, "message");
    lua_pushstring(L, [[notification alertBody] UTF8String]);
    lua_settable(L, -3);

    lua_pushstring(L, "payload");
    NSString* payload = (NSString*)[[notification userInfo] objectForKey:@"payload"];
    lua_pushstring(L, [payload UTF8String]);
    lua_settable(L, -3);

    lua_pushstring(L, "action");
    lua_pushstring(L, [[notification alertAction] UTF8String]);
    lua_settable(L, -3);

    lua_pushstring(L, "badge_count");
    lua_pushnumber(L, [notification applicationIconBadgeNumber]);
    lua_settable(L, -3);

    // Deprecated
    lua_pushstring(L, "badge_number");
    lua_pushnumber(L, [notification applicationIconBadgeNumber]);
    lua_settable(L, -3);
}

/*# Retrieve data on a scheduled local push notification
 *
 * Returns a table with all data associated with a specified local push notification.
 * The notification is identified by a numeric id as returned by `push.schedule()`.
 *
 * @name push.get_scheduled
 * @param id [type:number] the numeric id of the local push notification
 * @return data [type:table] table with all data associated with the notification
 */
int Push_GetScheduled(lua_State* L)
{
    int get_id = luaL_checkinteger(L, 1);
    for (id obj in [[UIApplication sharedApplication] scheduledLocalNotifications]) {
        UILocalNotification* notification = (UILocalNotification*)obj;

        if ([(NSNumber*)notification.userInfo[@"id"] intValue] == get_id)
        {
            NotificationToLua(L, notification);
            return 1;
        }
    }
    return 0;
}

/*# Retrieve data on all scheduled local push notifications
 *
 * Returns a table with all data associated with all scheduled local push notifications.
 * The table contains key, value pairs where the key is the push notification id and the
 * value is a table with the notification data, corresponding to the data given by
 * `push.get_scheduled(id)`.
 *
 * @name push.get_all_scheduled
 * @return data [type:table] table with all data associated with all scheduled notifications
 */
int Push_GetAllScheduled(lua_State* L)
{
    lua_createtable(L, 0, 0);
    for (id obj in [[UIApplication sharedApplication] scheduledLocalNotifications]) {

        UILocalNotification* notification = (UILocalNotification*)obj;

        NSNumber* notification_id = (NSNumber*)[[notification userInfo] objectForKey:@"id"];
        lua_pushnumber(L, [notification_id intValue]);
        NotificationToLua(L, notification);
        lua_settable(L, -3);
    }

    return 1;
}

static const luaL_reg Push_methods[] =
{
    {"register", Push_Register},
    {"set_listener", Push_SetListener},
    {"set_badge_count", Push_SetBadgeCount},

    // local
    {"schedule", Push_Schedule},
    {"cancel", Push_Cancel},
    {"get_scheduled", Push_GetScheduled},
    {"get_all_scheduled", Push_GetAllScheduled},

    {0, 0}
};

/*# badge notification type
 *
 * @name push.NOTIFICATION_BADGE
 * @variable
 */

/*# sound notification type
 *
 * @name push.NOTIFICATION_SOUND
 * @variable
 */

/*# alert notification type
 *
 * @name push.NOTIFICATION_ALERT
 * @variable
 */

/*# local push origin
 *
 * @name push.ORIGIN_LOCAL
 * @variable
 */

/*# remote push origin
 *
 * @name push.ORIGIN_REMOTE
 * @variable
 */


/*# remote push origin
 *
 * @name push.ORIGIN_REMOTE
 * @variable
 */


/*# lowest notification priority [icon:android]
 *
 * This priority is for items might not be shown to the user except under special circumstances, such as detailed notification logs. Only available on Android. [icon:android]
 *
 * @name push.PRIORITY_MIN
 *
 * @variable
 */


/*# lower notification priority [icon:android]
 *
 * Priority for items that are less important. Only available on Android. [icon:android]
 *
 * @name push.PRIORITY_LOW
 *
 * @variable
 */

/*# default notification priority [icon:android]
 *
 * The default notification priority. Only available on Android. [icon:android]
 *
 * @name push.PRIORITY_DEFAULT
 *
 * @variable
 */

/*# higher notification priority [icon:android]
 *
 * Priority for more important notifications or alerts. Only available on Android. [icon:android]
 *
 * @name push.PRIORITY_HIGH
 *
 * @variable
 */

/*# highest notification priority [icon:android]
 *
 * Set this priority for your application's most important items that require the user's prompt attention or input. Only available on Android. [icon:android]
 *
 * @name push.PRIORITY_MAX
 *
 * @variable
 */


dmExtension::Result AppInitializePush(dmExtension::AppParams* params)
{
    g_Push.Clear();
    g_Push.m_AppDelegate = [[PushAppDelegate alloc] init];
    dmExtension::RegisterUIApplicationDelegate(g_Push.m_AppDelegate);
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizePush(dmExtension::AppParams* params)
{
    dmExtension::UnregisterUIApplicationDelegate(g_Push.m_AppDelegate);
    [g_Push.m_AppDelegate release];
    g_Push.m_AppDelegate = 0;
    g_Push.Clear();
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializePush(dmExtension::Params* params)
{
    lua_State*L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, Push_methods);

#define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(NOTIFICATION_BADGE, UIRemoteNotificationTypeBadge);
    SETCONSTANT(NOTIFICATION_SOUND, UIRemoteNotificationTypeSound);
    SETCONSTANT(NOTIFICATION_ALERT, UIRemoteNotificationTypeAlert);

    SETCONSTANT(ORIGIN_REMOTE, DM_PUSH_EXTENSION_ORIGIN_REMOTE);
    SETCONSTANT(ORIGIN_LOCAL,  DM_PUSH_EXTENSION_ORIGIN_LOCAL);

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizePush(dmExtension::Params* params)
{
    if (params->m_L == g_Push.m_Listener.m_L && g_Push.m_Listener.m_Callback != LUA_NOREF) {
        dmScript::Unref(g_Push.m_Listener.m_L, LUA_REGISTRYINDEX, g_Push.m_Listener.m_Callback);
        dmScript::Unref(g_Push.m_Listener.m_L, LUA_REGISTRYINDEX, g_Push.m_Listener.m_Self);
        g_Push.m_Listener.m_L = 0;
        g_Push.m_Listener.m_Callback = LUA_NOREF;
        g_Push.m_Listener.m_Self = LUA_NOREF;
    }

    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(PushExt, "Push", AppInitializePush, AppFinalizePush, InitializePush, 0, 0, FinalizePush)
