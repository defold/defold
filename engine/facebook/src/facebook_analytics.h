#ifndef H_FACEBOOK_ANALYTICS
#define H_FACEBOOK_ANALYTICS

extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>
}

namespace dmFacebook {

    namespace Analytics {

        /**
         * \brief The number of predefined events from Facebook.
         *
         * \see https://developers.facebook.com/docs/app-events
         * \see https://notendur.hi.is/hap14/projx/com/facebook/AppEventsConstants.html
         */
        const unsigned int EVENTS = 17;

        /**
         * \brief The number of predefined parameters from Facebook.
         *
         * \see https://developers.facebook.com/docs/app-events
         * \see https://notendur.hi.is/hap14/projx/com/facebook/AppEventsConstants.html
         **/
        const unsigned int PARAMS = 12;

        /**
         * \brief The maximum number of parameters that can be sent with an event.
         *
         * \see https://developers.facebook.com/docs/app-events
         */
        const unsigned int MAX_PARAMS = 25;

        /**
         * \brief The internal representation of the predefined events for
         * Facebook Analytics.
         *
         * \note This list has to match the list in
         * facebook_analytics_android.cpp
         */
        enum Event {
            ACHIEVED_LEVEL          = 0,
            ACTIVATED_APP           = 1,
            ADDED_PAYMENT_INFO      = 2,
            ADDED_TO_CART           = 3,
            ADDED_TO_WISHLIST       = 4,
            COMPLETED_REGISTRATION  = 5,
            COMPLETED_TUTORIAL      = 6,
            DEACTIVATED_APP         = 7,
            INITIATED_CHECKOUT      = 8,
            PURCHASED               = 9,
            RATED                   = 10,
            SEARCHED                = 11,
            SESSION_INTERRUPTIONS   = 12,
            SPENT_CREDITS           = 13,
            TIME_BETWEEN_SESSIONS   = 14,
            UNLOCKED_ACHIEVEMENT    = 15,
            VIEWED_CONTENT          = 16
        };

        /**
         * \brief Get the Android SDK string representing the event index.
         *
         * \param index The index that should be looked up.
         *
         * \return The Android SDK string for the event, or 0 iff the index is
         * greater than the number of events (dm::Facebook::Analytics::EVENTS).
         */
        const char* lookupEvent(unsigned int index);

        /**
         * \brief Get the event identifier (Android SDK string or custom) based
         * on the first argument on the LUA stack provided.
         *
         * \param L The LUA stack.
         * \param index The position on the stack where the event can be found.
         *
         * \exception If the event is neither represented as a string or a
         * number an argerror will be thrown. If the event is represented as a
         * number that doesn't exist an argerror will be thrown. If the event
         * is represented as a string that is empty an argerror will be thrown.
         *
         * \return An event identifier to be used with Facebook Analytics.
         */
        const char* getEvent(lua_State* L, int index);

        /**
         * \brief Get the values from a table on the LUA stack at the position
         * specified by index and populate the arrays keys and values with the
         * table content.
         *
         * \param L The LUA stack.
         * \param index The position on the stack where the table can be found.
         * \param keys An array that should be populated with keys from the
         * table.
         * \param values An array that should be populated with the values from
         * the table.
         * \param length The length of the arrays keys and values. This
         * parameter will be populated with the number of items that were put
         * into the arrays keys and values.
         */
        void getTable(lua_State* L, int index, const char** keys,
            const char** values, unsigned int* length);

        /**
         * \brief Push constants for predefined events and predefined paramters
         * onto the stack L.
         *
         * \param L The LUA stack.
         */
        void registerConstants(lua_State* L);

    };

};

const char* CStringArrayToJson(const char** array, unsigned int length);

#endif