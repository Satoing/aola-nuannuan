package com.aola.dressup;

import org.libsdl.app.SDLActivity;

/**
 * 奥拉暖暖 Android 入口 Activity
 * 继承 SDLActivity，SDL2 会自动处理 OpenGL 上下文、输入事件等
 */
public class AolaDressUpActivity extends SDLActivity {

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
            "aola_dressup"  // 我们的游戏逻辑 native 库
        };
    }
}
