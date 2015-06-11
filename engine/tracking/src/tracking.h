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
     * @param saves_app_name App named passed to sys.get_save_file
     * @param defold_version Version to supply to the backend
     */
    void Start(HContext context, const char* saves_app_name, const char* defold_version);

    /**
     * Update tracking context. This triggers sends and internal handling of posted
     * events.
     * @param context contetx
     * @param dt time delta since last call
     */
    void Update(HContext context, float dt);

    /**
     * Finalize the system. Not necessary but gives the tracking system a chance to
     * flush out any pending saves. No more Update calls are allowed after this.
     * @param context contetx
     */
    void Finalize(HContext context);

    /**
     * Destroy and free all resources.
     * @param context contetx
     */
    void Delete(HContext context);

    /**
     * Helper to register a simple event. Internally it posts a message to the socket
     * @param context context
     * @param type type
     */
    void SimpleEvent(HContext context, const char *type);

    /**
     * Get socket associated with a context.
     * @param context contetx
     */
    dmMessage::HSocket GetSocket(HContext context);
}

#endif
