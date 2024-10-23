# Car Launcher

## How to start an app from a homecard

- Step 1: Add the following permission to the AndroidManifest.xml of the Launcher.
```xml
<uses-permission android:name="android.permission.QUERY_ALL_PACKAGES"/>
```
- Step 2: Add intent logic in the custom HomecardModel, see [DemoAppModel](src/com/android/car/carlauncher/homescreen/assistive/DemoAppModel.java).
- Step 3: Add intent-filters to the AndroidManifest.xml of the target app.
```xml
<intent-filter>
    <action android:name="android.intent.action.MAIN" />
    <category android:name="android.intent.category.DEFAULT" />
    <category android:name="android.intent.category.LAUNCHER" />
</intent-filter>
```