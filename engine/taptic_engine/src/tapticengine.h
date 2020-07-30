enum ImpactStyle
{
    IMPACT_LIGHT,
    IMPACT_MEDIUM,
    IMPACT_HEAVY,
};

enum NotificationType
{
    NOTIFICATION_SUCCESS,
    NOTIFICATION_WARNING,
    NOTIFICATION_ERROR,
};

extern bool TapticEngine_IsSupported();
extern void TapticEngine_Impact(ImpactStyle style);
extern void TapticEngine_Notification(NotificationType type);
extern void TapticEngine_Selection();