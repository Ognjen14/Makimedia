-keep class org.qtproject.qt.** { *; }
-keep interface org.qtproject.qt.** { *; }
-dontwarn org.qtproject.qt.**

-keep class com.topicdev.makimedia.org.** { *; }

-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}

-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile
