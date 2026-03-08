package com.aola.nuannuan;

import org.libsdl.app.SDLActivity;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import androidx.core.content.FileProvider;

import java.io.File;
import java.io.FileInputStream;
import java.io.OutputStream;

/**
 * 奥拉暖暖 Android 入口 Activity
 * 继承 SDLActivity，SDL2 会自动处理 OpenGL 上下文、输入事件等
 */
public class AolaNuanNuanActivity extends SDLActivity {

    private static final String TAG = "AolaNuanNuan";

    /**
     * 指定要加载的 native 库
     * SDL2 会自动加载 libSDL2.so，这里额外加载我们的游戏库
     */
    @Override
    protected String[] getLibraries() {
        return new String[]{
            "SDL2",
            "SDL2_image",
            "SDL2_ttf",
            "aola_nuannuan"  // 我们的游戏逻辑 native 库
        };
    }

    /**
     * 保存图片到系统相册（通过 MediaStore）
     * 从 native 层通过 JNI 调用
     */
    public static boolean saveToGallery(String pngPath) {
        try {
            Context context = getContext();
            if (context == null) {
                Log.e(TAG, "saveToGallery: context 为空");
                return false;
            }

            File file = new File(pngPath);
            if (!file.exists()) {
                Log.e(TAG, "saveToGallery: 文件不存在 " + pngPath);
                return false;
            }

            // 使用 MediaStore 保存（兼容 Android 10+）
            ContentValues values = new ContentValues();
            values.put(MediaStore.Images.Media.DISPLAY_NAME, file.getName());
            values.put(MediaStore.Images.Media.MIME_TYPE, "image/png");

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                values.put(MediaStore.Images.Media.RELATIVE_PATH, Environment.DIRECTORY_PICTURES + "/AolaNuanNuan");
                values.put(MediaStore.Images.Media.IS_PENDING, 1);
            }

            Uri uri = context.getContentResolver().insert(
                MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values);

            if (uri == null) {
                Log.e(TAG, "saveToGallery: MediaStore 插入失败");
                return false;
            }

            // 写入文件内容
            try (OutputStream os = context.getContentResolver().openOutputStream(uri);
                 FileInputStream fis = new FileInputStream(file)) {
                byte[] buffer = new byte[8192];
                int bytesRead;
                while ((bytesRead = fis.read(buffer)) != -1) {
                    os.write(buffer, 0, bytesRead);
                }
            }

            // 标记完成（Android 10+）
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                values.clear();
                values.put(MediaStore.Images.Media.IS_PENDING, 0);
                context.getContentResolver().update(uri, values, null, null);
            }

            Log.i(TAG, "saveToGallery: 成功保存到相册 " + uri);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "saveToGallery: 异常 " + e.getMessage(), e);
            return false;
        }
    }

    /**
     * 分享图片到社交平台（微信、QQ、小红书等）
     * 从 native 层通过 JNI 调用
     */
    public static void shareImage(String pngPath) {
        try {
            Context context = getContext();
            if (context == null) {
                Log.e(TAG, "shareImage: context 为空");
                return;
            }

            File file = new File(pngPath);
            if (!file.exists()) {
                Log.e(TAG, "shareImage: 文件不存在 " + pngPath);
                return;
            }

            // 使用 FileProvider 获取安全的 content:// URI
            Uri contentUri = FileProvider.getUriForFile(context,
                context.getPackageName() + ".fileprovider", file);

            Intent shareIntent = new Intent(Intent.ACTION_SEND);
            shareIntent.setType("image/png");
            shareIntent.putExtra(Intent.EXTRA_STREAM, contentUri);
            shareIntent.putExtra(Intent.EXTRA_TEXT, "看看我在奥拉暖暖的搭配！");
            shareIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            shareIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            Intent chooser = Intent.createChooser(shareIntent, "分享到...");
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(chooser);

            Log.i(TAG, "shareImage: 已打开分享选择器");
        } catch (Exception e) {
            Log.e(TAG, "shareImage: 异常 " + e.getMessage(), e);
        }
    }
}
