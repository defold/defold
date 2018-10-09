#include "facebook_analytics.h"

#include <string.h>
#include <cstring>
#include <cstdlib>

#include <dlib/log.h>
#include "facebook_private.h"

namespace
{
    const char* EVENT_TABLE[dmFacebook::Analytics::MAX_NUM_EVENTS] = {
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

    const char* PARAMETER_TABLE[dmFacebook::Analytics::MAX_NUM_PARAMS] = {
        "fb_content_id",
        "fb_content_type",
        "fb_currency",
        "fb_description",
        "fb_level",
        "fb_max_rating_value",
        "fb_num_items",
        "fb_payment_info_available",
        "fb_registration_method",
        "fb_search_string",
        "fb_mobile_launch_source",
        "fb_success"
    };

    const char* LookupEvent(unsigned int index)
    {
        if (index < dmFacebook::Analytics::MAX_NUM_EVENTS) {
            return ::EVENT_TABLE[index];
        }

        return 0;
    }

    const char* LookupParameter(unsigned int index)
    {
        if (index < dmFacebook::Analytics::MAX_NUM_PARAMS)
        {
            return ::PARAMETER_TABLE[index];
        }

        return 0;
    }
};

const char* dmFacebook::Analytics::GetEvent(lua_State* L, int index)
{
    const char* event = 0;
    if (lua_isnil(L, index))
    {
        luaL_argerror(L, index, "Facebook Analytics event cannot be nil");
    }
    else if (lua_isnumber(L, index))
    {
        unsigned int event_number = (unsigned int) luaL_checknumber(L, index);
        event = ::LookupEvent(event_number);
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
        luaL_argerror(L, index,
            "Facebook Analytics event must be number or string");
    }

    return event;
}

const char* dmFacebook::Analytics::GetParameter(lua_State* L, int index, int tableIndex)
{
    const char* parameter = 0;
    if (lua_isnil(L, index))
    {
        luaL_argerror(L, tableIndex, "Facebook Analytics parameter cannot be nil");
    }
    else if (lua_isnumber(L, index))
    {
        unsigned int parameter_number =
            (unsigned int) luaL_checknumber(L, index);
        parameter = ::LookupParameter(parameter_number);
        if (parameter == 0)
        {
            luaL_argerror(L, tableIndex,
                "Facebook Analytics parameter does not exist");
        }
    }
    else if (lua_isstring(L, index))
    {
        size_t len = 0;
        parameter = luaL_checklstring(L, index, &len);
        if (len == 0)
        {
            luaL_argerror(L, tableIndex,
                "Facebook Analytics parameter cannot be empty");
        }
    }
    else
    {
        luaL_argerror(L, tableIndex,
            "Facebook Analytics parameter must be number or string");
    }

    return parameter;
}

void dmFacebook::Analytics::GetParameterTable(lua_State* L, int index, const char** keys,
    const char** values, unsigned int* length)
{
    lua_pushvalue(L, index);
    lua_pushnil(L);

    unsigned int position = 0;
    while (lua_next(L, -2) && position < (*length))
    {
        lua_pushvalue(L, -2);
        keys[position] = dmFacebook::Analytics::GetParameter(L, -1, index);
        values[position] = lua_tostring(L, -2);
        lua_pop(L, 2);

        if (keys[position] == 0x0)
        {
            dmLogError("Unsupported parameter type for key, must be string or number.");
        }
        else if (values[position] == 0x0)
        {
            dmLogError("Unsupported parameter value type for key '%s', value must be string or number.", keys[position]);
        }
        else
        {
            ++position;
        }
    }

    lua_pop(L, 1);
    *length = position;
}

void dmFacebook::Analytics::RegisterConstants(lua_State* L)
{
    // Add constants to table LIB_NAME
    lua_getglobal(L, LIB_NAME);

    #define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); lua_setfield(L, -2, #name);

    SETCONSTANT(EVENT_ACHIEVED_LEVEL,           dmFacebook::Analytics::ACHIEVED_LEVEL);
    // SETCONSTANT(EVENT_ACTIVATED_APP,         dmFacebook::Analytics::ACTIVATED_APP);
    SETCONSTANT(EVENT_ADDED_PAYMENT_INFO,       dmFacebook::Analytics::ADDED_PAYMENT_INFO);
    SETCONSTANT(EVENT_ADDED_TO_CART,            dmFacebook::Analytics::ADDED_TO_CART);
    SETCONSTANT(EVENT_ADDED_TO_WISHLIST,        dmFacebook::Analytics::ADDED_TO_WISHLIST);
    SETCONSTANT(EVENT_COMPLETED_REGISTRATION,   dmFacebook::Analytics::COMPLETED_REGISTRATION);
    SETCONSTANT(EVENT_COMPLETED_TUTORIAL,       dmFacebook::Analytics::COMPLETED_TUTORIAL);
    // SETCONSTANT(EVENT_DEACTIVATED_APP,       dmFacebook::Analytics::DEACTIVATED_APP);
    SETCONSTANT(EVENT_INITIATED_CHECKOUT,       dmFacebook::Analytics::INITIATED_CHECKOUT);
    SETCONSTANT(EVENT_PURCHASED,                dmFacebook::Analytics::PURCHASED);
    SETCONSTANT(EVENT_RATED,                    dmFacebook::Analytics::RATED);
    SETCONSTANT(EVENT_SEARCHED,                 dmFacebook::Analytics::SEARCHED);
    // SETCONSTANT(EVENT_SESSION_INTERRUPTIONS, dmFacebook::Analytics::SESSION_INTERRUPTIONS);
    SETCONSTANT(EVENT_SPENT_CREDITS,            dmFacebook::Analytics::SPENT_CREDITS);
    SETCONSTANT(EVENT_TIME_BETWEEN_SESSIONS,    dmFacebook::Analytics::TIME_BETWEEN_SESSIONS);
    SETCONSTANT(EVENT_UNLOCKED_ACHIEVEMENT,     dmFacebook::Analytics::UNLOCKED_ACHIEVEMENT);
    SETCONSTANT(EVENT_VIEWED_CONTENT,           dmFacebook::Analytics::VIEWED_CONTENT);

    SETCONSTANT(PARAM_CONTENT_ID,               dmFacebook::Analytics::CONTENT_ID);
    SETCONSTANT(PARAM_CONTENT_TYPE,             dmFacebook::Analytics::CONTENT_TYPE);
    SETCONSTANT(PARAM_CURRENCY,                 dmFacebook::Analytics::CURRENCY);
    SETCONSTANT(PARAM_DESCRIPTION,              dmFacebook::Analytics::DESCRIPTION);
    SETCONSTANT(PARAM_LEVEL,                    dmFacebook::Analytics::LEVEL);
    SETCONSTANT(PARAM_MAX_RATING_VALUE,         dmFacebook::Analytics::MAX_RATING_VALUE);
    SETCONSTANT(PARAM_NUM_ITEMS,                dmFacebook::Analytics::NUM_ITEMS);
    SETCONSTANT(PARAM_PAYMENT_INFO_AVAILABLE,   dmFacebook::Analytics::PAYMENT_INFO_AVAILABLE);
    SETCONSTANT(PARAM_REGISTRATION_METHOD,      dmFacebook::Analytics::REGISTRATION_METHOD);
    SETCONSTANT(PARAM_SEARCH_STRING,            dmFacebook::Analytics::SEARCH_STRING);
    SETCONSTANT(PARAM_SOURCE_APPLICATION,       dmFacebook::Analytics::SOURCE_APPLICATION);
    SETCONSTANT(PARAM_SUCCESS,                  dmFacebook::Analytics::SUCCESS);

    #undef SETCONSTANT

    // Pop table LIB_NAME
    lua_pop(L, 1);

}
