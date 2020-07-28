
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

-keepclassmembers class android.support.v4.app.NotificationCompatJellybean {
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

-keepclassmembers class android.support.v4.app.NotificationManagerCompat {
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
-dontwarn android.arch.lifecycle.LifecycleProcessor
-dontwarn com.google.auto.common.BasicAnnotationProcessor
-dontwarn android.support.v4.**
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
-keep class com.facebook.** {
   *;
}


# To find what we're using
# $ cd <defold>/engine
# $ find . -iname "*.java" ! -ipath "*build/*.java" ! -ipath "*test*.java" -exec rg -N "import" {} \; | sort -u | sed s/import/-keep\ public\ class/ | sed s/\;//

-keep public class android.support.v4.app.Fragment
-keep public class android.support.v4.app.FragmentActivity
-keep public class android.support.v4.app.FragmentManager
-keep public class android.support.v4.app.NotificationCompat
-keep        class android.support.v4.app.Fragment$SavedState
-keep        class android.support.v4.app.Fragment$OnStartEnterTransitionListener
-keep public class android.support.v4.app.FragmentTransaction
-keep public class android.support.v4.app.SharedElementCallback
-keep public class android.support.v4.app.SupportActivity$ExtraData
-keep        class android.support.v4.content.LocalBroadcastManager { *; }
-keep public class android.arch.lifecycle.Lifecycle$State

# -keep public class com.amazon.device.iap.**
-keep public class com.facebook.**

-keep public class android.Manifest
-keep public class android.R
-keep public class android.app.Activity
-keep public class android.app.AlarmManager
-keep public class android.app.NativeActivity
-keep public class android.app.Notification
-keep public class android.app.NotificationManager
-keep public class android.app.PendingIntent
-keep public class android.app.Service
-keep public class android.content.BroadcastReceiver
-keep public class android.content.ComponentName
-keep public class android.content.Context
-keep public class android.content.Intent
-keep public class android.content.IntentSender
-keep public class android.content.ServiceConnection
-keep public class android.content.SharedPreferences
-keep public class android.content.pm.ActivityInfo
-keep public class android.content.pm.ApplicationInfo
-keep public class android.content.pm.PackageInfo
-keep public class android.content.pm.PackageManager
-keep public class android.content.pm.ResolveInfo
-keep public class android.content.res.Configuration
-keep public class android.content.res.Resources
-keep public class android.graphics.Bitmap
-keep public class android.graphics.BitmapFactory
-keep public class android.media.AudioManager
-keep public class android.net.ConnectivityManager
-keep public class android.net.NetworkInfo
-keep public class android.net.ParseException
-keep public class android.net.Uri
-keep public class android.os.AsyncTask
-keep public class android.os.Build
-keep public class android.os.Bundle
-keep public class android.os.Handler
-keep public class android.os.IBinder
-keep public class android.os.Message
-keep public class android.os.Messenger
-keep public class android.os.Process
-keep public class android.os.RemoteException
-keep public class android.support.v4.app.FragmentActivity
-keep public class android.support.v4.app.NotificationCompat
-keep public class android.support.v4.content.ContextCompat
-keep public class android.support.v4.content.WakefulBroadcastReceiver
-keep public class android.telephony.PhoneStateListener
-keep public class android.telephony.TelephonyManager
-keep public class android.text.InputType
-keep public class android.util.Log
-keep public class android.view.Gravity
-keep public class android.view.KeyEvent
-keep public class android.view.MotionEvent
-keep public class android.view.View
-keep public class android.view.ViewGroup
-keep public class android.view.Window
-keep public class android.view.WindowManager
-keep public class android.view.inputmethod.CompletionInfo
-keep public class android.view.inputmethod.EditorInfo
-keep public class android.view.inputmethod.InputConnection
-keep public class android.view.inputmethod.InputConnectionWrapper
-keep public class android.view.inputmethod.InputMethodManager
-keep public class android.webkit.ConsoleMessage
-keep public class android.webkit.JavascriptInterface
-keep public class android.webkit.WebChromeClient
-keep public class android.webkit.WebSettings
-keep public class android.webkit.WebView
-keep public class android.webkit.WebViewClient
-keep public class android.widget.EditText
-keep public class android.widget.FrameLayout
-keep public class android.widget.LinearLayout
-keep public class android.widget.TextView
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
-keep public class com.facebook.AccessToken
-keep public class com.facebook.CallbackManager
-keep public class com.facebook.FacebookCallback
-keep public class com.facebook.FacebookException
-keep public class com.facebook.FacebookSdk
-keep public class com.facebook.GraphRequest
-keep public class com.facebook.GraphResponse
-keep public class com.facebook.HttpMethod
-keep public class com.facebook.appevents.AppEventsLogger
-keep public class com.facebook.login.DefaultAudience
-keep public class com.facebook.login.LoginManager
-keep public class com.facebook.login.LoginResult
-keep public class com.facebook.share.Sharer
-keep public class com.facebook.share.model.AppInviteContent
-keep public class com.facebook.share.model.GameRequestContent
-keep public class com.facebook.share.model.ShareLinkContent
-keep public class com.facebook.share.widget.AppInviteDialog
-keep public class com.facebook.share.widget.GameRequestDialog
-keep public class com.facebook.share.widget.ShareDialog
-keep public class bolts.**
-keep public class com.google.android.gms.ads.identifier.AdvertisingIdClient
-keep public class com.google.android.gms.common.*
-keep public class com.google.android.gms.common.ConnectionResult
-keep public class com.google.android.gms.common.GooglePlayServicesUtil
-keep public class com.google.android.gms.common.GoogleApiAvailability
-keep public class com.google.android.gms.tasks.OnCompleteListener
-keep public class com.google.android.gms.tasks.Task
-keep class com.google.android.gms.** { *; }
-dontwarn com.google.android.gms.**
-dontwarn com.google.protobuf.**
-keep public class java.io.BufferedReader
-keep public class java.io.File
-keep public class java.io.FileNotFoundException
-keep public class java.io.IOException
-keep public class java.io.InputStreamReader
-keep public class java.io.PrintStream
-keep public class java.lang.Boolean
-keep public class java.lang.Thread
-keep public class java.lang.reflect.Array
-keep public class java.net.DatagramPacket
-keep public class java.net.DatagramSocket
-keep public class java.net.Inet4Address
-keep public class java.net.InetAddress
-keep public class java.net.MulticastSocket
-keep public class java.net.NetworkInterface
-keep public class java.net.SocketException
-keep public class java.net.SocketTimeoutException
-keep public class java.net.URLEncoder
-keep public class java.net.UnknownHostException
-keep public class java.text.SimpleDateFormat
-keep public class java.util.ArrayList
-keep public class java.util.Arrays
-keep public class java.util.Collection
-keep public class java.util.Collections
-keep public class java.util.Date
-keep public class java.util.HashMap
-keep public class java.util.HashSet
-keep public class java.util.Iterator
-keep public class java.util.List
-keep public class java.util.Map
-keep public class java.util.Set
-keep public class java.util.StringTokenizer
-keep public class java.util.concurrent.*
-keep public class java.util.concurrent.ArrayBlockingQueue
-keep public class java.util.concurrent.BlockingQueue
-keep public class java.util.concurrent.CountDownLatch
-keep public class java.util.concurrent.atomic.AtomicInteger
-keep public class java.util.logging.Level
-keep public class java.util.logging.Logger
-keep public class java.util.regex.Matcher
-keep public class java.util.regex.Pattern
-keep public class org.json.JSONArray
-keep public class org.json.JSONException
-keep public class org.json.JSONObject

-dontwarn android.graphics.drawable.AdaptiveIconDrawable

-keep class com.dynamo.** {
    public <methods>;
}

-keep class com.defold.** {
    public <methods>;
}
