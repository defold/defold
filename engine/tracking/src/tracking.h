#ifndef __TRACKING_H__
#define __TRACKING_H__

#include <dlib/configfile.h>
#include <dlib/message.h>

namespace dmTracking
{
    typedef struct Context* HContext;

    /**
     * Create a new tracking context
     * @param config_file config file to read app version and tracking setup from
     * @return new tracking context
     */
    HContext New(dmConfigFile::HConfig config_file);

    /**
     * Start up and configure the tracking script. In case it fails it will start up
     * in a disabled mode.
     * @param context context
     * @param saves_app_name app named passed to sys.get_save_file
     * @param defold_version version to supply to the backend
     */
    void Start(HContext context, const char* saves_app_name, const char* defold_version);

    /**
     * Update tracking context. This triggers sends and internal handling of posted
     * events.
     * @param context context
     * @param dt time delta since last call
     */
    void Update(HContext context, float dt);

    /**
     * Finalize the system. Not necessary but gives the tracking system a chance to
     * flush out any pending saves. No more Update calls are allowed after this.
     * @param context context
     */
    void Finalize(HContext context);

    /**
     * Destroy and free all resources.
     * @param context context
     */
    void Delete(HContext context);

    /**
     * Helper to post a simple event to the tracking socket.
     * @param context context
     * @param type type
     */
    void PostSimpleEvent(HContext context, const char *type);

    /**
     * Get socket associated with a context.
     * @param context context
     */
    dmMessage::HSocket GetSocket(HContext context);
}

#endif
