#include "facebook_analytics.h"

#include <string.h>

#include <dlib/log.h>
#include "facebook.h"

namespace
{
    const char* eventTable[dmFacebook::Analytics::EVENTS] = {
        "fb_mobile_level_achieved",
        "fb_mobile_activate_app",
        "fb_mobile_add_payment_info",
        "fb_mobile_add_to_cart",
        "fb_mobile_add_to_wishlist",
        "fb_mobile_complete_registration",
        "fb_mobile_tutorial_completion",
        "fb_mobile_deactivate_app",
        "fb_mobile_initiated_checkout",
        "fb_mobile_purchase",
        "fb_mobile_rate",
        "fb_mobile_search",
        "fb_mobile_app_interruptions",
        "fb_mobile_spent_credits",
        "fb_mobile_time_between_sessions",
        "fb_mobile_achievement_unlocked",
        "fb_mobile_content_view"
    };
};

const char* dmFacebook::Analytics::lookupEvent(unsigned int index)
{
    if (index < dmFacebook::Analytics::EVENTS) {
        return ::eventTable[index];
    }

    return 0;
}

const char* dmFacebook::Analytics::getEvent(lua_State* L, int index)
{
    const char* event = 0;
    if (lua_isnumber(L, index))
    {
        unsigned int eventNumber = (unsigned int) luaL_checknumber(L, index);
        event = dmFacebook::Analytics::lookupEvent(eventNumber);
        if (event == 0)
        {
            luaL_argerror(L, index, "Facebook Analytics event does not exist");
        }
    }
    else if (lua_isstring(L, index))
    {
        size_t len = 0;
        event = luaL_checklstring(L, index, &len);
        if (len == 0)
        {
            luaL_argerror(L, index, "Facebook Analytics event cannot be empty");
        }
    }
    else
    {
        luaL_argerror(L, index, "Facebook Analytics event must be number or string");
    }

    return event;
}

void dmFacebook::Analytics::getTable(lua_State* L, int index, const char** keys,
    const char** values, unsigned int* length)
{
    lua_pushvalue(L, index);
    lua_pushnil(L);

    unsigned int position = 0;
    while (lua_next(L, -2) && position < (*length))
    {
        lua_pushvalue(L, -2);
        keys[position] = lua_tostring(L, -1);
        values[position] = lua_tostring(L, -2);
        lua_pop(L, 2);

        ++position;
    }

    lua_pop(L, 1);
    *length = position;
}

void dmFacebook::Analytics::registerConstants(lua_State* L)
{
    // Add constants to table LIB_NAME
    lua_getglobal(L, LIB_NAME);

    #define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); lua_setfield(L, -2, #name);

    SETCONSTANT(EVENT_ACHIEVED_LEVEL,           dmFacebook::Analytics::ACHIEVED_LEVEL);
    SETCONSTANT(EVENT_ACTIVATED_APP,            dmFacebook::Analytics::ACTIVATED_APP);
    SETCONSTANT(EVENT_ADDED_PAYMENT_INFO,       dmFacebook::Analytics::ADDED_PAYMENT_INFO);
    SETCONSTANT(EVENT_ADDED_TO_CART,            dmFacebook::Analytics::ADDED_TO_CART);
    SETCONSTANT(EVENT_ADDED_TO_WISHLIST,        dmFacebook::Analytics::ADDED_TO_WISHLIST);
    SETCONSTANT(EVENT_COMPLETED_REGISTRATION,   dmFacebook::Analytics::COMPLETED_REGISTRATION);
    SETCONSTANT(EVENT_COMPLETED_TUTORIAL,       dmFacebook::Analytics::COMPLETED_TUTORIAL);
    SETCONSTANT(EVENT_DEACTIVATED_APP,          dmFacebook::Analytics::DEACTIVATED_APP);
    SETCONSTANT(EVENT_INITIATED_CHECKOUT,       dmFacebook::Analytics::INITIATED_CHECKOUT);
    SETCONSTANT(EVENT_PURCHASED,                dmFacebook::Analytics::PURCHASED);
    SETCONSTANT(EVENT_RATED,                    dmFacebook::Analytics::RATED);
    SETCONSTANT(EVENT_SEARCHED,                 dmFacebook::Analytics::SEARCHED);
    SETCONSTANT(EVENT_SESSION_INTERRUPTIONS,    dmFacebook::Analytics::SESSION_INTERRUPTIONS);
    SETCONSTANT(EVENT_SPENT_CREDITS,            dmFacebook::Analytics::SPENT_CREDITS);
    SETCONSTANT(EVENT_TIME_BETWEEN_SESSIONS,    dmFacebook::Analytics::TIME_BETWEEN_SESSIONS);
    SETCONSTANT(EVENT_UNLOCKED_ACHIEVEMENT,     dmFacebook::Analytics::UNLOCKED_ACHIEVEMENT);
    SETCONSTANT(EVENT_VIEWED_CONTENT,           dmFacebook::Analytics::VIEWED_CONTENT);

    #undef SETCONSTANT
    #define SETCONSTANT(name, val) \
        lua_pushstring(L, val); lua_setfield(L, -2, #name)

    SETCONSTANT(PARAM_CONTENT_ID,               "fb_content_id");
    SETCONSTANT(PARAM_CONTENT_TYPE,             "fb_content_type");
    SETCONSTANT(PARAM_CURRENCY,                 "fb_currency");
    SETCONSTANT(PARAM_DESCRIPTION,              "fb_description");
    SETCONSTANT(PARAM_LEVEL,                    "fb_level");
    SETCONSTANT(PARAM_MAX_RATING_VALUE,         "fb_max_rating_value");
    SETCONSTANT(PARAM_NUM_ITEMS,                "fb_num_items");
    SETCONSTANT(PARAM_PAYMENT_INFO_AVAILABLE,   "fb_payment_info_available");
    SETCONSTANT(PARAM_REGISTRATION_METHOD,      "fb_registration_method");
    SETCONSTANT(PARAM_SEARCH_STRING,            "fb_search_string");
    SETCONSTANT(PARAM_SOURCE_APPLICATION,       "fb_mobile_launch_source");
    SETCONSTANT(PARAM_SUCCESS,                  "fb_success");

    #undef SETCONSTANT

    // Pop table LIB_NAME
    lua_pop(L, 1);

}