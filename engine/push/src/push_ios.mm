#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/json.h>
#include <extension/extension.h>
#include <script/script.h>
#include "push_utils.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#define LIB_NAME "push"

struct Push;

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

struct SchedueledNotification
{

    int id;
    UILocalNotification* notification;
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
        m_ScheduledNotifications.SetCapacity(32);
    }

    lua_State*           m_L;
    int                  m_Callback;
    int                  m_Self;
    id<UIApplicationDelegate> m_AppDelegate;
    PushListener         m_Listener;
    NSDictionary*        m_SavedNotification;

    dmArray<SchedueledNotification> m_ScheduledNotifications;
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

    int ret = lua_pcall(L, 3, LUA_MULTRET, 0);
    if (ret != 0) {
        dmLogError("Error running push callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }

    g_Push.m_L = 0;
    g_Push.m_Callback = LUA_NOREF;
    g_Push.m_Self = LUA_NOREF;

    assert(top == lua_gettop(L));
}

static void ToLua(lua_State*L, id obj)
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
            ToLua(L, (NSDictionary*) value);
            lua_rawset(L, -3);
        }
    } else if ([obj isKindOfClass:[NSArray class]]) {
        NSArray* a = (NSArray*) obj;
        lua_createtable(L, [a count], 0);
        for (int i = 0; i < [a count]; ++i) {
            ToLua(L, [a objectAtIndex: i]);
            lua_rawseti(L, -2, i+1);
        }
    } else {
        dmLogWarning("Unsupported value '%s' (%s)", [[obj description] UTF8String], [[[obj class] description] UTF8String]);
        lua_pushnil(L);
    }
}

static void RunListener(NSDictionary *userdata, bool local)
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
            NSString* json = (NSString*)[userdata objectForKey:@"userdata"];
            dmJson::Result r = dmJson::Parse([json UTF8String], &doc);
            if (r == dmJson::RESULT_OK && doc.m_NodeCount > 0) {
                JsonToLua(L, &doc, 0);
            } else {
                dmLogError("Failed to parse local push response (%d)", r);
            }
            dmJson::Free(&doc);

            lua_pushnumber(L, 1);

        } else {
            ToLua(L, userdata);
            lua_pushnumber(L, 0);
        }

        int ret = lua_pcall(L, 3, LUA_MULTRET, 0);
        if (ret != 0) {
            dmLogError("Error running push callback: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }
        assert(top == lua_gettop(L));
    }
}

@interface PushAppDelegate : NSObject <UIApplicationDelegate>

@end

@implementation PushAppDelegate

- (void)application:(UIApplication *)application didReceiveRemoteNotification:(NSDictionary *)userInfo {
    if (g_Push.m_Listener.m_Callback == LUA_NOREF) {
        if (g_Push.m_SavedNotification) {
            dmLogWarning("No push listener set. Message discarded.");
        } else {
            // Save notification as push.set_listener may not be set at this point, e.g. when launching the app
            // buy clicking on the notification
            // TODO: Test this functionality. Not possible to test with dev-app as m_SavedNotification is cleared on reboot
            g_Push.m_SavedNotification = [[NSDictionary alloc] initWithDictionary:userInfo copyItems:YES];
        }
        return;
    }

    RunListener(userInfo, false);
}

