
# Most settings from proguard-android-optimize.txt from the Android SDK
# -optimizations !code/simplification/arithmetic,!code/simplification/cast,!field/*,!class/merging/*
# -optimizationpasses 5
# -allowaccessmodification
# -dontpreverify

# THe rest of the settings from proguard-android.txt
# -dontusemixedcaseclassnames
# -dontskipnonpubliclibraryclasses


# When debugging
-verbose

# ANDROID
# https://www.jon-douglas.com/2016/11/22/xamarin-android-proguard/

-dontpreverify
-repackageclasses ''
-allowaccessmodification
-optimizations !code/simplification/arithmetic

# Keep a fixed source file attribute and all line number tables to get line
# numbers in the stack traces.
# You can comment this out if you're not interested in stack traces.

-renamesourcefileattribute SourceFile
-keepattributes SourceFile,LineNumberTable

# RemoteViews might need annotations.

-keepattributes *Annotation*

# Preserve all fundamental application classes.

-keep public class * extends android.app.Activity
-keep public class * extends android.app.NativeActivity
-keep public class * extends android.app.Application
-keep public class * extends android.app.Service
-keep public class * extends android.content.BroadcastReceiver
-keep public class * extends android.content.ContentProvider

# Preserve all View implementations, their special context constructors, and
# their setters.

-keep public class * extends android.view.View {
    public <init>(android.content.Context);
    public <init>(android.content.Context, android.util.AttributeSet);
    public <init>(android.content.Context, android.util.AttributeSet, int);
    public void set*(...);
}

# Preserve all classes that have special context constructors, and the
# constructors themselves.

-keepclasseswithmembers class * {
    public <init>(android.content.Context, android.util.AttributeSet);
}

# Preserve all classes that have special context constructors, and the
# constructors themselves.

-keepclasseswithmembers class * {
    public <init>(android.content.Context, android.util.AttributeSet, int);
}

# Preserve all possible onClick handlers.

-keepclassmembers class * extends android.content.Context {
   public void *(android.view.View);
   public void *(android.view.MenuItem);
}

# Preserve the special fields of all Parcelable implementations.

-keepclassmembers class * implements android.os.Parcelable {
    static android.os.Parcelable$Creator CREATOR;
}

# Preserve static fields of inner classes of R classes that might be accessed
# through introspection.

-keepclassmembers class **.R$* {
  public static <fields>;
}

# Preserve annotated Javascript interface methods.

-keepclassmembers class * {
    @android.webkit.JavascriptInterface <methods>;
}

-keepclassmembers class androidx.core.app.NotificationCompatJellybean {
    ** icon;
    ** title;
    ** actionIntent;
}

-keepclassmembers class com.google.common.cache.Striped64 {
    ** base;
    ** busy;
}

-keepclassmembers class com.google.common.cache.Striped64$Cell {
    ** value;
}

-keepclassmembers class android.support.v4.media.session.** {
  ** mCallback;
}

# Make sure we keep Android resource fields.
-keepattributes InnerClasses
-keep class **.R
-keep class **.R$* {
    <fields>;
}

-keepclassmembers class androidx.core.app.NotificationManagerCompat {
  ** OP_POST_NOTIFICATION;
}

-keepclassmembers class com.google.android.gms.dynamite.DynamiteModule {
  ** sClassLoader;
  ** MODULE_VERSION;
  ** MODULE_ID;
}

-keepclassmembers class ** {
  ** SDK_INT;
}

# Preserve the required interface from the License Verification Library
# (but don't nag the developer if the library is not used at all).

-keep public interface com.android.vending.licensing.ILicensingService

-dontnote com.android.vending.licensing.ILicensingService

# The Android Compatibility library references some classes that may not be
# present in all versions of the API, but we know that's ok.

-dontwarn android.support.**

# DEFOLD

-keep public enum com.defold.**
-keep public enum com.dynamo.**

-keep public class com.defold.**
-keep public class com.dynamo.**