- (void)application:(UIApplication *)application didReceiveLocalNotification:(UILocalNotification *)notification {
    RunListener(notification.userInfo, true);
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    UILocalNotification *localNotification = [launchOptions objectForKey:UIApplicationLaunchOptionsLocalNotificationKey];
    if (localNotification) {
        RunListener(localNotification.userInfo, true);
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
    RunCallback(g_Push.m_L, g_Push.m_Callback, g_Push.m_Self, 0, error);
}

@end

/*# Register for push notifications
 *
 * @name push.register
 * @examples
 * <p>
 * Convert device-token to string
 * </p>
 * <pre>
 * -- NOTE: %02x to pad byte with leading zero
 * local s = ""
 *     for i = 1,#token do
 *         s = s .. string.format("%02x", string.byte(token, i))
 * end
 * </pre>
 */
int Push_Register(lua_State* L)
{
    int top = lua_gettop(L);
    if (g_Push.m_Callback != LUA_NOREF) {
        dmLogError("Unexpected push callback set");
        luaL_unref(L, LUA_REGISTRYINDEX, g_Push.m_Callback);
        luaL_unref(L, LUA_REGISTRYINDEX, g_Push.m_Self);
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
    g_Push.m_Callback = luaL_ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_Push.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

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

    } else {
        [[UIApplication sharedApplication] registerForRemoteNotificationTypes: types];
    }


    assert(top == lua_gettop(L));
    return 0;
}

/*# set push listener
 *
 * The listener callback has the following signature: function(self, message, error_msg, origin) where message is a table
 * with the push payload.
 * @name push.set_listener
 * @param listener listener function
 */
int Push_SetListener(lua_State* L)
{
    Push* push = &g_Push;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = luaL_ref(L, LUA_REGISTRYINDEX);

    if (push->m_Listener.m_Callback != LUA_NOREF) {
        luaL_unref(push->m_Listener.m_L, LUA_REGISTRYINDEX, push->m_Listener.m_Callback);
        luaL_unref(push->m_Listener.m_L, LUA_REGISTRYINDEX, push->m_Listener.m_Self);
    }

    push->m_Listener.m_L = dmScript::GetMainThread(L);
    push->m_Listener.m_Callback = cb;

    dmScript::GetInstance(L);
    push->m_Listener.m_Self = luaL_ref(L, LUA_REGISTRYINDEX);

    if (g_Push.m_SavedNotification) {
        [g_Push.m_AppDelegate application: [UIApplication sharedApplication] didReceiveRemoteNotification: g_Push.m_SavedNotification];
        [g_Push.m_SavedNotification release];
        g_Push.m_SavedNotification = 0;
    }
    return 0;
}

/*# set badge icon count
 *
 * Set the badge count for application icon on iOS.
 * NOTE: Only available on iOS
 * @name push.set_badge_count
 * @param count badge count
 */
int Push_SetBadgeCount(lua_State* L)
{
    int count = luaL_checkinteger(L, 1);
    [UIApplication sharedApplication].applicationIconBadgeNumber = count;
    return 0;
}

/*# Schedule a local push notification to be triggered at a specific time in the future
 *
 * @name push.schedule
 * @param time Number of seconds, into the future until the notification should be triggered
 * @param title Localized title to be displayed to the user if the application is not running.
 * @param message Localized message to be displayed to the user if the application is not running.
 * @param userdata JSON string to be passed to the registered listener function
 * @param notification_settings Table with notification and platform specific data
 * @return id Unique id that can be used to cancel the notification
 * @return err Error string if something went wrong, otherwise nil
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

    const char* title = luaL_checkstring(L, 2);
    const char* message = luaL_checkstring(L, 3);

    // param: userdata
    NSDictionary* userdata = nil;
    if (top > 3) {
        userdata = @{@"userdata": [NSString stringWithUTF8String:luaL_checkstring(L, 4)]};
    }

    UILocalNotification *notification = [[UILocalNotification alloc] init];
    if (notification == nil)
    {
        lua_pushnil(L);
        lua_pushstring(L, "could not allocate local notification");
        return 2;
    }

    notification.fireDate   = [NSDate dateWithTimeIntervalSinceNow:seconds];
    notification.timeZone   = [NSTimeZone defaultTimeZone];
    notification.alertTitle = [NSString stringWithUTF8String:title];
    notification.alertBody  = [NSString stringWithUTF8String:message];
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

        // badge_number
        lua_pushstring(L, "badge_number");
        lua_gettable(L, 5);
        if (lua_isnumber(L, -1)) {
            notification.applicationIconBadgeNumber = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

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
    SchedueledNotification sn;
    sn.id = g_Push.m_ScheduledNotifications.Size();
    sn.notification = notification;

    g_Push.m_ScheduledNotifications.Push(sn);
    lua_pushnumber(L, sn.id);
    return 1;
}

int Push_Cancel(lua_State* L)
{

    int cancel_id = luaL_checkinteger(L, 1);
    if (cancel_id >= 0 &&
        cancel_id < g_Push.m_ScheduledNotifications.Size() &&
        g_Push.m_ScheduledNotifications[cancel_id].id == cancel_id &&
        g_Push.m_ScheduledNotifications[cancel_id].notification != nil)
    {
        [[UIApplication sharedApplication] cancelLocalNotification:g_Push.m_ScheduledNotifications[cancel_id].notification];
        g_Push.m_ScheduledNotifications[cancel_id].notification = nil;

    }

    return 0;
}

static const luaL_reg Push_methods[] =
{
    {"register", Push_Register},
    {"set_listener", Push_SetListener},
    {"set_badge_count", Push_SetBadgeCount},

    // local
    {"schedule", Push_Schedule},
    {"cancel", Push_Cancel},
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

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizePush(dmExtension::Params* params)
{
    if (params->m_L == g_Push.m_Listener.m_L && g_Push.m_Listener.m_Callback != LUA_NOREF) {
        luaL_unref(g_Push.m_Listener.m_L, LUA_REGISTRYINDEX, g_Push.m_Listener.m_Callback);
        luaL_unref(g_Push.m_Listener.m_L, LUA_REGISTRYINDEX, g_Push.m_Listener.m_Self);
        g_Push.m_Listener.m_L = 0;
        g_Push.m_Listener.m_Callback = LUA_NOREF;
        g_Push.m_Listener.m_Self = LUA_NOREF;
    }

    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(PushExt, "Push", AppInitializePush, AppFinalizePush, InitializePush, FinalizePush)