-dontwarn sun.misc.Unsafe
-dontwarn java.nio.**
-dontwarn com.squareup.javapoet.**
-dontwarn androidx.lifecycle.LifecycleProcessor
-dontwarn com.google.auto.common.BasicAnnotationProcessor
-dontwarn androidx.core.**
-dontwarn android.app.NotificationChannel
-dontwarn android.app.NotificationManager
-dontwarn com.google.android.gms.auth.GoogleAuthUtil
-dontwarn com.google.android.gms.**
-dontwarn com.google.protobuf.**

-dontwarn javax.lang.**
-dontwarn javax.annotation.**
-dontwarn javax.tools.**

-dontwarn com.amazon.android.**
-dontwarn com.amazon.device.iap.**
-dontwarn com.amazon.venezia.**


# Switch off some optimizations that trip older versions of the Dalvik VM.

-optimizations !code/simplification/arithmetic

# Android support (we've assembled it ourselves)
-dontwarn android.support.coreui.**

-keepattributes Signature


# To find what we're using
# $ cd <defold>/engine
# $ find . -iname "*.java" ! -ipath "*build/*.java" ! -ipath "*test*.java" -exec rg -N "import" {} \; | sort -u | sed s/import/-keep\ public\ class/ | sed s/\;//

-keep public class androidx.fragment.app.Fragment
-keep public class androidx.fragment.app.FragmentActivity
-keep public class androidx.fragment.app.FragmentManager
-keep public class androidx.core.app.NotificationCompat
-keep        class androidx.fragment.app.Fragment$SavedState
-keep        class androidx.fragment.app.Fragment$OnStartEnterTransitionListener
-keep public class androidx.fragment.app.FragmentTransaction
-keep public class androidx.core.app.SharedElementCallback
-keep public class androidx.core.app.ComponentActivity$ExtraData
-keep        class androidx.localbroadcastmanager.content.LocalBroadcastManager { *; }
-keep public class androidx.lifecycle.Lifecycle$State

# -keep public class com.amazon.device.iap.**
-keep public class com.google.firebase.**
-keep public class com.amazon.device.iap.PurchasingListener
-keep public class com.amazon.device.iap.PurchasingService
-keep public class com.amazon.device.iap.model.FulfillmentResult
-keep public class com.amazon.device.iap.model.Product
-keep public class com.amazon.device.iap.model.ProductDataResponse
-keep public class com.amazon.device.iap.model.PurchaseResponse
-keep public class com.amazon.device.iap.model.PurchaseUpdatesResponse
-keep public class com.amazon.device.iap.model.Receipt
-keep public class com.amazon.device.iap.model.RequestId
-keep public class com.amazon.device.iap.model.UserData
-keep public class com.amazon.device.iap.model.UserDataResponse
-keep public class com.android.vending.billing.IInAppBillingService
-keep public class com.google.android.gms.ads.identifier.AdvertisingIdClient
-keep public class com.google.android.gms.common.*
-keep public class com.google.android.gms.common.ConnectionResult
-keep public class com.google.android.gms.common.GooglePlayServicesUtil
-keep public class com.google.android.gms.common.GoogleApiAvailability
-keep public class com.google.android.gms.tasks.OnCompleteListener
-keep public class com.google.android.gms.tasks.Task
-keep public class com.google.firebase.FirebaseApp
-keep public class com.google.firebase.FirebaseOptions
-keep public class com.google.firebase.iid.FirebaseInstanceId
-keep public class com.google.firebase.iid.InstanceIdResult
-keep public class com.google.firebase.messaging.RemoteMessage
-keep public class com.google.firebase.components.**
-dontwarn com.google.firebase.components.**
-keep public class com.google.android.gms.gcm.GoogleCloudMessaging
-keep class com.google.android.gms.** { *; }

-dontwarn com.google.android.gms.**
-dontwarn com.google.protobuf.**

-dontwarn android.graphics.drawable.AdaptiveIconDrawable

-keep class com.dynamo.** {
    public <methods>;
}

-keep class com.defold.** {
    public <methods>;
}
